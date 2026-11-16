// Copyright (C) 2026 RealSense, Inc. SPDX-License-Identifier: Apache-2.0
#pragma once
#include <IntentDictionary.h>
#include <stddef.h>
#include <stdint.h>
namespace rsintent {
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
    bool deviceDeclared;
};
struct Compatibility {
    uint16_t skippedCount, offset, id;
    uint8_t type, sensor, branch;
};
struct Request {
    Operation op;
    uint8_t minorVersion;
    uint8_t patchVersion;
    Model model;
    Compatibility compatibility;
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
const Stream *find(const Model &model, uint8_t branch);
bool same(const Stream &a, const Stream &b);
uint32_t bit(uint8_t branch);
bool knownSensor(uint8_t sensor);
bool knownBranch(uint8_t branch);
Stream nativeStream(const Stream &stream, bool gmsl = false);
typedef bool (*CapabilityFilter)(uint8_t branch, uint32_t format, void *context);
Error capabilities(uint16_t sku, uint8_t *data, size_t size,
                   CapabilityFilter filter = nullptr, void *context = nullptr);
} // namespace rsintent
