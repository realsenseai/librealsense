// License: Apache 2.0. Copyright (C) 2026 RealSense, Inc.
#include "contract/IntentProtocol.h"
#include "intent-json.h"
#include "intent-tlv.h"
#include <librealsense2/h/rs_composite_option.h>
#include <mutex>
#include <src/composite-option-interface.h>
#include <src/core/device-interface.h>
#include <src/ds/d500/d500-device.h>
#include <src/ds/ds-private.h>
#include <src/librealsense-exception.h>
#include <src/uvc-sensor.h>
namespace librealsense {
namespace {
class intent_option : public composite_option_interface {
    std::weak_ptr<uvc_sensor> _sensor;
    bool _status;
    uint16_t _sku;
    std::mutex _mutex;
    std::vector<uint8_t> _expected;

  public:
    intent_option(std::shared_ptr<uvc_sensor> sensor, bool status, uint16_t sku = 0)
        : _sensor(sensor), _status(status), _sku(sku) {}
    std::vector<uint8_t> get_raw() const override {
        auto sensor = _sensor.lock();
        if (!sensor)
            throw std::runtime_error("Intent: sensor disconnected");
        return sensor->invoke_powered([&](platform::uvc_device &dev) {
            std::vector<uint8_t> wire(_status ? rsintent::status_size : rsintent::wire_size);
            if (!dev.get_xu(ds::depth_xu,
                            _status ? rsintent::GLOBAL_INTENT_STATUS : rsintent::GLOBAL_INTENT,
                            wire.data(), static_cast<int>(wire.size())))
                throw std::runtime_error("Intent GET failed");
            return wire;
        });
    }
    void set_raw(const void *data, size_t size) override {
        if (_status)
            throw std::invalid_argument("Intent status is read-only");
        if (!data || size != rsintent::wire_size)
            throw std::invalid_argument("Intent transfer must contain 1024 bytes");
        auto sensor = _sensor.lock();
        if (!sensor)
            throw std::runtime_error("Intent: sensor disconnected");
        std::vector<uint8_t> payload(static_cast<const uint8_t *>(data), static_cast<const uint8_t *>(data) + size);
        std::vector<uint8_t> version(rsintent::status_size);
        sensor->invoke_powered([&](platform::uvc_device &dev) {
            if (!dev.get_xu(ds::depth_xu, rsintent::GLOBAL_INTENT_STATUS, version.data(), static_cast<int>(version.size())))
                throw std::runtime_error("Intent: status read failed before SET");
        });
        // Raw composite-option callers follow the same version policy as the JSON API.
        const auto document = decode_intent_json(payload, version);
        payload = encode_intent_json(document, version);
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
                bool supported = true;
                for (const auto &b : rsintent::branches)
                    if (b.id == model.streams[i].branch && b.since > version[1]) supported = false;
                for (const auto &s : rsintent::sensors)
                    if (s.id == model.streams[i].sensor && s.since > version[1]) supported = false;
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
            if (intent_tlv::pack(records) != intent_tlv::pack(intent_tlv::filter(records)))
                next = payload;
            next[0] = version[0];
            next[6] = version[1];
            next[7] = version[18];
        }
        std::lock_guard<std::mutex> lock(_mutex);
        sensor->invoke_powered([&](platform::uvc_device &dev) {
            if (!dev.set_xu(ds::depth_xu, rsintent::GLOBAL_INTENT,
                            payload.data(), static_cast<int>(payload.size())))
                throw std::runtime_error("Intent SET rejected");
        });
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
        std::vector<uint8_t> status(rsintent::status_size);
        sensor->invoke_powered([&](platform::uvc_device &dev) {
            if (!dev.get_xu(ds::depth_xu, rsintent::GLOBAL_INTENT_STATUS, status.data(),
                            static_cast<int>(status.size())))
                throw std::runtime_error("Intent: status read failed before start");
        });
        if (status[2] == 0U || get_raw() != _expected)
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
void register_intent_options(d500_depth_sensor &sensor) {
    auto raw = std::dynamic_pointer_cast<uvc_sensor>(sensor.get_raw_sensor());
    if (!raw)
        return;
    auto status = std::make_shared<intent_option>(raw, true);
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
                                     std::make_shared<intent_option>(raw, false, sku));
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
