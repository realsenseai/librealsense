// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
// Generated from schema/dictionary.json by tools/generate_intent_contract_headers.py; do not edit.
#pragma once
#include <stdint.h>
namespace rsintent {
static const char dictionary_sha256[] = "71d207f7e76b8b6b633e9fd4f9f4476d9940609c26cad6db54f82dbc678a27ad";
static const unsigned major = 1U;
static const unsigned minor = 0U;
static const unsigned patch = 0U;
static const unsigned wire_size = 1000U;
static const unsigned status_size = 1000U;
static const unsigned max_branch_id = 31U;
static const unsigned max_sensor_id = 31U;
static const unsigned max_streams = 20U;
static const unsigned envelope_size = 8U;
static const unsigned record_header_size = 4U;
static const unsigned max_tlv_size = 992U;
static const unsigned sensor_record_bytes = 12U;
static const unsigned enabled_stream_bytes = 44U;
static const unsigned disabled_stream_bytes = 20U;
static const unsigned envelope_major_offset = 0U;
static const unsigned envelope_minor_offset = 1U;
static const unsigned envelope_patch_offset = 2U;
static const unsigned envelope_operation_offset = 3U;
static const unsigned envelope_tlv_size_offset = 4U;
static const unsigned envelope_reserved_offset = 6U;
static const unsigned rule_revision = 5U;
enum Opcodes {
    HWMC_SET_INTENT = 193,
    HWMC_GET_INTENT = 194,
    HWMC_GET_INTENT_STATUS = 195,
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
struct SensorDefinition { uint8_t id; const char* name; uint8_t since; const char* sdk_name; };
static const SensorDefinition sensors[] = {
    {1U, "STEREO", 0U, "Stereo Module"},
    {2U, "COLOR", 0U, "RGB Camera"},
    {3U, "MOTION", 0U, "Motion Module"},
    {4U, "PERCEPTION", 0U, "Perception"},
};
struct SkuDefinition { uint16_t pid; const char* name; };
static const SkuDefinition skus[] = {
    {0x0c04U, "D585_2C"},
    {0x0c05U, "D585_3C"},
    {0x0c06U, "D585F_3C"},
    {0x0c07U, "D585_2C_PROTO"},
    {0x0c08U, "D585_3C_PROTO"},
};
// One row per (branch, SKU); format is the branch default FourCC.
struct BranchDefinition { uint8_t id, sensor; uint16_t sku; uint32_t format; const char* name; uint8_t since; const char* sdk_name; uint8_t sdk_index, native_branch, gmsl_member; };
static const BranchDefinition branches[] = {
    {1U, 1U, 3076U, 0x2036315aU, "DEPTH", 0U, "Depth", 0U, 1U, 0U},
    {1U, 1U, 3077U, 0x2036315aU, "DEPTH", 0U, "Depth", 0U, 1U, 0U},
    {1U, 1U, 3078U, 0x2036315aU, "DEPTH", 0U, "Depth", 0U, 1U, 0U},
    {1U, 1U, 3079U, 0x2036315aU, "DEPTH", 0U, "Depth", 0U, 1U, 0U},
    {1U, 1U, 3080U, 0x2036315aU, "DEPTH", 0U, "Depth", 0U, 1U, 0U},
    {2U, 1U, 3076U, 0x20203859U, "IR_LEFT", 0U, "Infrared", 1U, 4U, 0U},
    {2U, 1U, 3077U, 0x20203859U, "IR_LEFT", 0U, "Infrared", 1U, 4U, 0U},
    {2U, 1U, 3078U, 0x20203859U, "IR_LEFT", 0U, "Infrared", 1U, 4U, 0U},
    {2U, 1U, 3079U, 0x20203859U, "IR_LEFT", 0U, "Infrared", 1U, 4U, 0U},
    {2U, 1U, 3080U, 0x20203859U, "IR_LEFT", 0U, "Infrared", 1U, 4U, 0U},
    {3U, 1U, 3076U, 0x20203859U, "IR_RIGHT", 0U, "Infrared", 2U, 4U, 0U},
    {3U, 1U, 3077U, 0x20203859U, "IR_RIGHT", 0U, "Infrared", 2U, 4U, 0U},
    {3U, 1U, 3078U, 0x20203859U, "IR_RIGHT", 0U, "Infrared", 2U, 4U, 0U},
    {3U, 1U, 3079U, 0x20203859U, "IR_RIGHT", 0U, "Infrared", 2U, 4U, 0U},
    {3U, 1U, 3080U, 0x20203859U, "IR_RIGHT", 0U, "Infrared", 2U, 4U, 0U},
    {5U, 1U, 3076U, 0x3231564eU, "RGB_L", 0U, "Color", 1U, 5U, 0U},
    {5U, 1U, 3079U, 0x3231564eU, "RGB_L", 0U, "Color", 1U, 5U, 0U},
    {6U, 1U, 3076U, 0x3231564eU, "RGB_R", 0U, "Color", 2U, 6U, 0U},
    {6U, 1U, 3079U, 0x3231564eU, "RGB_R", 0U, "Color", 2U, 6U, 0U},
    {7U, 2U, 3077U, 0x3231564eU, "RGB", 0U, "Color", 0U, 7U, 0U},
    {7U, 2U, 3078U, 0x3231564eU, "RGB", 0U, "Color", 0U, 7U, 0U},
    {7U, 2U, 3080U, 0x3231564eU, "RGB", 0U, "Color", 0U, 7U, 0U},
    {8U, 3U, 3076U, 0x5a59584dU, "GYRO", 0U, "Gyro", 0U, 8U, 0U},
    {8U, 3U, 3077U, 0x5a59584dU, "GYRO", 0U, "Gyro", 0U, 8U, 0U},
    {8U, 3U, 3078U, 0x5a59584dU, "GYRO", 0U, "Gyro", 0U, 8U, 0U},
    {8U, 3U, 3079U, 0x5a59584dU, "GYRO", 0U, "Gyro", 0U, 8U, 0U},
    {8U, 3U, 3080U, 0x5a59584dU, "GYRO", 0U, "Gyro", 0U, 8U, 0U},
    {9U, 3U, 3076U, 0x5a59584dU, "ACCEL", 0U, "Accel", 0U, 9U, 0U},
    {9U, 3U, 3077U, 0x5a59584dU, "ACCEL", 0U, "Accel", 0U, 9U, 0U},
    {9U, 3U, 3078U, 0x5a59584dU, "ACCEL", 0U, "Accel", 0U, 9U, 0U},
    {9U, 3U, 3079U, 0x5a59584dU, "ACCEL", 0U, "Accel", 0U, 9U, 0U},
    {9U, 3U, 3080U, 0x5a59584dU, "ACCEL", 0U, "Accel", 0U, 9U, 0U},
    {10U, 4U, 3076U, 0x20203859U, "OD", 0U, "Object Detection", 0U, 10U, 6U},
    {10U, 4U, 3077U, 0x20203859U, "OD", 0U, "Object Detection", 0U, 10U, 6U},
    {10U, 4U, 3078U, 0x20203859U, "OD", 0U, "Object Detection", 0U, 10U, 6U},
    {10U, 4U, 3079U, 0x20203859U, "OD", 0U, "Object Detection", 0U, 10U, 6U},
    {10U, 4U, 3080U, 0x20203859U, "OD", 0U, "Object Detection", 0U, 10U, 6U},
    {11U, 4U, 3076U, 0x20203859U, "OCCUPANCY", 0U, "Occupancy", 0U, 11U, 7U},
    {11U, 4U, 3077U, 0x20203859U, "OCCUPANCY", 0U, "Occupancy", 0U, 11U, 7U},
    {11U, 4U, 3078U, 0x20203859U, "OCCUPANCY", 0U, "Occupancy", 0U, 11U, 7U},
    {11U, 4U, 3079U, 0x20203859U, "OCCUPANCY", 0U, "Occupancy", 0U, 11U, 7U},
    {11U, 4U, 3080U, 0x20203859U, "OCCUPANCY", 0U, "Occupancy", 0U, 11U, 7U},
    {12U, 4U, 3076U, 0x20203859U, "LPCL", 0U, "Labeled Point Cloud", 0U, 12U, 8U},
    {12U, 4U, 3077U, 0x20203859U, "LPCL", 0U, "Labeled Point Cloud", 0U, 12U, 8U},
    {12U, 4U, 3078U, 0x20203859U, "LPCL", 0U, "Labeled Point Cloud", 0U, 12U, 8U},
    {12U, 4U, 3079U, 0x20203859U, "LPCL", 0U, "Labeled Point Cloud", 0U, 12U, 8U},
    {12U, 4U, 3080U, 0x20203859U, "LPCL", 0U, "Labeled Point Cloud", 0U, 12U, 8U},
    {13U, 4U, 3076U, 0x465a5958U, "PCL", 0U, "Point Cloud", 0U, 13U, 9U},
    {13U, 4U, 3077U, 0x465a5958U, "PCL", 0U, "Point Cloud", 0U, 13U, 9U},
    {13U, 4U, 3078U, 0x465a5958U, "PCL", 0U, "Point Cloud", 0U, 13U, 9U},
    {13U, 4U, 3079U, 0x465a5958U, "PCL", 0U, "Point Cloud", 0U, 13U, 9U},
    {13U, 4U, 3080U, 0x465a5958U, "PCL", 0U, "Point Cloud", 0U, 13U, 9U},
};
struct FormatDefinition { uint8_t branch; uint32_t format; };
static const FormatDefinition formats[] = {
    {1U, 0x2036315aU},
    {2U, 0x20203859U},
    {2U, 0x20363159U},
    {3U, 0x20203859U},
    {3U, 0x20363159U},
    {5U, 0x3231564eU},
    {5U, 0x32595559U},
    {5U, 0x36315752U},
    {5U, 0x38424752U},
    {5U, 0x38524742U},
    {5U, 0x41424752U},
    {5U, 0x41524742U},
    {6U, 0x3231564eU},
    {6U, 0x32595559U},
    {6U, 0x36315752U},
    {6U, 0x38424752U},
    {6U, 0x38524742U},
    {6U, 0x41424752U},
    {6U, 0x41524742U},
    {7U, 0x3231564eU},
    {7U, 0x32595559U},
    {7U, 0x47504a4dU},
    {7U, 0x36315752U},
    {7U, 0x59565955U},
    {7U, 0x38424752U},
    {7U, 0x38524742U},
    {7U, 0x41424752U},
    {7U, 0x41524742U},
    {8U, 0x5a59584dU},
    {9U, 0x5a59584dU},
    {10U, 0x20203859U},
    {11U, 0x20203859U},
    {12U, 0x20203859U},
    {13U, 0x465a5958U},
};
struct SdkFormatDefinition { const char* name; uint32_t format, native, native_gmsl; };
static const SdkFormatDefinition sdk_formats[] = {
    {"Z16", 0x2036315aU, 0x2036315aU, 0x2036315aU},
    {"Y8", 0x20203859U, 0x20203859U, 0x20203859U},
    {"Y16", 0x20363159U, 0x20363159U, 0x20363159U},
    {"Y8I", 0x20493859U, 0x20493859U, 0x20493859U},
    {"Y16I", 0x49363159U, 0x49363159U, 0x49363159U},
    {"NV12", 0x3231564eU, 0x3231564eU, 0x3231564eU},
    {"YUYV", 0x32595559U, 0x32595559U, 0x32595559U},
    {"UYVY", 0x59565955U, 0x59565955U, 0x59565955U},
    {"MJPEG", 0x47504a4dU, 0x47504a4dU, 0x47504a4dU},
    {"RAW16", 0x36315752U, 0x36315752U, 0x36315247U},
    {"MOTION_XYZ32F", 0x5a59584dU, 0x5a59584dU, 0x5a59584dU},
    {"XYZ32F", 0x465a5958U, 0x20203859U, 0x20203859U},
    {"RGB8", 0x38424752U, 0x3231564eU, 0x3231564eU},
    {"BGR8", 0x38524742U, 0x3231564eU, 0x3231564eU},
    {"RGBA8", 0x41424752U, 0x3231564eU, 0x3231564eU},
    {"BGRA8", 0x41524742U, 0x3231564eU, 0x3231564eU},
};
} // namespace rsintent
