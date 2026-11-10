// License: Apache 2.0. Copyright (C) 2026 RealSense, Inc.
#pragma once
#include "contract/IntentProtocol.h"
#include <rsutils/json.h>
#include <algorithm>
#include <stdexcept>
#include <set>
#include <vector>
namespace librealsense {
namespace intent_tlv {
using rsutils::json;
struct Record {
    uint8_t type, flags;
    std::vector<uint8_t> value;
};
inline void require(bool valid) {
    if (!valid) throw std::invalid_argument("Intent: invalid TLV layout or opaque record");
}
inline std::vector<Record> parse(const std::vector<uint8_t> &data) {
    std::vector<Record> records;
    for (size_t pos = 0; pos < data.size();) {
        require(data.size() - pos >= 4);
        const size_t length = rsintent::read16(data.data() + pos + 2);
        const size_t padded = (length + 3U) & ~size_t(3U);
        require((data[pos + 1] & ~3U) == 0 && padded <= data.size() - pos - 4);
        Record r = {data[pos], data[pos + 1], {}};
        r.value.assign(data.begin() + pos + 4, data.begin() + pos + 4 + length);
        for (size_t i = length; i < padded; ++i) require(data[pos + 4 + i] == 0);
        records.push_back(r);
        pos += 4 + padded;
    }
    return records;
}
inline std::vector<uint8_t> pack(const std::vector<Record> &records) {
    std::vector<uint8_t> bytes;
    for (const auto &r : records) {
        require(r.value.size() <= rsintent::max_tlv_size && bytes.size() + 4 + ((r.value.size() + 3U) & ~size_t(3U)) <= rsintent::max_tlv_size);
        const size_t pos = bytes.size();
        bytes.resize(pos + 4 + ((r.value.size() + 3U) & ~size_t(3U)), 0);
        bytes[pos] = r.type; bytes[pos + 1] = r.flags;
        rsintent::write16(bytes.data() + pos + 2, static_cast<uint16_t>(r.value.size()));
        std::copy(r.value.begin(), r.value.end(), bytes.begin() + pos + 4);
    }
    return bytes;
}
inline bool known(uint8_t type) {
    switch (type) {
    case rsintent::SENSOR: case rsintent::SENSOR_ID: case rsintent::STREAM:
    case rsintent::BRANCH_ID: case rsintent::FORMAT: case rsintent::WIDTH:
    case rsintent::HEIGHT: case rsintent::FPS: case rsintent::STATE: case rsintent::CONTROL:
        return true;
    default: return false;
    }
}
inline unsigned id(const Record &r) {
    if (r.type != rsintent::SENSOR && r.type != rsintent::STREAM) return 0;
    require((r.flags & 2U) != 0);
    const auto children = parse(r.value);
    const unsigned type = r.type == rsintent::SENSOR ? rsintent::SENSOR_ID : rsintent::BRANCH_ID;
    unsigned result = 0;
    for (const auto &c : children) if (c.type == type) {
        require(!result && !(c.flags & 2U) && c.value.size() == (type == rsintent::SENSOR_ID ? 4U : 1U));
        result = c.value[0];
        if (type == rsintent::SENSOR_ID) require(!c.value[1] && !c.value[2] && !c.value[3]);
    }
    require(result != 0);
    return result;
}
inline bool recognized(const Record &r) {
    if (r.type == rsintent::SENSOR) {
        for (const auto &s : rsintent::sensors) if (s.id == id(r)) return true;
        return false;
    }
    if (r.type == rsintent::STREAM) {
        for (const auto &b : rsintent::branches) if (b.id == id(r)) return true;
        return false;
    }
    return known(r.type);
}
inline void structure(const std::vector<Record> &records, unsigned level = 0) {
    std::set<unsigned> ids;
    for (const auto &r : records) {
        if (r.type == rsintent::SENSOR || r.type == rsintent::STREAM) {
            require((r.type == rsintent::SENSOR && level == 0) || (r.type == rsintent::STREAM && level == 1));
            require(ids.insert(id(r)).second);
            const auto children = parse(r.value);
            if (r.type == rsintent::SENSOR)
                require(!children.empty() && children.front().type == rsintent::SENSOR_ID);
            if (recognized(r)) structure(children, level + 1);
        }
    }
}
inline std::vector<Record> filter(const std::vector<Record> &records) {
    std::vector<Record> clean;
    for (auto r : records) if (recognized(r)) {
        if (r.type == rsintent::SENSOR || r.type == rsintent::STREAM) r.value = pack(filter(parse(r.value)));
        clean.push_back(r);
    }
    return clean;
}
inline std::vector<uint8_t> transfer(const std::vector<uint8_t> &original, const std::vector<Record> &records) {
    require(original.size() == rsintent::wire_size);
    auto body = pack(records);
    auto wire = original;
    std::fill(wire.begin() + 8, wire.end(), 0);
    require(body.size() <= wire.size() - 8);
    std::copy(body.begin(), body.end(), wire.begin() + 8);
    rsintent::write16(wire.data() + rsintent::envelope_tlv_size_offset, static_cast<uint16_t>(body.size()));
    return wire;
}
inline std::vector<Record> body(const std::vector<uint8_t> &wire) {
    require(wire.size() == rsintent::wire_size);
    const unsigned length = rsintent::read16(wire.data() + rsintent::envelope_tlv_size_offset);
    require(length <= rsintent::max_tlv_size && length % 4 == 0 &&
            (wire[rsintent::envelope_operation_offset] != rsintent::CLEAR || length == 0));
    return parse(std::vector<uint8_t>(wire.begin() + 8, wire.begin() + 8 + length));
}
inline std::vector<uint8_t> canonical_known(const std::vector<uint8_t> &wire) {
    const auto records = body(wire);
    structure(records);
    const auto clean = transfer(wire, filter(records));
    rsintent::Request request = {};
    require(rsintent::decode(clean.data(), clean.size(), request).code == rsintent::OK);
    std::vector<uint8_t> canonical(rsintent::wire_size);
    require(rsintent::encode(request.model, rsintent::SET, canonical.data(), canonical.size()).code == rsintent::OK);
    return canonical;
}
inline std::string hex(const std::vector<uint8_t> &bytes) {
    const char *digits = "0123456789abcdef";
    std::string text;
    for (auto b : bytes) { text += digits[b >> 4]; text += digits[b & 15]; }
    return text;
}
inline json layout(const std::vector<Record> &records) {
    json result = json::array();
    for (const auto &r : records) {
        json entry = {{"type", r.type}, {"flags", r.flags}};
        if (!recognized(r)) entry["hex"] = hex(r.value);
        if (r.type == rsintent::SENSOR || r.type == rsintent::STREAM) entry["id"] = id(r);
        result.push_back(entry);
    }
    return result;
}
} // namespace intent_tlv
} // namespace librealsense
