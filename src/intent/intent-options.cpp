// License: Apache 2.0. Copyright (C) 2026 RealSense, Inc.
#include "contract/IntentProtocol.h"
#include "intent-json.h"
#include "intent-tlv.h"
#include <librealsense2/h/rs_composite_option.h>
#include <mutex>
#include <set>
#include <src/composite-option-interface.h>
#include <src/core/device-interface.h>
#include <src/ds/d500/d500-device.h>
#include <src/ds/ds-private.h>
#include <src/librealsense-exception.h>
#include <src/hw-monitor.h>
#include <rsutils/json.h>
#include <src/uvc-sensor.h>
namespace librealsense {
void declare_pipeline_intent(device_interface &device,
    const std::vector<std::shared_ptr<stream_profile_interface>> &profiles) {
    composite_option_interface *option = nullptr, *statusOption = nullptr;
    for (size_t i = 0; i < device.get_sensors_count(); ++i) {
        auto &sensor = device.get_sensor(i);
        if (sensor.supports_composite_option(RS2_COMPOSITE_OPTION_GLOBAL_INTENT)) {
            option = &sensor.get_composite_option(RS2_COMPOSITE_OPTION_GLOBAL_INTENT);
            statusOption = &sensor.get_composite_option(RS2_COMPOSITE_OPTION_GLOBAL_INTENT_STATUS);
            break;
        }
    }
    if (!option || !statusOption)
        throw invalid_value_exception("Global Intent is not supported by this device");
    const auto status = statusOption->get_raw();
    auto document = rsutils::json::parse(decode_intent_json(option->get_raw(), status));
    document.erase("layout"); document.erase("status");
    for (auto &sensor : document["sensors"]) {
        sensor.erase("layout");
        for (auto &stream : sensor["streams"]) {
            stream.erase("layout");
            const auto name = stream["stream"], index = stream["index"];
            stream = {{"stream", name}, {"index", index}, {"state", "DISABLED"}, {"required", false}};
        }
    }
    for (const auto &profile : profiles) {
        const std::string name = rs2_stream_to_string(profile->get_stream_type());
        bool matched = false;
        for (auto &sensor : document["sensors"])
            for (auto &stream : sensor["streams"]) {
                if (stream["stream"] != name || stream["index"] != profile->get_stream_index()) continue;
                stream.erase("state");
                stream["format"] = rs2_format_to_string(profile->get_format());
                stream["fps"] = profile->get_framerate();
                const auto video = dynamic_cast<const video_stream_profile_interface *>(profile.get());
                stream["width"] = video ? video->get_width() : 0;
                stream["height"] = video ? video->get_height() : 0;
                stream["required"] = true;
                matched = true;
            }
        if (!matched) throw invalid_value_exception("Resolved SDK stream/index is absent from FW Intent capabilities: " + name);
    }
    const auto wire = encode_intent_json(document.dump(), status);
    option->set_raw(wire.data(), wire.size());
}
namespace {
class intent_option : public composite_option_interface {
    std::weak_ptr<uvc_sensor> _sensor;
    std::shared_ptr<hw_monitor> _monitor;
    bool _status;
    uint16_t _sku;
    std::mutex _mutex;
    std::vector<uint8_t> _expected;

  public:
    intent_option(std::shared_ptr<uvc_sensor> sensor, std::shared_ptr<hw_monitor> monitor,
                  bool status, uint16_t sku = 0)
        : _sensor(sensor), _monitor(monitor), _status(status), _sku(sku) {}
    std::vector<uint8_t> read(uint8_t opcode, size_t expected, uint32_t selector = 0) const {
        auto wire = _monitor->send(command(opcode, selector));
        if (wire.size() != expected)
            throw std::runtime_error("Intent HWMC response has an invalid length");
        return wire;
    }
    std::vector<uint8_t> get_raw() const override {
        auto sensor = _sensor.lock();
        if (!sensor)
            throw std::runtime_error("Intent: sensor disconnected");
        return read(_status ? rsintent::HWMC_GET_INTENT_STATUS : rsintent::HWMC_GET_INTENT,
                    _status ? rsintent::status_size : rsintent::wire_size);
    }
    void set_raw(const void *data, size_t size) override {
        if (_status)
            throw std::invalid_argument("Intent status is read-only");
        if (!data || size != rsintent::wire_size)
            throw std::invalid_argument("Intent HWMC data must contain 1000 bytes");
        auto sensor = _sensor.lock();
        if (!sensor)
            throw std::runtime_error("Intent: sensor disconnected");
        std::vector<uint8_t> payload(static_cast<const uint8_t *>(data), static_cast<const uint8_t *>(data) + size);
        auto version = read(rsintent::HWMC_GET_INTENT_STATUS, rsintent::status_size);
        auto firmware = read(rsintent::HWMC_GET_INTENT, rsintent::wire_size);
        auto discovery = read(rsintent::HWMC_GET_INTENT, rsintent::wire_size, 2U);
        decode_intent_capabilities(discovery);
        // Raw composite-option callers follow the same version policy as the JSON API.
        const auto document = decode_intent_json(payload, version);
        payload = encode_intent_json(document, version, firmware, discovery);
        std::set<unsigned> reported;
        for (const auto &s : intent_tlv::body(firmware))
            if (s.type == rsintent::SENSOR)
                for (const auto &c : intent_tlv::parse(s.value))
                    if (c.type == rsintent::STREAM) reported.insert(intent_tlv::id(c));
        const auto records = intent_tlv::body(payload);
        const auto clean = intent_tlv::transfer(payload, intent_tlv::filter(records));
        rsintent::Request request = {};
        if (rsintent::decode(clean.data(), clean.size(), request).code != rsintent::OK)
            throw std::invalid_argument("Intent: malformed payload");
        // Allocate and normalize before SET so a successful device mutation is
        // followed only by a nonthrowing local state update.
        std::vector<uint8_t> next;
        if (request.op == rsintent::SET) {
            auto model = rsintent::defaultModel(_sku);
            size_t retained = 0;
            for (size_t i = 0; i < model.count; ++i) {
                bool supported = reported.count(model.streams[i].branch) != 0U;
                if (supported) model.streams[retained++] = model.streams[i];
            }
            model.count = retained;
            for (size_t i = 0; i < request.model.count; ++i)
                for (size_t j = 0; j < model.count; ++j)
                    if (request.model.streams[i].branch == model.streams[j].branch)
                        model.streams[j] = request.model.streams[i];
            next.resize(rsintent::wire_size);
            if (rsintent::encode(model, rsintent::SET, next.data(), next.size()).code !=
                rsintent::OK)
                throw std::invalid_argument("Intent: cannot normalize declaration");
        }
        std::lock_guard<std::mutex> lock(_mutex);
        command hwmc_request(rsintent::HWMC_SET_INTENT);
        hwmc_request.data = std::move(payload);
        _monitor->send(hwmc_request);
        if (request.op != rsintent::VALIDATE_ONLY)
            _expected.swap(next);
    }

