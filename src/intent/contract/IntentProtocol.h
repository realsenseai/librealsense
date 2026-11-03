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
static const size_t max_streams = 8;
struct Model {
    Stream streams[max_streams];
    size_t count;
};
struct Request {
    Operation op;
    uint8_t minorVersion;
    Model model;
};
uint16_t read16(const uint8_t *p);
uint32_t read32(const uint8_t *p);
void write16(uint8_t *p, uint16_t v);
void write32(uint8_t *p, uint32_t v);
Error decode(const uint8_t *data, size_t size, Request &output);
Error encode(const Model &model, Operation op, uint8_t *data, size_t size);
Model defaultModel(uint16_t sku);
Error validate(const Model &model, uint16_t sku);
const Stream *find(const Model &model, uint8_t branch);
bool same(const Stream &a, const Stream &b);
uint16_t bit(uint8_t branch);
} // namespace rsintent
