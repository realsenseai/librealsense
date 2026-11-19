// License: Apache 2.0. Copyright (C) 2026 RealSense, Inc.
#include "contract/IntentStatus.h"
#include "intent-json.h"
#include "intent-tlv.h"
#include <mutex>
#include <set>
#include <sstream>
#include <src/core/device-interface.h>
#include <src/core/extension.h>
#include <src/ds/d500/d500-device.h>
#include <src/ds/ds-private.h>
#include <src/librealsense-exception.h>
#include <rsutils/json.h>
#include <src/uvc-sensor.h>
namespace librealsense {
class intent_transport {
    std::weak_ptr<uvc_sensor> _sensor;
    uint16_t _sku;
    std::mutex _mutex;
    std::vector<uint8_t> _expected;
    bool _pipeline_owned = false;

  public:
    explicit intent_transport(std::shared_ptr<uvc_sensor> sensor) : _sensor(sensor), _sku(0) {}
    std::vector<uint8_t> read(uint8_t selector, size_t expected) const {
        auto sensor = _sensor.lock();
        if (!sensor) throw std::runtime_error("Intent: sensor disconnected");
        return sensor->invoke_powered([&](platform::uvc_device &device) {
            std::vector<uint8_t> wire(expected);
            if (!device.get_xu(ds::depth_xu, selector, wire.data(), static_cast<int>(wire.size())))
                throw invalid_value_exception("Intent XU GET_CUR failed");
            return wire;
        });
    }
    std::vector<uint8_t> get_raw() const { return read(rsintent::XU_INTENT, rsintent::wire_size); }
    std::vector<uint8_t> status() const { return read(rsintent::XU_STATUS, rsintent::status_size); }

    std::vector<uint8_t> discovery(const std::vector<uint8_t> &version) const {
        const size_t length = rsintent::read16(version.data() + rsintent::status_discovery_length_offset);
        if (!rsintent::supportedStatusLayout(version[rsintent::status_format_version_offset]) ||
            length < 16U || length > rsintent::status_discovery_capacity)
            throw invalid_value_exception("Intent STATUS has invalid discovery section");
        const auto start = version.begin() + rsintent::status_discovery_offset;
        std::vector<uint8_t> report(start, start + rsintent::status_discovery_capacity);
        if (rsintent::read16(report.data() + 6) != length ||
            (report[0] != version[rsintent::status_major_offset] ||
                 report[1] != version[rsintent::status_minor_offset] ||
                 report[2] != version[rsintent::status_patch_offset]) ||
            rsintent::read16(report.data() + 4) != rsintent::read16(version.data() + rsintent::status_sku_offset))
            throw invalid_value_exception("Intent STATUS and discovery header disagree");
        report.resize(rsintent::wire_size);
        return report;
    }

    bool probe() {
        const auto version = status();
        _sku = rsintent::read16(version.data() + rsintent::status_sku_offset);
        decode_intent_capabilities(discovery(version));
        return rsintent::defaultModel(_sku).count != 0U;
    }

    void write(const std::vector<uint8_t> &payload) const {
        auto sensor = _sensor.lock();
        if (!sensor)
            throw std::runtime_error("Intent: sensor disconnected");
        sensor->invoke_powered([&](platform::uvc_device &device) {
            if (!device.set_xu(ds::depth_xu, rsintent::XU_INTENT, payload.data(),
                               static_cast<int>(payload.size())))
                throw invalid_value_exception("Intent XU SET_CUR failed");
        });
    }

    void set(const std::string &document, const std::vector<uint8_t> &version,
             const std::vector<uint8_t> &firmware, bool pipeline_owned = false) {
        const auto payload = encode_intent_json(document, version, firmware, discovery(version));
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
            for (size_t i = 0; i < model.controlCount; ++i)
                if (const auto * control = rsintent::findControl(request.model, model.controls[i].sensor,
                                                                model.controls[i].kind, model.controls[i].id))
                    model.controls[i] = *control;
            next.resize(rsintent::wire_size);
            if (rsintent::encode(model, rsintent::SET, next.data(), next.size()).code != rsintent::OK)
                throw std::invalid_argument("Intent: cannot normalize declaration");
        }
        std::lock_guard<std::mutex> lock(_mutex);
        try {
            write(payload);
        } catch (const std::exception &error) {
            std::string message = error.what();
            try {
                message += " (device " + describe_intent_status(status(), &request.model) +
                           "; diagnostic may be from another client)";
            } catch (...) {}
            throw invalid_value_exception(message);
        }
        if (request.op != rsintent::VALIDATE_ONLY) {
            _expected.swap(next);
            _pipeline_owned = pipeline_owned;
        }
    }

