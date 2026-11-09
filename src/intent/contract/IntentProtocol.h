// Copyright (C) 2026 RealSense, Inc. SPDX-License-Identifier: Apache-2.0
#pragma once
#include <IntentDictionary.h>
#include <stddef.h>
#include <stdint.h>
namespace rsintent {
enum Result : uint8_t {
    OK,
    BUSY,
    INVALID,
    INCOMPLETE,
    CONSTRAINT,
    WRONG_STATE,
    STREAM_MISMATCH,
    CONTROL_STATE,
    READ_ONLY,
    INTERNAL
};
// Error::detail for INVALID/INCOMPLETE decode results (Error::rule is 0).
enum ParseDetail : uint16_t {
    PARSE_NONE,
    PARSE_TRANSFER_SIZE,    // null buffer or size != wire_size (truncated/oversized)
    PARSE_MAJOR,            // major version differs from the FW major
    PARSE_OPERATION,        // unknown operation code
    PARSE_RESERVED,         // nonzero envelope reserved byte
    PARSE_TLV_SIZE,         // tlv_size too large, unaligned, or CLEAR with a body
    PARSE_BOUNDS,           // record exceeds its parent, bad flag bits or nonzero pad
    PARSE_SHAPE,            // container/scalar flag, scalar width or SENSOR_ID position
    PARSE_MISPLACED,        // known record outside its grammar position
    PARSE_UNKNOWN_CRITICAL, // unknown record marked CRITICAL
    PARSE_UNSUPPORTED,      // CONTROL record; not supported in Phase 1
    PARSE_DUPLICATE,        // repeated sensor, branch or stream attribute
    PARSE_VALUE,            // out-of-range sensor/branch ID or STATE value
    PARSE_CAPACITY,         // more than max_streams streams
    PARSE_MISSING           // stream attribute set incomplete (Result INCOMPLETE)
};
enum Operation : uint8_t { SET, VALIDATE_ONLY, CLEAR };
enum State : uint8_t { UNSET, VALIDATING, CONFIGURED, ACTIVATING, LOCKED, UNLOCKING };
struct Error {
    Result code;
    uint8_t sensor, branch;
    uint16_t offset, rule, detail;
    Error(Result c = OK, uint8_t s = 0, uint8_t b = 0, uint16_t o = 0, uint16_t r = 0,
          uint16_t d = 0)
        : code(c), sensor(s), branch(b), offset(o), rule(r), detail(d) {}
};
struct Stream {
    uint8_t sensor, branch;
    bool enabled;
    uint32_t format;
    uint16_t width, height, fps;
};
struct Model {
    Stream streams[max_streams];
    size_t count;
};
struct Request {
    Operation op;
    uint8_t minorVersion;
    uint8_t patchVersion;
    Model model;
};
uint16_t read16(const uint8_t *p);
uint32_t read32(const uint8_t *p);
void write16(uint8_t *p, uint16_t v);
void write32(uint8_t *p, uint32_t v);
// I1 codec. decode() publishes output only on success; on failure output is
// untouched. encode() writes data only on success. Both require size == wire_size.
Error decode(const uint8_t *data, size_t size, Request &output);
Error encode(const Model &model, Operation op, uint8_t *data, size_t size);
// SKU template: every branch of the SKU, DISABLED, in branch-ID order.
Model defaultModel(uint16_t sku);
// C1 placeholder rules (IntentRules.cpp); first error in stable branch-ID order.
Error completeness(const Model &model, uint16_t sku, uint8_t senderMinor,
                   const BranchDefinition *definitions = branches,
                   size_t count = sizeof(branches) / sizeof(branches[0]));
Error validate(const Model &model, uint16_t sku);
const Stream *find(const Model &model, uint8_t branch);
bool same(const Stream &a, const Stream &b);
uint16_t bit(uint8_t branch);
} // namespace rsintent
