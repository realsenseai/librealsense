// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
// Generated from schema/dictionary.json by tools/generate_intent_contract_headers.py; do not edit.
#pragma once
#include <stdint.h>
namespace rsintent {
static const char dictionary_sha256[] = "727baba76bf003aaaf6784fc85bf013f9d7936c8d57f4866430fedfcf1e607b2";
static const unsigned major = 1U;
static const unsigned minor = 0U;
static const unsigned patch = 0U;
static const unsigned wire_size = 1024U;
static const unsigned status_size = 1024U;
static const unsigned max_branch_id = 15U;
static const unsigned max_sensor_id = 5U;
static const unsigned max_streams = 8U;
static const unsigned envelope_size = 8U;
static const unsigned record_header_size = 4U;
static const unsigned max_tlv_size = 1016U;
static const unsigned sensor_record_bytes = 12U;
static const unsigned enabled_stream_bytes = 44U;
static const unsigned disabled_stream_bytes = 20U;
static const unsigned envelope_major_offset = 0U;
static const unsigned envelope_minor_offset = 1U;
static const unsigned envelope_patch_offset = 2U;
static const unsigned envelope_operation_offset = 3U;
static const unsigned envelope_tlv_size_offset = 4U;
static const unsigned envelope_reserved_offset = 6U;
static const unsigned rule_revision = 4U;
enum Selectors {
    GLOBAL_INTENT = 28,
    GLOBAL_INTENT_STATUS = 29,
};
enum Records {
    SENSOR = 16,
    SENSOR_ID = 17,
    STREAM = 32,
    BRANCH_ID = 33,
    FORMAT = 34,
    WIDTH = 35,
    HEIGHT = 36,
    FPS = 37,
    STATE = 38,
    CONTROL = 48,
};
inline uint8_t recordCritical(uint8_t type) {
    switch (type) {
    case BRANCH_ID: return 1U;
    case CONTROL: return 0U;
    case FORMAT: return 1U;
    case FPS: return 1U;
    case HEIGHT: return 1U;
    case SENSOR: return 0U;
    case SENSOR_ID: return 1U;
    case STATE: return 1U;
    case STREAM: return 0U;
    case WIDTH: return 1U;
    default: return 0U;
    }
}
inline uint8_t recordSince(uint8_t type) {
    switch (type) {
    case BRANCH_ID: return 0U;
    case CONTROL: return 0U;
    case FORMAT: return 0U;
    case FPS: return 0U;
    case HEIGHT: return 0U;
    case SENSOR: return 0U;
    case SENSOR_ID: return 0U;
    case STATE: return 0U;
    case STREAM: return 0U;
    case WIDTH: return 0U;
    default: return 0U;
    }
}
// Implemented grammar metadata; reserved CONTROL is not supported.
static const uint8_t supported_records[] = {SENSOR, SENSOR_ID, STREAM, BRANCH_ID, FORMAT, WIDTH, HEIGHT, FPS, STATE};
static const uint8_t stream_attributes[] = {BRANCH_ID, FORMAT, WIDTH, HEIGHT, FPS, STATE};
struct SensorDefinition { uint8_t id; const char* name; uint8_t since; };
static const SensorDefinition sensors[] = {
    {1U, "STEREO", 0U},
    {2U, "COLOR", 0U},
    {3U, "MOTION", 0U},
    {4U, "OBJECT_DETECTION", 0U},
    {5U, "DEPTH_MAPPING", 0U},
};
struct SkuDefinition { uint16_t pid; const char* name; };
static const SkuDefinition skus[] = {
    {0x0c06U, "D585F_3C"},
    {0x0c07U, "D585_2C_PROTO"},
    {0x0c08U, "D585_3C_PROTO"},
};
// One row per (branch, SKU); format is the branch default FourCC.
struct BranchDefinition { uint8_t id, sensor; uint16_t sku; uint32_t format; const char* name; uint8_t since; };
static const BranchDefinition branches[] = {
    {1U, 1U, 3078U, 0x2036315aU, "DEPTH", 0U},
    {1U, 1U, 3079U, 0x2036315aU, "DEPTH", 0U},
    {1U, 1U, 3080U, 0x2036315aU, "DEPTH", 0U},
    {4U, 1U, 3078U, 0x20493859U, "IR_LR", 0U},
    {4U, 1U, 3079U, 0x20493859U, "IR_LR", 0U},
    {4U, 1U, 3080U, 0x20493859U, "IR_LR", 0U},
    {5U, 1U, 3079U, 0x3231564eU, "RGB_L", 0U},
    {6U, 1U, 3079U, 0x3231564eU, "RGB_R", 0U},
    {7U, 2U, 3078U, 0x32595559U, "RGB", 0U},
    {7U, 2U, 3080U, 0x32595559U, "RGB", 0U},
    {8U, 3U, 3078U, 0x5a59584dU, "GYRO", 0U},
    {8U, 3U, 3079U, 0x5a59584dU, "GYRO", 0U},
    {8U, 3U, 3080U, 0x5a59584dU, "GYRO", 0U},
    {9U, 3U, 3078U, 0x5a59584dU, "ACCEL", 0U},
    {9U, 3U, 3079U, 0x5a59584dU, "ACCEL", 0U},
    {9U, 3U, 3080U, 0x5a59584dU, "ACCEL", 0U},
    {10U, 4U, 3078U, 0x20203859U, "OD", 0U},
    {10U, 4U, 3079U, 0x20203859U, "OD", 0U},
    {10U, 4U, 3080U, 0x20203859U, "OD", 0U},
    {11U, 5U, 3078U, 0x20203859U, "DMAP", 0U},
    {11U, 5U, 3079U, 0x20203859U, "DMAP", 0U},
    {11U, 5U, 3080U, 0x20203859U, "DMAP", 0U},
};
struct FormatDefinition { uint8_t branch; uint32_t format; };
static const FormatDefinition formats[] = {
    {1U, 0x2036315aU},
    {4U, 0x20493859U},
    {4U, 0x49363159U},
    {5U, 0x3231564eU},
    {5U, 0x32595559U},
    {5U, 0x36315752U},
    {6U, 0x3231564eU},
    {6U, 0x32595559U},
    {6U, 0x36315752U},
    {7U, 0x32595559U},
    {7U, 0x3231564eU},
    {7U, 0x47504a4dU},
    {7U, 0x36315752U},
    {7U, 0x59565955U},
    {7U, 0x36315247U},
    {8U, 0x5a59584dU},
    {9U, 0x5a59584dU},
    {10U, 0x20203859U},
    {11U, 0x20203859U},
};
} // namespace rsintent