    void clear_pipeline() {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_pipeline_owned)
            return;
        _pipeline_owned = false;
        auto expected = std::move(_expected);
        _expected.clear();
        const auto current = status();
        // Skip an active, cleared or replaced declaration. Exact readback also
        // protects unknown entries. The protocol provides no cross-client CAS.
        if (current[rsintent::status_state_offset] != rsintent::CONFIGURED ||
            rsintent::read32(current.data() + rsintent::status_active_mask_offset) != 0U ||
            get_raw() != expected)
            return;
        const auto payload = encode_intent_json(R"({"op":"CLEAR"})", current);
        write(payload);
    }

    std::vector<uint8_t> configured() {
        std::lock_guard<std::mutex> lock(_mutex);
        const auto current = status();
        if (current[rsintent::status_state_offset] == rsintent::UNSET) {
            if (!_expected.empty())
                throw invalid_value_exception("Intent cleared; declare it again before starting");
            return {};
        }
        auto wire = get_raw();
        if (!_expected.empty() && intent_tlv::canonical_known(wire) != _expected)
            throw invalid_value_exception("Intent replaced; declare it again before starting");
        return wire;
    }
};

std::shared_ptr<intent_transport> probe_intent_transport(d500_depth_sensor &sensor) {
    const auto raw = std::dynamic_pointer_cast<uvc_sensor>(sensor.get_raw_sensor());
    if (!raw)
        return {};
    auto intent = std::make_shared<intent_transport>(raw);
    try {
        if (intent->probe()) return intent;
    } catch (const std::exception &) {}
    return {};
}

namespace {
intent_transport * find_intent(device_interface &device) {
    void * ptr = nullptr;
    auto * extensions = dynamic_cast<extendable_interface *>(&device);
    return extensions && extensions->extend_to(RS2_EXTENSION_INTENT_DEVICE, &ptr)
        ? static_cast<intent_transport *>(ptr) : nullptr;
}

intent_transport & require_intent(device_interface &device) {
    if (auto * intent = find_intent(device)) return *intent;
    throw invalid_value_exception("Global Intent is not supported by this device");
}

std::string profile_name(const stream_profile_interface &profile) {
    std::ostringstream text;
    text << "stream=" << rs2_stream_to_string(profile.get_stream_type())
         << " index=" << profile.get_stream_index()
         << " format=" << rs2_format_to_string(profile.get_format());
    return text.str();
}
} // namespace

std::string get_device_intent(device_interface &device) {
    auto &intent = require_intent(device);
    const auto wire = intent.get_raw();
    return decode_intent_json(wire, intent.status());
}

void set_device_intent(device_interface &device, const std::string &document) {
    auto &intent = require_intent(device);
    const auto status = intent.status();
    intent.set(document, status, intent.get_raw());
}

