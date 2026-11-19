// Copyright (C) 2026 RealSense, Inc. SPDX-License-Identifier: Apache-2.0
#pragma once
#include "IntentProtocol.h"
namespace rsintent {
// I1 STATUS encoding; the I2 controller produces the state and diagnostic values.
// Fixed 1024 B XU data. All enums, offsets and section capacities are generated
// from dictionary.json. Numeric reasons, UTF-8 text and grammar discovery occupy
// distinct bounded sections; unused bytes are zero.
// Bytes 10..17 repeat the first reason plus the decoder byte offset. Host-token
// echo is reserved at offset 40; its valid flag is clear until token input exists.
static_assert(max_reject_reasons <= 0xffU, "n_reasons is one byte");
// REJECT_REASON, design v0.4 section 7.8 (8 B).
struct RejectReason {
    uint8_t kind;    // last_result code
    uint8_t sensor;
    uint8_t record;  // record_idx; no_record_index when not reported (Phase 1)
    uint8_t branch;
    uint16_t id;     // constraint/rule for CONSTRAINT, ctl_id for CONTROL_STATE, else 0
    uint16_t detail; // ParseDetail for INVALID/INCOMPLETE, StreamMismatch bits for A1
};
constexpr uint8_t status_layout_i1 = 1;
constexpr uint8_t status_layout_i2 = 2;

inline bool supportedStatusLayout(uint8_t version) {
    return version == status_layout_i1 || version == status_layout_i2;
}

inline bool validStatusResult(uint8_t version, uint8_t result, uint8_t mask) {
    if (version == status_layout_i1)
        return result <= READ_ONLY;
    constexpr uint8_t known = REJECT_A1 | REJECT_A2 | REJECT_A3;
    if (version != status_layout_i2 || (mask & ~known))
        return false;
    if (result == ADMISSION_REJECT)
        return mask == REJECT_A3 || (mask && !(mask & REJECT_A3));
    return result <= READ_ONLY && !mask;
}

// Result codes that carry a REJECT_REASON (design v0.4 section 7.8 kind list).
inline bool hasReason(uint8_t code) {
    return code == INVALID || code == INCOMPLETE || code == CONSTRAINT || code == STREAM_MISMATCH ||
           code == CONTROL_STATE;
}
inline RejectReason toReason(const Error &e) {
    RejectReason r = {e.code, e.sensor, no_record_index, e.branch, e.rule, e.detail};
    return r;
}
inline void writeReason(uint8_t *p, const RejectReason &r) {
    p[reason_kind_offset] = r.kind;
    p[reason_sensor_offset] = r.sensor;
    p[reason_record_offset] = r.record;
    p[reason_branch_offset] = r.branch;
    write16(p + reason_id_offset, r.id);
    write16(p + reason_detail_offset, r.detail);
}
inline RejectReason readReason(const uint8_t *p) {
    RejectReason r = {p[reason_kind_offset], p[reason_sensor_offset], p[reason_record_offset],
                      p[reason_branch_offset], read16(p + reason_id_offset), read16(p + reason_detail_offset)};
    return r;
}
} // namespace rsintent