    void verify() {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_expected.empty())
            return;
        auto sensor = _sensor.lock();
        if (!sensor)
            throw std::runtime_error("Intent: sensor disconnected");
        auto status = read(rsintent::HWMC_GET_INTENT_STATUS, rsintent::status_size);
        // An older SDK can compare known fields, not interpret newer opaque entries.
        if (status[2] == 0U || intent_tlv::canonical_known(get_raw()) != _expected)
            throw std::runtime_error(
                "Intent cleared or replaced; declare it again before starting");
    }
    std::vector<uint8_t> get_raw_range() const override {
        throw std::invalid_argument("Intent is a document, not a numeric range");
    }
    bool is_read_only() const override { return _status; }
    bool is_enabled() const override { return true; }
    const char *get_description() const override {
        return _status ? "Global Intent status (read-only)" : "Global streams Intent (1.0)";
    }
};
} // namespace
void register_intent_options(d500_depth_sensor &sensor, std::shared_ptr<hw_monitor> monitor) {
    auto raw = std::dynamic_pointer_cast<uvc_sensor>(sensor.get_raw_sensor());
    if (!raw)
        return;
    auto status = std::make_shared<intent_option>(raw, monitor, true);
    uint16_t sku = 0;
    try {
        const auto wire = status->get_raw();
        if (wire.size() != rsintent::status_size || wire[0] != rsintent::major)
            return;
        sku = rsintent::read16(wire.data() + 8);
        if (rsintent::defaultModel(sku).count == 0U)
            return;
    } catch (const std::exception &) {
        return;
    } // Unsupported firmware keeps legacy behavior.
    sensor.register_composite_option(RS2_COMPOSITE_OPTION_GLOBAL_INTENT,
                                     std::make_shared<intent_option>(raw, monitor, false, sku));
    sensor.register_composite_option(RS2_COMPOSITE_OPTION_GLOBAL_INTENT_STATUS, status);
}
} // namespace librealsense

namespace librealsense {
void verify_intent_before_start(device_interface &device) {
    for (size_t i = 0; i < device.get_sensors_count(); ++i) {
        auto &sensor = device.get_sensor(i);
        if (!sensor.supports_composite_option(RS2_COMPOSITE_OPTION_GLOBAL_INTENT))
            continue;
        auto *option = dynamic_cast<intent_option *>(
            &sensor.get_composite_option(RS2_COMPOSITE_OPTION_GLOBAL_INTENT));
        if (option)
            option->verify();
    }
}
std::string intent_rejection(device_interface &device) noexcept {
    try {
        for (size_t i = 0; i < device.get_sensors_count(); ++i) {
            auto &sensor = device.get_sensor(i);
            if (!sensor.supports_composite_option(RS2_COMPOSITE_OPTION_GLOBAL_INTENT_STATUS))
                continue;
            const auto status =
                sensor.get_composite_option(RS2_COMPOSITE_OPTION_GLOBAL_INTENT_STATUS).get_raw();
            // INVALID/INCOMPLETE come only from SET, so they cannot explain a start failure.
            if (status.size() != rsintent::status_size ||
                (status[3] != rsintent::CONSTRAINT && status[3] != rsintent::STREAM_MISMATCH &&
                 status[3] != rsintent::CONTROL_STATE))
                return std::string();
            // STATUS is device-wide and last-writer-wins (AR-4).
            return " (Intent " + describe_intent_status(status) +
                   "; diagnostic may be stale or from another client)";
        }
    } catch (...) {
    }
    return std::string();
}
void rethrow_with_intent_rejection(device_interface &device) {
    const auto reasons = intent_rejection(device);
    try {
        throw;
    } catch (const backend_exception &e) {
        if (reasons.empty())
            throw;
        // Keep the backend exception type so callers see the same rs2 error class.
        throw backend_exception(e.what() + reasons);
    }
}
} // namespace librealsense