void declare_pipeline_intent(device_interface &device,
    const std::vector<std::shared_ptr<stream_profile_interface>> &profiles) {
    auto &intent = require_intent(device);
    const auto status = intent.status();
    const auto firmware = intent.get_raw();
    const auto discovery = intent.discovery(status);
    const auto report = rsutils::json::parse(decode_intent_capabilities(discovery));
    auto document = rsutils::json::parse(decode_intent_json(firmware, status));
    const auto actual = document.at("status").value("actual_controls", rsutils::json::array());
    document.erase("layout");
    document.erase("status");
    const auto require_ids = [&](const char *key, std::initializer_list<uint8_t> required) {
        const auto &ids = report.at(key);
        for (const auto id : required)
            if (std::find(ids.begin(), ids.end(), id) == ids.end())
                throw invalid_value_exception("Global Intent is missing required grammar ID " + std::to_string(id));
    };
    require_ids("supported_record_ids", {rsintent::SENSOR, rsintent::SENSOR_ID, rsintent::STREAM});
    require_ids("supported_stream_attribute_ids", {rsintent::BRANCH_ID, rsintent::STATE,
        rsintent::FORMAT, rsintent::WIDTH, rsintent::HEIGHT, rsintent::FPS});
    const auto sku = rsintent::read16(status.data() + rsintent::status_sku_offset);
    for (auto &sensor : document["sensors"]) {
        sensor.erase("layout");
        if (sensor.contains("controls"))
            require_ids("supported_record_ids", {rsintent::CONTROL});
        unsigned sensor_id = rsintent::max_sensor_id + 1U;
        for (const auto &entry : rsintent::sensors)
            if (sensor["sensor"] == entry.sdk_name)
                sensor_id = entry.id;
        for (auto &control : sensor["controls"]) {
            control["required_state"] = "OFF";
            if (actual.empty())
                continue;
            const auto state = std::find_if(actual.begin(), actual.end(), [&](const auto &entry) {
                return entry["sensor"] == sensor_id && entry["id"] == control["id"];
            });
            if (state == actual.end())
                throw invalid_value_exception("Global Intent Status lacks a mandatory control state");
            control["required_state"] = state->at("state");
        }
        if (sensor["controls"].is_null()) sensor.erase("controls");
        for (auto &stream : sensor["streams"]) {
            const auto name = stream["stream"], index = stream["index"];
            stream = {{"stream", name}, {"index", index}, {"state", "DISABLED"}};
        }
    }
    for (const auto &profile : profiles) {
        const std::string name = rs2_stream_to_string(profile->get_stream_type());
        const std::string format = rs2_format_to_string(profile->get_format());
        const auto index = profile->get_stream_index();
        bool available = false;
        for (const auto &branch : rsintent::branches) {
            if (branch.sku != sku || branch.sdk_name != name || branch.sdk_index != index)
                continue;
            for (const auto &entry : report["branches"])
                if (entry["sensor"] == branch.sensor && entry["id"] == branch.id)
                    for (const auto &value : entry["formats"])
                        available |= intent_format_name(value.get<uint32_t>()) == format;
        }
        if (!available)
            throw invalid_value_exception("Global Intent unavailable for " + profile_name(*profile));
        bool matched = false;
        for (auto &sensor : document["sensors"])
            for (auto &stream : sensor["streams"]) {
                if (stream["stream"] != name || stream["index"] != index) continue;
                stream.erase("state");
                stream["format"] = format;
                stream["fps"] = profile->get_framerate();
                const auto video = dynamic_cast<const video_stream_profile_interface *>(profile.get());
                stream["width"] = video ? video->get_width() : 0;
                stream["height"] = video ? video->get_height() : 0;
                matched = true;
            }
        if (!matched)
            throw invalid_value_exception("Global Intent template missing " + profile_name(*profile));
    }
    intent.set(document.dump(), status, firmware, true);
}

void clear_pipeline_intent(device_interface &device) noexcept {
    try {
        if (auto * intent = find_intent(device))
            intent->clear_pipeline();
    } catch (...) {}
}

void verify_intent_before_start(device_interface &device) {
    if (auto * intent = find_intent(device))
        intent->configured();
}

void verify_intent_outputs(device_interface &device,
    const std::vector<std::shared_ptr<stream_profile_interface>> &profiles) {
    auto * intent = find_intent(device);
    if (!intent) return;
    const auto wire = intent->configured();
    if (wire.empty()) return;
    const auto document = rsutils::json::parse(decode_intent_json(wire, intent->status()));
    for (const auto &profile : profiles) {
        const std::string name = rs2_stream_to_string(profile->get_stream_type());
        const auto *video = dynamic_cast<const video_stream_profile_interface *>(profile.get());
        const auto mismatch = intent_output_mismatch(document, name, profile->get_stream_index(),
            rs2_format_to_string(profile->get_format()), video ? video->get_width() : 0,
            video ? video->get_height() : 0, profile->get_framerate());
        if (!mismatch.empty())
            throw invalid_value_exception("Global Intent STREAM_MISMATCH " + profile_name(*profile)
                                          + " mismatch=" + mismatch);
    }
}

std::string intent_rejection(device_interface &device) noexcept {
    try {
        if (auto * intent = find_intent(device)) {
            const auto status = intent->status();
            const auto result = status[rsintent::status_result_offset];
            if (result == rsintent::CONSTRAINT || result == rsintent::STREAM_MISMATCH ||
                result == rsintent::CONTROL_STATE || result == rsintent::ADMISSION_REJECT)
                return " (Intent " + describe_intent_status(status) +
                       "; diagnostic may be stale or from another client)";
        }
    } catch (...) {}
    return {};
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
