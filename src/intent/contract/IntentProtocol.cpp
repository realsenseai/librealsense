// Copyright (C) 2026 RealSense, Inc. SPDX-License-Identifier: Apache-2.0
#include "IntentProtocol.h"
#include <string.h>
namespace rsintent {
uint16_t read16(const uint8_t *p) {
    return static_cast<uint16_t>(p[0] | (static_cast<unsigned>(p[1]) << 8));
}
uint32_t read32(const uint8_t *p) {
    return static_cast<uint32_t>(read16(p)) | (static_cast<uint32_t>(read16(p + 2)) << 16);
}
void write16(uint8_t *p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
}
void write32(uint8_t *p, uint32_t v) {
    write16(p, static_cast<uint16_t>(v));
    write16(p + 2, static_cast<uint16_t>(v >> 16));
}
uint16_t bit(uint8_t branch) { return static_cast<uint16_t>(branch < 16U ? (1U << branch) : 0U); }
const Stream *find(const Model &m, uint8_t b) {
    for (size_t i = 0; i < m.count; ++i)
        if (m.streams[i].branch == b)
            return &m.streams[i];
    return nullptr;
}
bool same(const Stream &a, const Stream &b) {
    return a.sensor == b.sensor && a.branch == b.branch && a.enabled == b.enabled &&
           (!a.enabled ||
            (a.format == b.format && a.width == b.width && a.height == b.height && a.fps == b.fps));
}
namespace {
struct Record {
    uint8_t type, flags;
    size_t length, start, value, next;
};
bool next(const uint8_t *data, size_t &cursor, size_t end, Record &r) {
    if (cursor > end || end - cursor < 4U)
        return false;
    r.start = cursor;
    r.type = data[cursor];
    r.flags = data[cursor + 1U];
    r.length = read16(data + cursor + 2U);
    r.value = cursor + 4U;
    const size_t padded = (r.length + 3U) & ~size_t(3U);
    if ((r.flags & ~3U) != 0U || padded > end - r.value)
        return false;
    r.next = r.value + padded;
    for (size_t p = r.value + r.length; p < r.next; ++p)
        if (data[p] != 0U)
            return false;
    cursor = r.next;
    return true;
}
// A known record outside its grammar position, or CONTROL (Phase 2), is malformed
// even when not CRITICAL. Only unknown, non-critical records may be skipped.
bool known(uint8_t type) {
    switch (type) {
    case SENSOR:
    case SENSOR_ID:
    case STREAM:
    case BRANCH_ID:
    case FORMAT:
    case WIDTH:
    case HEIGHT:
    case FPS:
    case STATE:
    case CONTROL:
        return true;
    default:
        return false;
    }
}
Error bad(const Record &r, ParseDetail why, uint8_t sensor = 0, uint8_t branch = 0) {
    return Error(INVALID, sensor, branch, static_cast<uint16_t>(r.start), 0U, why);
}
Error skip(const Record &r, uint8_t sensor) {
    if (known(r.type))
        return bad(r, r.type == CONTROL ? PARSE_UNSUPPORTED : PARSE_MISPLACED, sensor);
    if ((r.flags & 1U) != 0U)
        return bad(r, PARSE_UNKNOWN_CRITICAL, sensor);
    return Error();
}
Error stream(const uint8_t *data, const Record &parent, uint8_t sensor, Stream &s) {
    if ((parent.flags & 2U) == 0U)
        return bad(parent, PARSE_SHAPE, sensor);
    s = Stream{};
    s.sensor = sensor;
    s.enabled = true;
    unsigned seen = 0U;
    size_t pos = parent.value;
    const size_t end = pos + parent.length;
    while (pos < end) {
        Record r = {};
        r.start = pos;
        if (!next(data, pos, end, r))
            return bad(r, PARSE_BOUNDS, sensor);
        if (r.type < BRANCH_ID || r.type > STATE) {
            const Error e = skip(r, sensor);
            if (e.code != OK)
                return e;
            continue;
        }
        const unsigned field = 1U << (r.type - BRANCH_ID);
        const size_t width =
            (r.type == FORMAT) ? 4U : ((r.type == BRANCH_ID || r.type == STATE) ? 1U : 2U);
        if ((seen & field) != 0U)
            return bad(r, PARSE_DUPLICATE, sensor, s.branch);
        if ((r.flags & 2U) != 0U || r.length != width)
            return bad(r, PARSE_SHAPE, sensor, s.branch);
        seen |= field;
        switch (r.type) {
        case BRANCH_ID:
            s.branch = data[r.value];
            break;
        case FORMAT:
            s.format = read32(data + r.value);
            break;
        case WIDTH:
            s.width = read16(data + r.value);
            break;
        case HEIGHT:
            s.height = read16(data + r.value);
            break;
        case FPS:
            s.fps = read16(data + r.value);
            break;
        case STATE:
            if (data[r.value] != 0U)
                return bad(r, PARSE_VALUE, sensor, s.branch);
            s.enabled = false;
            break;
        default:
            break;
        }
    }
    if (seen != (s.enabled ? 31U : 33U))
        return Error(INCOMPLETE, sensor, s.branch, static_cast<uint16_t>(parent.start), 0U,
                     PARSE_MISSING);
    if (s.branch == 0U || s.branch > max_branch_id)
        return bad(parent, PARSE_VALUE, sensor, s.branch);
    return Error();
}
void header(uint8_t *p, uint8_t type, uint8_t flags, uint16_t length) {
    p[0] = type;
    p[1] = flags;
    write16(p + 2, length);
}
void attr(uint8_t *p, uint8_t type, uint16_t length, uint32_t value) {
    header(p, type, recordCritical(type), length);
    write32(p + 4, value);
}
} // namespace
Error decode(const uint8_t *data, size_t size, Request &output) {
    // V1 is first-error: 2026-10-28 handover, RSDEV-15834/RSDEV-15835 design baseline.
    // Exactly one XU transfer: shorter is truncated, longer is oversized.
    if (!data || size != wire_size)
        return Error(INVALID, 0, 0, 0, 0, PARSE_TRANSFER_SIZE);
    // Envelope fields are checked in a fixed order; offset names the failing field.
    if (data[0] != major)
        return Error(INVALID, 0, 0, 0, 0, PARSE_MAJOR);
    if (data[1] > CLEAR)
        return Error(INVALID, 0, 0, 1, 0, PARSE_OPERATION);
    if (read16(data + 4) != 0U)
        return Error(INVALID, 0, 0, 4, 0, PARSE_RESERVED);
    const size_t length = read16(data + 2);
    if (length > max_tlv_size || (length & 3U) != 0U || (data[1] == CLEAR && length != 0U))
        return Error(INVALID, 0, 0, 2, 0, PARSE_TLV_SIZE);
    Request candidate = {};
    candidate.op = static_cast<Operation>(data[1]);
    candidate.minorVersion = data[6];
    candidate.patchVersion = data[7];
    size_t pos = envelope_size;
    const size_t end = pos + length;
    unsigned sensorsSeen = 0U;
    uint16_t branchesSeen = 0U;
    while (pos < end) {
        Record r = {};
        r.start = pos;
        if (!next(data, pos, end, r))
            return bad(r, PARSE_BOUNDS);
        if (r.type != SENSOR) {
            const Error e = skip(r, 0U);
            if (e.code != OK)
                return e;
            continue;
        }
        if ((r.flags & 2U) == 0U)
            return bad(r, PARSE_SHAPE);
        size_t child = r.value;
        const size_t childEnd = child + r.length;
        Record id = {};
        id.start = child;
        if (!next(data, child, childEnd, id))
            return bad(id, PARSE_BOUNDS);
        if (id.type != SENSOR_ID || id.length != 4U || (id.flags & 2U) != 0U)
            return bad(id, PARSE_SHAPE);
        const uint8_t sensor = data[id.value];
        if (sensor == 0U || sensor > max_sensor_id || data[id.value + 1U] ||
            data[id.value + 2U] || data[id.value + 3U])
            return bad(id, PARSE_VALUE, sensor);
        if ((sensorsSeen & (1U << sensor)) != 0U)
            return bad(id, PARSE_DUPLICATE, sensor);
        sensorsSeen |= 1U << sensor;
        while (child < childEnd) {
            Record c = {};
            c.start = child;
            if (!next(data, child, childEnd, c))
                return bad(c, PARSE_BOUNDS, sensor);
            if (c.type != STREAM) {
                const Error e = skip(c, sensor);
                if (e.code != OK)
                    return e;
                continue;
            }
            if (candidate.model.count >= max_streams)
                return bad(c, PARSE_CAPACITY, sensor);
            Stream value = {};
            Error e = stream(data, c, sensor, value);
            if (e.code != OK)
                return e;
            if ((branchesSeen & bit(value.branch)) != 0U)
                return bad(c, PARSE_DUPLICATE, sensor, value.branch);
            branchesSeen = static_cast<uint16_t>(branchesSeen | bit(value.branch));
            candidate.model.streams[candidate.model.count++] = value;
        }
    }
    // Ignore transport padding after tlv_size; never interpret it as records.
    output = candidate;
    return Error();
}
Error encode(const Model &m, Operation op, uint8_t *data, size_t size) {
    if (!data || size != wire_size || m.count > max_streams || op > CLEAR)
        return Error(INVALID);
    size_t needed = envelope_size;
    for (unsigned sensor = 1U; sensor <= max_sensor_id; ++sensor) {
        size_t children = 0U;
        for (size_t i = 0; i < m.count; ++i)
            if (m.streams[i].sensor == sensor)
                children += m.streams[i].enabled ? enabled_stream_bytes : disabled_stream_bytes;
        if (children)
            needed += sensor_record_bytes + children;
    }
    for (size_t i = 0; i < m.count; ++i) {
        const Stream &s = m.streams[i];
        if (s.sensor == 0U || s.sensor > max_sensor_id || s.branch == 0U || s.branch > max_branch_id)
            return Error(INVALID);
        for (size_t j = 0; j < i; ++j)
            if (m.streams[j].branch == s.branch)
                return Error(INVALID);
    }
    if (needed > size)
        return Error(INVALID);
    memset(data, 0, size);
    data[0] = static_cast<uint8_t>(major);
    data[1] = op;
    data[6] = static_cast<uint8_t>(minor);
    data[7] = static_cast<uint8_t>(patch);
    if (op == CLEAR)
        return Error();
    size_t pos = envelope_size;
    for (unsigned sensor = 1U; sensor <= max_sensor_id; ++sensor) {
        size_t children = 0U;
        for (size_t i = 0; i < m.count; ++i)
            if (m.streams[i].sensor == sensor)
                children += m.streams[i].enabled ? enabled_stream_bytes : disabled_stream_bytes;
        if (!children)
            continue;
        header(data + pos, SENSOR, 2U | recordCritical(SENSOR), static_cast<uint16_t>(8U + children));
        pos += 4U;
        attr(data + pos, SENSOR_ID, 4U, sensor);
        pos += 8U;
        for (unsigned branch = 1U; branch <= max_branch_id; ++branch) {
            const Stream *s = find(m, static_cast<uint8_t>(branch));
            if (!s || s->sensor != sensor)
                continue;
            header(data + pos, STREAM, 2U | recordCritical(STREAM), s->enabled ? 40U : 16U);
            pos += 4U;
            attr(data + pos, BRANCH_ID, 1U, s->branch);
            pos += 8U;
            if (!s->enabled) {
                attr(data + pos, STATE, 1U, 0U);
                pos += 8U;
                continue;
            }
            attr(data + pos, FORMAT, 4U, s->format);
            pos += 8U;
            attr(data + pos, WIDTH, 2U, s->width);
            pos += 8U;
            attr(data + pos, HEIGHT, 2U, s->height);
            pos += 8U;
            attr(data + pos, FPS, 2U, s->fps);
            pos += 8U;
        }
    }
    write16(data + 2, static_cast<uint16_t>(pos - envelope_size));
    return Error();
}
Model defaultModel(uint16_t sku) {
    Model m = {};
    for (size_t i = 0; i < sizeof(branches) / sizeof(branches[0]); ++i)
        if (branches[i].sku == sku && m.count < max_streams) {
            Stream s = {};
            s.sensor = branches[i].sensor;
            s.branch = branches[i].id;
            m.streams[m.count++] = s;
        }
    return m;
}
} // namespace rsintent
