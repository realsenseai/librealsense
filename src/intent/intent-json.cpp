// License: Apache 2.0. Copyright (C) 2026 RealSense, Inc.
#include "intent-json.h"
#include "intent-tlv.h"
#include "contract/IntentProtocol.h"
#include "contract/IntentStatus.h"
#include "contract/rs_intent_tlv.h"
#include <rsutils/json.h>
#include <rsutils/easylogging/easyloggingpp.h>
#include <set>
#include <sstream>
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
void version(const json &value, uint8_t &minor, uint8_t &patch) {
    if (!value.is_string())
        throw std::invalid_argument("Intent: version must be major.minor.patch");
    const auto text = value.get<std::string>();
    unsigned components[3] = {};
    size_t pos = 0;
    for (unsigned index = 0; index < 3U; ++index) {
        unsigned &component = components[index];
        const size_t start = pos;
        while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
            component = component * 10U + static_cast<unsigned>(text[pos++] - '0');
            if (component > 255U)
                throw std::invalid_argument("Intent: version component exceeds 255");
        }
        if (pos == start || (pos - start > 1U && text[start] == '0') ||
            (index < 2U && (pos == text.size() || text[pos++] != '.')))
            throw std::invalid_argument("Intent: version must be canonical major.minor.patch");
    }
    if (pos != text.size())
        throw std::invalid_argument("Intent: version must be canonical major.minor.patch");
    if (components[0] != rsintent::major)
        throw std::invalid_argument("Intent: unsupported major version");
    minor = static_cast<uint8_t>(components[1]);
    patch = static_cast<uint8_t>(components[2]);
}
const rsintent::BranchDefinition &definition(const json &stream) {
    if (stream.contains("stream")) {
        const auto name = stream.at("stream").get<std::string>();
        const unsigned index = integer(stream, "index", 255);
        for (const auto &b : rsintent::branches)
            if (name == b.sdk_name && index == b.sdk_index) return b;
        throw std::invalid_argument("Intent: unknown SDK stream/index " + name);
    }
    const auto name = stream.at("branch").get<std::string>();
    for (const auto &b : rsintent::branches) if (name == b.name) return b;
    throw std::invalid_argument("Intent: unknown branch " + name);
}
uint32_t formatId(const std::string &name) {
    for (const auto &f : rsintent::sdk_formats)
        if (name == f.name) return f.format;
    if (name.size() <= 4U && !name.empty()) {
        auto text = name; text.resize(4U, ' ');
        const uint32_t value = rsintent::read32(reinterpret_cast<const uint8_t *>(text.data()));
        for (const auto &f : rsintent::sdk_formats) if (f.format == value) return value;
    }
    throw std::invalid_argument("Intent: unknown SDK format " + name);
}
std::string formatName(uint32_t value) {
    for (const auto &f : rsintent::sdk_formats) if (f.format == value) return f.name;
    throw std::invalid_argument("Intent: unknown semantic format");
}
unsigned sensorId(const std::string &name) {
    for (const auto &s : rsintent::sensors)
        if (name == s.sdk_name || name == s.name)
            return s.id;
    return 0U;
}
const json &childDocument(const json &j, const intent_tlv::Record &r, unsigned level) {
    const unsigned id = intent_tlv::id(r);
    if (level == 0) {
        for (const auto &sensor : j.at("sensors"))
            if (sensorId(sensor.at("sensor").get<std::string>()) == id) return sensor;
    } else {
        for (const auto &stream : j.at("streams"))
            if (definition(stream).id == id) return stream;
    }
    throw std::invalid_argument("Intent: layout references a missing declaration");
}
// Layout retains original order and flags; hex values are opaque, not FW-validated.
std::vector<intent_tlv::Record> applyLayouts(const json &j, std::vector<intent_tlv::Record> records,
                                           unsigned level) {
    using namespace intent_tlv;
    for (auto &r : records)
        if ((level == 0 && r.type == rsintent::SENSOR) || (level == 1 && r.type == rsintent::STREAM))
            r.value = pack(applyLayouts(childDocument(j, r, level), parse(r.value), level + 1));
    if (!j.contains("layout")) return records;
    const auto &layout = j.at("layout");
    require(layout.is_array() && layout.size() <= rsintent::max_tlv_size / 4);
    std::vector<Record> result;
    std::set<size_t> consumed;
    for (const auto &entry : layout) {
        keys(entry, {"type", "flags", "id", "hex"});
        const auto type = static_cast<uint8_t>(integer(entry, "type", 255));
        const auto flags = static_cast<uint8_t>(integer(entry, "flags", 3));
        if (entry.contains("hex")) {
            const bool container = type == rsintent::SENSOR || type == rsintent::STREAM;
            require(entry.contains("id") == container);
            const auto text = entry.at("hex").get<std::string>();
            require(text.size() % 2 == 0 && text.size() <= 2 * rsintent::max_tlv_size);
            Record r = {type, flags, {}};
            unsigned value = 0;
            for (size_t n = 0; n < text.size(); ++n) {
                const char c = text[n];
                require((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
                value = (value << 4) | static_cast<unsigned>(c <= '9' ? c - '0' : (c <= 'F' ? c - 'A' + 10 : c - 'a' + 10));
                if (n % 2) { r.value.push_back(static_cast<uint8_t>(value)); value = 0; }
            }
            require(!recognized(r));
            if (container) require(integer(entry, "id", 255) == id(r));
            require((type != rsintent::SENSOR || level == 0) && (type != rsintent::STREAM || level == 1));
            result.push_back(r);
        } else {
            const bool container = type == rsintent::SENSOR || type == rsintent::STREAM;
            require(entry.contains("id") == container);
            const unsigned idValue = container ? integer(entry, "id", 255) : 0;
            size_t match = records.size();
            for (size_t n = 0; n < records.size(); ++n)
                if (records[n].type == type && (!container || id(records[n]) == idValue)) match = n;
            require(match < records.size() && consumed.insert(match).second);
            Record r = records[match];
            require((flags & 2U) == (r.flags & 2U));
            require(!(r.flags & 1U) || (flags & 1U));
            r.flags = flags;
            result.push_back(r);
        }
    }
    require(consumed.size() == records.size());
    // SENSOR_ID is the first child even when opaque extensions are present.
    if (level == 1) require(!result.empty() && result.front().type == rsintent::SENSOR_ID);
    return result;
}
void warn_unsupported(const std::vector<intent_tlv::Record> &records,
                      unsigned firmwareMinor, unsigned sourceMinor) {
    using namespace intent_tlv;
    for (const auto &r : records) {
        unsigned since = rsintent::recordSince(r.type);
        std::string name = "record type " + std::to_string(r.type);
        if (!recognized(r)) {
            // Opaque entries retain their source version; FW decides whether to accept them.
            since = sourceMinor;
            name = "opaque " + name;
            if (r.type == rsintent::SENSOR || r.type == rsintent::STREAM)
                name += " id " + std::to_string(id(r));
        } else if (r.type == rsintent::SENSOR || r.type == rsintent::STREAM) {
            const unsigned identifier = id(r);
            if (r.type == rsintent::SENSOR) {
                for (const auto &s : rsintent::sensors) if (s.id == identifier) {
                    since = std::max<unsigned>(since, s.since);
                    name = std::string("sensor ") + s.name;
                }
            } else {
                for (const auto &b : rsintent::branches) if (b.id == identifier) {
                    since = std::max<unsigned>(since, b.since);
                    name = std::string("branch ") + b.name;
                }
            }
            warn_unsupported(parse(r.value), firmwareMinor, sourceMinor);
        } else {
            switch (r.type) {
            case rsintent::SENSOR_ID: name = "SENSOR_ID"; break;
            case rsintent::BRANCH_ID: name = "BRANCH_ID"; break;
            case rsintent::FORMAT: name = "FORMAT"; break;
            case rsintent::WIDTH: name = "WIDTH"; break;
            case rsintent::HEIGHT: name = "HEIGHT"; break;
            case rsintent::FPS: name = "FPS"; break;
            case rsintent::STATE: name = "STATE"; break;
            default: break;
            }
        }
        if (since > firmwareMinor) {
            LOG_WARNING("Intent: forwarding " << name << " (since minor " << since
                        << "; firmware minor " << firmwareMinor << "); firmware decides acceptance");
        }
    }
}
void warn_version(const std::vector<uint8_t> &status) {
    if (!status.empty() && status[1] != rsintent::minor) {
        LOG_WARNING("Intent dictionary mismatch: SDK " << rsintent::major << "." << rsintent::minor
                    << "." << rsintent::patch << ", firmware " << unsigned(status[0]) << "."
                    << unsigned(status[1]) << "." << unsigned(status[18])
                    << "; continuing with known fields and opaque pass-through");
    }
}
std::string containerName(const intent_tlv::Record &r) {
    const unsigned identifier = intent_tlv::id(r);
    if (r.type == rsintent::SENSOR) {
        for (const auto &s : rsintent::sensors)
            if (s.id == identifier) return std::string("sensor ") + s.name;
        return "sensor id " + std::to_string(identifier);
    }
    for (const auto &b : rsintent::branches)
        if (b.id == identifier) return std::string("branch ") + b.name;
    return "branch id " + std::to_string(identifier);
}
// Light D reports capability mismatches; it must not silently change the request.
void warn_unreported(const std::vector<intent_tlv::Record> &records,
                     const std::vector<intent_tlv::Record> &firmware) {
    using namespace intent_tlv;
    for (const auto &r : records) {
        if (r.type != rsintent::SENSOR) continue;
        const Record *match = nullptr;
        for (const auto &f : firmware)
            if (f.type == rsintent::SENSOR && id(f) == id(r)) match = &f;
        if (!match) {
            const auto name = containerName(r);
            LOG_WARNING("Intent: forwarding " << name << " (not reported by firmware); firmware decides acceptance");
            continue;
        }
        std::set<unsigned> streams;
        for (const auto &c : parse(match->value))
            if (c.type == rsintent::STREAM) streams.insert(id(c));
        for (const auto &c : parse(r.value)) {
            if (c.type == rsintent::STREAM && !streams.count(id(c))) {
                const auto name = containerName(c);
                LOG_WARNING("Intent: forwarding " << name << " (not reported by firmware); firmware decides acceptance");
            }
        }
    }
}
void attachLayouts(json &j, const std::vector<intent_tlv::Record> &records, unsigned level) {
    j["layout"] = intent_tlv::layout(records);
    for (const auto &r : records) {
        if (!intent_tlv::recognized(r)) continue;
        if (level == 0 && r.type == rsintent::SENSOR) {
            for (auto &sensor : j["sensors"])
                if (sensorId(sensor.at("sensor").get<std::string>()) == intent_tlv::id(r))
                    attachLayouts(sensor, intent_tlv::parse(r.value), 1);
        } else if (level == 1 && r.type == rsintent::STREAM) {
            for (auto &stream : j["streams"])
                if (definition(stream).id == intent_tlv::id(r))
                    attachLayouts(stream, intent_tlv::parse(r.value), 2);
        }
    }
}
} // namespace
std::vector<uint8_t> encode_intent_json(const std::string &text, const std::vector<uint8_t> &status,
                                       const std::vector<uint8_t> &firmware,
                                       const std::vector<uint8_t> &discovery) {
    const json j = json::parse(text);
    if (!status.empty() && (status.size() != rsintent::status_size || status[0] != rsintent::major ||
                           status[3] > rsintent::READ_ONLY))
        throw std::invalid_argument("Intent: firmware major mismatch or invalid status");
    if (!firmware.empty() && (firmware.size() != rsintent::wire_size || firmware[0] != rsintent::major))
        throw std::invalid_argument("Intent: firmware major mismatch or invalid GET_CUR");
    warn_version(status);
    keys(j, {"version", "op", "sensors", "status", "layout"});
    const std::string operation = j.value("op", std::string("SET"));
    rsintent::Operation op = rsintent::SET;
    if (operation == "CLEAR")
        op = rsintent::CLEAR;
    else if (operation == "VALIDATE_ONLY")
        op = rsintent::VALIDATE_ONLY;
    else if (operation != "SET")
        throw std::invalid_argument("Intent: unknown operation");
    uint8_t minor = static_cast<uint8_t>(rsintent::minor);
    uint8_t patch = static_cast<uint8_t>(rsintent::patch);
    if (j.contains("version")) {
        version(j.at("version"), minor, patch);
    } else if (op != rsintent::CLEAR)
        throw std::invalid_argument("Intent: missing version");
    rsintent::Model model = {};
    std::set<unsigned> seen, sensors;
    if (op != rsintent::CLEAR) {
        const auto &list = j.at("sensors");
        if (!list.is_array())
            throw std::invalid_argument("Intent: sensors must be an array");
        for (const auto &sensor : list) {
            keys(sensor, {"sensor", "streams", "layout"});
            const auto name = sensor.at("sensor").get<std::string>();
            const unsigned id = sensorId(name);
            if (!id || !sensors.insert(id).second)
                throw std::invalid_argument("Intent: invalid or duplicate sensor");
            const auto &streams = sensor.at("streams");
            if (!streams.is_array())
                throw std::invalid_argument("Intent: streams must be an array");
            for (const auto &entry : streams) {
                keys(entry, {"stream", "index", "branch", "state", "format", "width", "height", "fps", "layout", "required"});
                if (entry.contains("stream") && entry.contains("branch"))
                    throw std::invalid_argument("Intent: use one stream identity form");
                const auto &def = definition(entry);
                if (def.sensor != id || !seen.insert(def.id).second ||
                    model.count == rsintent::max_streams)
                    throw std::invalid_argument("Intent: invalid or duplicate branch");
                rsintent::Stream s = {};
                s.sensor = def.sensor;
                s.branch = def.id;
                s.enabled = !entry.contains("state");
                if (entry.contains("required") && !entry.at("required").is_boolean())
                    throw std::invalid_argument("Intent: required must be boolean");
                s.required = entry.value("required", s.enabled);
                s.optional = s.enabled && !s.required;
                if (s.optional && rsintent::recordCritical(rsintent::STREAM))
                    throw std::invalid_argument("Intent: dictionary-required stream cannot be optional");
                if (!s.enabled) {
                    if (entry.at("state") != "DISABLED" || entry.contains("format") || entry.contains("width") ||
                        entry.contains("height") || entry.contains("fps"))
                        throw std::invalid_argument(
                            "Intent: disabled stream has profile attributes");
                } else {
                    s.format = formatId(entry.at("format").get<std::string>());
                    bool supported = false;
                    for (const auto &f : rsintent::formats)
                        supported |= f.branch == s.branch && f.format == s.format;
                    if (!supported) throw std::invalid_argument("Intent: format is not defined for this stream");
                    s.width = static_cast<uint16_t>(integer(entry, "width", 65535U));
                    s.height = static_cast<uint16_t>(integer(entry, "height", 65535U));
                    s.fps = static_cast<uint16_t>(integer(entry, "fps", 65535U));
                    const bool geometry = s.sensor == RS_INTENT_SENSOR_MOTION ? !s.width && !s.height : s.width && s.height;
                    if (!geometry || !s.fps)
                        throw std::invalid_argument("Intent: invalid geometry or frame/ODR rate");
                }
                model.streams[model.count++] = s;
            }
        }
    } else if (j.contains("sensors"))
        throw std::invalid_argument("Intent: CLEAR must not carry sensors");
    std::vector<uint8_t> wire(rsintent::wire_size);
    if (rsintent::encode(model, op, wire.data(), wire.size()).code != rsintent::OK)
        throw std::invalid_argument("Intent: encoding failed");
    wire[rsintent::envelope_minor_offset] = minor;
    wire[rsintent::envelope_patch_offset] = patch;
    if (op == rsintent::CLEAR && j.contains("layout"))
        throw std::invalid_argument("Intent: CLEAR must not carry a layout");
    if (op != rsintent::CLEAR) {
        auto records = intent_tlv::body(wire);
        for (const auto &sensor : j.at("sensors")) {
            const unsigned id = sensorId(sensor.at("sensor").get<std::string>());
            bool present = false;
            for (const auto &r : records) present |= intent_tlv::id(r) == id;
            if (!present) {
                intent_tlv::Record scalar = {rsintent::SENSOR_ID, rsintent::recordCritical(rsintent::SENSOR_ID),
                                            {static_cast<uint8_t>(id), 0, 0, 0}};
                records.push_back({rsintent::SENSOR, static_cast<uint8_t>(2U | rsintent::recordCritical(rsintent::SENSOR)),
                                   intent_tlv::pack({scalar})});
            }
        }
        std::sort(records.begin(), records.end(), [](const intent_tlv::Record &a, const intent_tlv::Record &b) {
            return intent_tlv::id(a) < intent_tlv::id(b);
        });
        records = applyLayouts(j, records, 0);
        intent_tlv::structure(records);
        if (!status.empty()) warn_unsupported(records, status[1], minor);
        if (!firmware.empty()) warn_unreported(records, intent_tlv::body(firmware));
        if (!discovery.empty()) {
            const auto report = json::parse(decode_intent_capabilities(discovery));
            const auto warn = [&](uint8_t type) {
                bool found = false;
                for (const auto &id : report["supported_record_ids"]) found |= id == type;
                if (!found) LOG_WARNING("Intent: FW does not report record/attribute " << unsigned(type)
                                        << "; request forwarded unchanged for FW validation");
            };
            for (const auto &sensor : records) {
                warn(sensor.type);
                if (sensor.type != rsintent::SENSOR) continue;
                for (const auto &stream : intent_tlv::parse(sensor.value)) {
                    warn(stream.type);
                    if (stream.type == rsintent::STREAM) {
                        const unsigned branch = intent_tlv::id(stream);
                        bool branchReported = false;
                        for (const auto &entry : report["branches"]) {
                            if (entry["sensor"] != intent_tlv::id(sensor) || entry["id"] != branch) continue;
                            branchReported = true;
                            for (const auto &attribute : intent_tlv::parse(stream.value)) {
                                if (attribute.type != rsintent::FORMAT || attribute.value.size() != 4U) continue;
                                const uint32_t format = rsintent::read32(attribute.value.data());
                                bool supported = false;
                                for (const auto &value : entry["formats"]) supported |= value == format;
                                if (!supported) LOG_WARNING("Intent: FW does not report format " << format
                                                            << " for branch " << branch << "; request forwarded unchanged");
                            }
                        }
                        if (!branchReported) LOG_WARNING("Intent: FW does not report branch " << branch
                                                        << "; request forwarded unchanged");
                        for (const auto &attribute : intent_tlv::parse(stream.value)) {
                            warn(attribute.type);
                            bool found = false;
                            for (const auto &id : report["supported_stream_attribute_ids"]) found |= id == attribute.type;
                            if (!found) LOG_WARNING("Intent: FW does not report stream attribute " << unsigned(attribute.type)
                                                    << "; request forwarded unchanged");
                        }
                    }
                }
            }
        }
        wire = intent_tlv::transfer(wire, records);
    }
    return wire;
}
std::string decode_intent_capabilities(const std::vector<uint8_t> &data) {
    if (data.size() != rsintent::wire_size || data[0] != rsintent::major || data[3] != 1U ||
        data[12] || data[13] || data[14] || data[15])
        throw std::invalid_argument("Intent: invalid capability report header");
    const size_t end = rsintent::read16(data.data() + 6);
    size_t pos = 16U;
    if (end < pos || end > data.size()) throw std::invalid_argument("Intent: truncated capability report");
    const auto need = [&](size_t size) {
        if (pos > end || size > end - pos) throw std::invalid_argument("Intent: truncated capability report");
    };
    json report = {{"sku", rsintent::read16(data.data() + 4)},
                   {"version", std::to_string(data[0]) + "." + std::to_string(data[1]) + "." + std::to_string(data[2])},
                   {"supported_record_ids", json::array()}, {"supported_stream_attribute_ids", json::array()},
                   {"sensors", json::array()}, {"branches", json::array()}};
    for (const auto &field : {std::make_pair(8U, "supported_record_ids"), std::make_pair(9U, "supported_stream_attribute_ids")}) {
        std::set<uint8_t> seen;
        need(data[field.first]);
        for (unsigned i = 0; i < data[field.first]; ++i) {
            const auto id = data[pos++];
            if (!id || !seen.insert(id).second) throw std::invalid_argument("Intent: duplicate capability ID");
            report[field.second].push_back(id);
        }
    }
    std::set<uint8_t> sensorIds, branchIds;
    for (unsigned i = 0; i < data[10]; ++i) {
        need(2U);
        const uint8_t id = data[pos++];
        if (!id || !sensorIds.insert(id).second) throw std::invalid_argument("Intent: invalid capability sensor");
        report["sensors"].push_back({{"id", id}, {"since", data[pos++]}});
    }
    for (unsigned i = 0; i < data[11]; ++i) {
        need(4U);
        const uint8_t sensor = data[pos++], branch = data[pos++], count = data[pos++], since = data[pos++];
        if (!sensorIds.count(sensor) || !branch || !branchIds.insert(branch).second)
            throw std::invalid_argument("Intent: invalid capability branch");
        json entry = {{"sensor", sensor}, {"id", branch}, {"since", since}, {"formats", json::array()}};
        std::set<uint32_t> formats;
        need(size_t(count) * 4U);
        for (unsigned n = 0; n < count; ++n) {
            const uint32_t value = rsintent::read32(data.data() + pos); pos += 4U;
            if (!formats.insert(value).second) throw std::invalid_argument("Intent: duplicate capability format");
            entry["formats"].push_back(value);
        }
        report["branches"].push_back(entry);
    }
    if (pos != end) throw std::invalid_argument("Intent: invalid capability report length");
    for (; pos < data.size(); ++pos)
        if (data[pos]) throw std::invalid_argument("Intent: nonzero capability padding");
    return report.dump();
}
std::string decode_intent_json(const std::vector<uint8_t> &wire,
                               const std::vector<uint8_t> &status) {
    const auto records = intent_tlv::body(wire);
    intent_tlv::structure(records);
    const auto clean = intent_tlv::transfer(wire, intent_tlv::filter(records));
    rsintent::Request request = {};
    if (rsintent::decode(clean.data(), clean.size(), request).code != rsintent::OK ||
        status.size() != rsintent::status_size || status[0] != rsintent::major ||
        status[3] > rsintent::READ_ONLY ||
        status[rsintent::status_reason_count_offset] > rsintent::max_reject_reasons)
        throw std::invalid_argument("Intent: invalid firmware response");
    warn_version(status);
    json j = {{"version", std::to_string(wire[rsintent::envelope_major_offset]) + "." +
                         std::to_string(wire[rsintent::envelope_minor_offset]) + "." +
                         std::to_string(wire[rsintent::envelope_patch_offset])},
              {"op", request.op == rsintent::CLEAR ? "CLEAR" :
                     (request.op == rsintent::VALIDATE_ONLY ? "VALIDATE_ONLY" : "SET")},
              {"sensors", json::array()}};
    size_t emitted = 0;
    for (const auto &definition : rsintent::sensors) {
        const unsigned id = definition.id;
        json sensor = {{"sensor", definition.sdk_name}, {"streams", json::array()}};
        for (size_t i = 0; i < request.model.count; ++i) {
            const auto &s = request.model.streams[i];
            if (s.sensor != id)
                continue;
            const rsintent::BranchDefinition *branch = nullptr;
            for (const auto &b : rsintent::branches)
                if (b.id == s.branch)
                    branch = &b;
            if (!branch)
                throw std::invalid_argument("Intent: firmware branch requires a newer SDK");
            json stream = {{"stream", branch->sdk_name}, {"index", branch->sdk_index},
                           {"required", s.required}};
            if (!s.enabled)
                stream["state"] = "DISABLED";
            else {
                stream["format"] = formatName(s.format);
                stream["width"] = s.width;
                stream["height"] = s.height;
                stream["fps"] = s.fps;
            }
            sensor["streams"].push_back(stream);
            ++emitted;
        }
        for (const auto &r : records)
            if (r.type == rsintent::SENSOR && intent_tlv::id(r) == id)
                j["sensors"].push_back(sensor);
    }
    if (emitted != request.model.count)
        throw std::invalid_argument("Intent: firmware sensor requires a newer SDK");
    if (request.op == rsintent::CLEAR) j.erase("sensors");
    else attachLayouts(j, records, 0);
    j["status"] = {{"state", status[2]},
                   {"last_result", status[3]},
                   {"error_sensor", status[10]},
                   {"error_branch", status[11]},
                   {"error_offset", rsintent::read16(status.data() + 12)},
                   {"error_rule", rsintent::read16(status.data() + 14)},
                   {"error_detail", rsintent::read16(status.data() + 16)},
                   {"source", status[5] ? "DEFAULT" : "CURRENT"},
                   {"active_mask", rsintent::read32(status.data() + rsintent::status_active_mask_offset)},
                   {"sku", rsintent::read16(status.data() + 8)},
                   {"rule_revision", rsintent::read32(status.data() + 20)},
                   {"reasons", json::array()}};
    for (size_t i = 0; i < status[rsintent::status_reason_count_offset]; ++i) {
        const auto r = rsintent::readReason(status.data() + rsintent::status_header_size +
                                            i * rsintent::reject_reason_size);
        j["status"]["reasons"].push_back({{"kind", r.kind},
                                          {"sensor", r.sensor},
                                          {"record_idx", r.record},
                                          {"branch", r.branch},
                                          {"id", r.id},
                                          {"detail", r.detail}});
    }
    return j.dump(2);
}
std::string describe_intent_status(const std::vector<uint8_t> &status) {
    static const char *const results[] = {"OK",          "BUSY",            "INVALID",
                                          "INCOMPLETE",  "CONSTRAINT",      "WRONG_STATE",
                                          "STREAM_MISMATCH", "CONTROL_STATE", "READ_ONLY"};
    static const char *const mismatches[] = {"UNDECLARED", "DISABLED", "FORMAT", "WIDTH",
                                             "HEIGHT",     "FPS",      "STALE"};
    const auto result = [&](uint8_t code) {
        return code < sizeof(results) / sizeof(*results) ? std::string(results[code])
                                                          : std::to_string(code);
    };
    if (status.size() != rsintent::status_size || status[0] != rsintent::major)
        return "status unavailable";
    const uint16_t sku = rsintent::read16(status.data() + 8);
    std::ostringstream out;
    out << "last_result=" << result(status[3]);
    if (status[3] == rsintent::INVALID || status[3] == rsintent::INCOMPLETE)
        out << " at byte " << rsintent::read16(status.data() + 12);
    const unsigned count = status[rsintent::status_reason_count_offset];
    if (count > rsintent::max_reject_reasons)
        return out.str() + ", malformed reason count " + std::to_string(count);
    for (unsigned i = 0; i < count; ++i) {
        const auto r = rsintent::readReason(status.data() + rsintent::status_header_size +
                                            i * rsintent::reject_reason_size);
        out << (i ? "; " : ": ") << result(r.kind);
        const char *sensor = nullptr;
        for (const auto &s : rsintent::sensors)
            if (s.id == r.sensor)
                sensor = s.name;
        const char *branch = nullptr;
        for (const auto &b : rsintent::branches)
            if (b.id == r.branch && (!branch || b.sku == sku))
                branch = b.name;
        if (r.sensor)
            out << " sensor=" << (sensor ? sensor : std::to_string(r.sensor).c_str());
        if (r.branch)
            out << " branch=" << (branch ? branch : std::to_string(r.branch).c_str());
        if (r.record != rsintent::no_record_index)
            out << " record=" << unsigned(r.record);
        if (r.id)
            out << " id=" << r.id;
        if (r.kind == rsintent::STREAM_MISMATCH && r.detail) {
            const char *separator = " mismatch=";
            for (unsigned bit = 0; bit < 16U; ++bit)
                if (r.detail & (1U << bit)) {
                    out << separator;
                    if (bit < sizeof(mismatches) / sizeof(*mismatches))
                        out << mismatches[bit];
                    else
                        out << "bit" << bit;
                    separator = "|";
                }
        } else if (r.detail)
            out << " detail=" << r.detail;
    }
    return out.str();
}
} // namespace librealsense
