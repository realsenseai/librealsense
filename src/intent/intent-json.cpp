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
            r.flags = flags;
            result.push_back(r);
        }
    }
    require(consumed.size() == records.size());
    // SENSOR_ID is the first child even when opaque extensions are present.
    if (level == 1) require(!result.empty() && result.front().type == rsintent::SENSOR_ID);
    return result;
}
std::vector<intent_tlv::Record> supported(const std::vector<intent_tlv::Record> &records,
                                        unsigned firmwareMinor, unsigned sourceMinor) {
    using namespace intent_tlv;
    std::vector<Record> result;
    for (auto r : records) {
        unsigned since = rsintent::recordSince(r.type);
        std::string name = "record type " + std::to_string(r.type);
        if (!recognized(r)) {
            // No local since metadata: retain on the source FW or newer, omit on older FW.
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
            r.value = pack(supported(parse(r.value), firmwareMinor, sourceMinor));
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
            LOG_WARNING("Intent: omitting " << name << " (since minor " << since
                        << "; firmware minor " << firmwareMinor << ")");
        } else result.push_back(r);
    }
    return result;
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
// Light D (design v0.4 section 7.10, HR-4): FW GET_CUR is a complete TLV listing every
// sensor and branch the FW supports (all DISABLED when no Intent is stored). Anything it
// does not list is omitted with a warning, like items newer than the FW minor.
std::vector<intent_tlv::Record> reported(const std::vector<intent_tlv::Record> &records,
                                         const std::vector<intent_tlv::Record> &firmware) {
    using namespace intent_tlv;
    std::vector<Record> result;
    for (auto r : records) {
        if (r.type != rsintent::SENSOR) {
            result.push_back(r);
            continue;
        }
        const Record *match = nullptr;
        for (const auto &f : firmware)
            if (f.type == rsintent::SENSOR && id(f) == id(r)) match = &f;
        if (!match) {
            const auto name = containerName(r);
            LOG_WARNING("Intent: omitting " << name << " (not reported by firmware)");
            continue;
        }
        std::set<unsigned> streams;
        for (const auto &c : parse(match->value))
            if (c.type == rsintent::STREAM) streams.insert(id(c));
        std::vector<Record> children;
        for (const auto &c : parse(r.value)) {
            if (c.type == rsintent::STREAM && !streams.count(id(c))) {
                const auto name = containerName(c);
                LOG_WARNING("Intent: omitting " << name << " (not reported by firmware)");
            } else
                children.push_back(c);
        }
        r.value = pack(children);
        result.push_back(r);
    }
    return result;
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
                                       const std::vector<uint8_t> &firmware) {
    const json j = json::parse(text);
    if (!status.empty() && (status.size() != rsintent::status_size || status[0] != rsintent::major ||
                           status[3] > rsintent::READ_ONLY))
        throw std::invalid_argument("Intent: firmware major mismatch or invalid status");
    if (!firmware.empty() && (firmware.size() != rsintent::wire_size || firmware[0] != rsintent::major))
        throw std::invalid_argument("Intent: firmware major mismatch or invalid GET_CUR");
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
                keys(entry, {"branch", "state", "format", "width", "height", "fps", "layout"});
                const auto &def = definition(entry);
                if (def.sensor != id || !seen.insert(def.id).second ||
                    model.count == rsintent::max_streams)
                    throw std::invalid_argument("Intent: invalid or duplicate branch");
                rsintent::Stream s = {};
                s.sensor = def.sensor;
                s.branch = def.id;
                s.enabled = !entry.contains("state");
                if (!s.enabled) {
                    if (entry.at("state") != "DISABLED" || entry.size() != (entry.contains("layout") ? 3U : 2U))
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
    wire[6] = minor;
    wire[7] = patch;
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
        if (!status.empty()) records = supported(records, status[1], minor);
        if (!firmware.empty()) records = reported(records, intent_tlv::body(firmware));
        wire = intent_tlv::transfer(wire, records);
    }
    return wire;
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
    json j = {{"version", std::to_string(wire[0]) + "." + std::to_string(wire[6]) + "." + std::to_string(wire[7])},
              {"op", request.op == rsintent::CLEAR ? "CLEAR" :
                     (request.op == rsintent::VALIDATE_ONLY ? "VALIDATE_ONLY" : "SET")},
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
                   {"active_mask", rsintent::read16(status.data() + 6)},
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
