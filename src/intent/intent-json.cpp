// License: Apache 2.0. Copyright (C) 2026 RealSense, Inc.
#include "intent-json.h"
#include "contract/IntentProtocol.h"
#include <rsutils/json.h>
#include <set>
#include <stdexcept>
namespace librealsense {
namespace {
using rsutils::json;
unsigned integer(const json &j, const char *name, unsigned maximum) {
    const auto &v = j.at(name);
    if (!v.is_number_integer() || v.get<int64_t>() < 0 || v.get<uint64_t>() > maximum)
        throw std::invalid_argument(std::string("Intent: invalid ") + name);
    return v.get<unsigned>();
}
void keys(const json &j, std::initializer_list<const char *> allowed) {
    if (!j.is_object())
        throw std::invalid_argument("Intent: expected object");
    for (auto i = j.begin(); i != j.end(); ++i) {
        bool known = false;
        for (const char *name : allowed)
            known |= i.key() == name;
        if (!known)
            throw std::invalid_argument("Intent: unknown key " + i.key());
    }
}
const rsintent::BranchDefinition &definition(const json &stream) {
    const auto name = stream.at("branch").get<std::string>();
    for (const auto &b : rsintent::branches)
        if (name == b.name)
            return b;
    throw std::invalid_argument("Intent: unknown branch " + name);
}
unsigned sensorId(const std::string &name) {
    for (const auto &s : rsintent::sensors)
        if (name == s.name)
            return s.id;
    return 0U;
}
} // namespace
std::vector<uint8_t> encode_intent_json(const std::string &text) {
    const json j = json::parse(text);
    keys(j, {"version", "op", "sensors", "status"});
    const std::string operation = j.value("op", std::string("SET"));
    rsintent::Operation op = rsintent::SET;
    if (operation == "CLEAR")
        op = rsintent::CLEAR;
    else if (operation == "VALIDATE_ONLY")
        op = rsintent::VALIDATE_ONLY;
    else if (operation != "SET")
        throw std::invalid_argument("Intent: unknown operation");
    uint8_t minor = 0;
    if (j.contains("version")) {
        const auto &v = j.at("version");
        keys(v, {"major", "minor"});
        if (integer(v, "major", 255U) != rsintent::major)
            throw std::invalid_argument("Intent: unsupported major version");
        minor = static_cast<uint8_t>(integer(v, "minor", 255U));
    } else if (op != rsintent::CLEAR)
        throw std::invalid_argument("Intent: missing version");
    rsintent::Model model = {};
    std::set<unsigned> seen, sensors;
    if (op != rsintent::CLEAR) {
        const auto &list = j.at("sensors");
        if (!list.is_array())
            throw std::invalid_argument("Intent: sensors must be an array");
        for (const auto &sensor : list) {
            keys(sensor, {"sensor", "streams"});
            const auto name = sensor.at("sensor").get<std::string>();
            const unsigned id = sensorId(name);
            if (!id || !sensors.insert(id).second)
                throw std::invalid_argument("Intent: invalid or duplicate sensor");
            const auto &streams = sensor.at("streams");
            if (!streams.is_array())
                throw std::invalid_argument("Intent: streams must be an array");
            for (const auto &entry : streams) {
                keys(entry, {"branch", "state", "format", "width", "height", "fps"});
                const auto &def = definition(entry);
                if (def.sensor != id || !seen.insert(def.id).second ||
                    model.count == rsintent::max_streams)
                    throw std::invalid_argument("Intent: invalid or duplicate branch");
                rsintent::Stream s = {};
                s.sensor = def.sensor;
                s.branch = def.id;
                s.enabled = !entry.contains("state");
                if (!s.enabled) {
                    if (entry.at("state") != "DISABLED" || entry.size() != 2U)
                        throw std::invalid_argument(
                            "Intent: disabled stream has profile attributes");
                } else {
                    std::string format = entry.at("format").get<std::string>();
                    if (format.empty() || format.size() > 4U)
                        throw std::invalid_argument("Intent: format must be a FourCC");
                    format.resize(4U, ' ');
                    s.format = rsintent::read32(reinterpret_cast<const uint8_t *>(format.data()));
                    s.width = static_cast<uint16_t>(integer(entry, "width", 65535U));
                    s.height = static_cast<uint16_t>(integer(entry, "height", 65535U));
                    s.fps = static_cast<uint16_t>(integer(entry, "fps", 65535U));
                    if (!s.width || !s.height || !s.fps)
                        throw std::invalid_argument("Intent: zero image profile field");
                }
                model.streams[model.count++] = s;
            }
        }
    } else if (j.contains("sensors"))
        throw std::invalid_argument("Intent: CLEAR must not carry sensors");
    std::vector<uint8_t> wire(rsintent::wire_size);
    if (rsintent::encode(model, op, wire.data(), wire.size()).code != rsintent::OK)
        throw std::invalid_argument("Intent: encoding failed");
    wire[6] = minor;
    return wire;
}
std::string decode_intent_json(const std::vector<uint8_t> &wire,
                               const std::vector<uint8_t> &status) {
    rsintent::Request request = {};
    if (rsintent::decode(wire.data(), wire.size(), request).code != rsintent::OK ||
        status.size() != rsintent::status_size || status[0] != rsintent::major)
        throw std::invalid_argument("Intent: invalid firmware response");
    if (wire[6] > rsintent::minor)
        throw std::invalid_argument(
            "Intent: firmware minor requires a newer SDK for lossless editing");
    json j = {{"version", {{"major", wire[0]}, {"minor", wire[6]}}},
              {"op", "SET"},
              {"sensors", json::array()}};
    size_t emitted = 0;
    for (const auto &definition : rsintent::sensors) {
        const unsigned id = definition.id;
        json sensor = {{"sensor", definition.name}, {"streams", json::array()}};
        for (size_t i = 0; i < request.model.count; ++i) {
            const auto &s = request.model.streams[i];
            if (s.sensor != id)
                continue;
            const char *name = nullptr;
            for (const auto &b : rsintent::branches)
                if (b.id == s.branch)
                    name = b.name;
            if (!name)
                throw std::invalid_argument("Intent: firmware branch requires a newer SDK");
            json stream = {{"branch", name}};
            if (!s.enabled)
                stream["state"] = "DISABLED";
            else {
                uint8_t bytes[4];
                rsintent::write32(bytes, s.format);
                std::string format(reinterpret_cast<char *>(bytes), 4U);
                while (!format.empty() && format.back() == ' ')
                    format.pop_back();
                stream["format"] = format;
                stream["width"] = s.width;
                stream["height"] = s.height;
                stream["fps"] = s.fps;
            }
            sensor["streams"].push_back(stream);
            ++emitted;
        }
        if (!sensor["streams"].empty())
            j["sensors"].push_back(sensor);
    }
    if (emitted != request.model.count)
        throw std::invalid_argument("Intent: firmware sensor requires a newer SDK");
    j["status"] = {{"state", status[2]},
                   {"last_result", status[3]},
                   {"error_sensor", status[10]},
                   {"error_branch", status[11]},
                   {"error_offset", rsintent::read16(status.data() + 12)},
                   {"error_rule", rsintent::read16(status.data() + 14)},
                   {"error_detail", rsintent::read16(status.data() + 16)},
                   {"source", status[5] ? "DEFAULT" : "CURRENT"},
                   {"active_mask", rsintent::read16(status.data() + 6)},
                   {"sku", rsintent::read16(status.data() + 8)},
                   {"rule_revision", rsintent::read32(status.data() + 20)}};
    return j.dump(2);
}
} // namespace librealsense
