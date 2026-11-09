// Copyright (C) 2026 RealSense, Inc. SPDX-License-Identifier: Apache-2.0
#pragma once
#include "IntentProtocol.h"
namespace rsintent {
// GLOBAL_INTENT_STATUS: read-only control, design v0.4 section 9.8 Option B (OI-12).
// Fixed status_size (1024 B) transfer, little-endian, zero after the last reason:
//    0 major          1 minor        2 state         3 last_result    4 last_op
//    5 source         6 active_mask  8 PID          10 sensor        11 branch
//   12 offset        14 rule        16 detail       18 patch         19 n_reasons
//   20 rule_revision 24 reserved    32 REJECT_REASON x n_reasons
// Bytes 10..17 repeat the first reason plus the decoder byte offset. No host token
// and no reason text.
static const size_t status_header_size = 32U;
static const size_t status_reason_count_offset = 19U;
static const size_t reject_reason_size = 8U;
static const size_t max_reject_reasons = (status_size - status_header_size) / reject_reason_size;
static_assert(max_reject_reasons <= 0xffU, "n_reasons is one byte");
static const uint8_t no_record_index = 0xffU;
// REJECT_REASON, design v0.4 section 7.8 (8 B).
struct RejectReason {
    uint8_t kind;    // last_result code
    uint8_t sensor;
    uint8_t record;  // record_idx; no_record_index when not reported (Phase 1)
    uint8_t branch;
    uint16_t id;     // constraint/rule for CONSTRAINT, ctl_id for CONTROL_STATE, else 0
    uint16_t detail; // ParseDetail for INVALID/INCOMPLETE, StreamMismatch bits for A1
};
// REJECT_REASON.detail of an A1 STREAM_MISMATCH: which requested attributes differ.
enum StreamMismatch : uint16_t {
    MISMATCH_UNDECLARED = 1U << 0, // branch not in the stored Intent
    MISMATCH_DISABLED = 1U << 1,   // branch declared DISABLED
    MISMATCH_FORMAT = 1U << 2,
    MISMATCH_WIDTH = 1U << 3,
    MISMATCH_HEIGHT = 1U << 4,
    MISMATCH_FPS = 1U << 5,
    MISMATCH_STALE = 1U << 6 // start without a COMMIT of the current Intent generation
};
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
    p[0] = r.kind;
    p[1] = r.sensor;
    p[2] = r.record;
    p[3] = r.branch;
    write16(p + 4, r.id);
    write16(p + 6, r.detail);
}
inline RejectReason readReason(const uint8_t *p) {
    RejectReason r = {p[0], p[1], p[2], p[3], read16(p + 4), read16(p + 6)};
    return r;
}
} // namespace rsintent
