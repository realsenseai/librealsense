// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
// Generated from schema/dictionary.json by tools/generate_intent_contract_headers.py; do not edit.
#pragma once
#include <stdint.h>
namespace rsintent {
static const char dictionary_sha256[] = "2a2b43cff65b7af13408aee6dbbdd9497a03949d81c8abcead84345a8ddbbcb6";
static const unsigned major = 1U;
static const unsigned minor = 0U;
static const unsigned patch = 0U;
static const unsigned wire_size = 1024U;
static const unsigned status_size = 1024U;
static const unsigned max_branch_id = 31U;
static const unsigned max_sensor_id = 31U;
static const unsigned max_streams = 20U;
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
static const unsigned rule_revision = 5U;
static const unsigned status_header_size = 64U;
static const unsigned status_active_mask_offset = 24U;
static const unsigned status_reason_count_offset = 19U;
static const unsigned reject_reason_size = 8U;
static const unsigned max_reject_reasons = 32U;
static const unsigned no_record_index = 255U;
static const unsigned status_conformance_offset = 6U;
static const unsigned status_flags_offset = 7U;
static const unsigned status_skipped_count_offset = 28U;
static const unsigned status_skipped_offset_offset = 30U;
static const unsigned status_skipped_type_offset = 32U;
static const unsigned status_skipped_sensor_offset = 33U;
static const unsigned status_skipped_branch_offset = 34U;
static const unsigned status_skipped_id_offset = 36U;
static const unsigned status_text_length_offset = 38U;
static const unsigned status_host_token_offset = 40U;
static const unsigned status_discovery_length_offset = 48U;
static const unsigned status_format_version_offset = 50U;
static const unsigned status_text_offset = 320U;
static const unsigned status_text_capacity = 128U;
static const unsigned status_discovery_offset = 448U;
static const unsigned status_discovery_capacity = 576U;
static const unsigned status_major_offset = 0U;
static const unsigned status_minor_offset = 1U;
static const unsigned status_state_offset = 2U;
static const unsigned status_result_offset = 3U;
static const unsigned status_operation_offset = 4U;
static const unsigned status_source_offset = 5U;
static const unsigned status_sku_offset = 8U;
static const unsigned status_error_sensor_offset = 10U;
static const unsigned status_error_branch_offset = 11U;
static const unsigned status_error_offset_offset = 12U;
static const unsigned status_error_rule_offset = 14U;
static const unsigned status_error_detail_offset = 16U;
static const unsigned status_patch_offset = 18U;
static const unsigned status_rule_revision_offset = 20U;
static const unsigned status_total_reasons_offset = 52U;
static const unsigned reason_kind_offset = 0U;
static const unsigned reason_sensor_offset = 1U;
static const unsigned reason_record_offset = 2U;
static const unsigned reason_branch_offset = 3U;
static const unsigned reason_id_offset = 4U;
static const unsigned reason_detail_offset = 6U;
enum Result : uint8_t {
    OK = 0,
    BUSY = 1,
    INVALID = 2,
    INCOMPLETE = 3,
    CONSTRAINT = 4,
    WRONG_STATE = 5,
    STREAM_MISMATCH = 6,
    CONTROL_STATE = 7,
    READ_ONLY = 8,
    INTERNAL = 255,
};
enum ParseDetail : uint16_t {
    PARSE_NONE = 0,
    PARSE_TRANSFER_SIZE = 1,
    PARSE_MAJOR = 2,
    PARSE_OPERATION = 3,
    PARSE_RESERVED = 4,
    PARSE_TLV_SIZE = 5,
    PARSE_BOUNDS = 6,
    PARSE_SHAPE = 7,
    PARSE_MISPLACED = 8,
    PARSE_UNKNOWN_CRITICAL = 9,
    PARSE_UNSUPPORTED = 10,
    PARSE_DUPLICATE = 11,
    PARSE_VALUE = 12,
    PARSE_CAPACITY = 13,
    PARSE_MISSING = 14,
};
enum Operation : uint8_t {
    SET = 0,
    VALIDATE_ONLY = 1,
    CLEAR = 2,
};
enum State : uint8_t {
    UNSET = 0,
    VALIDATING = 1,
    CONFIGURED = 2,
    ACTIVATING = 3,
    LOCKED = 4,
    UNLOCKING = 5,
};
enum Conformance : uint8_t {
    CONFORMANCE_UNAVAILABLE = 0,
    CONFORMANCE_MATCH = 1,
    CONFORMANCE_MISMATCH = 2,
};
enum StreamMismatch : uint16_t {
    MISMATCH_UNDECLARED = 1,
    MISMATCH_DISABLED = 2,
    MISMATCH_FORMAT = 4,
    MISMATCH_WIDTH = 8,
    MISMATCH_HEIGHT = 16,
    MISMATCH_FPS = 32,
    MISMATCH_STALE = 64,
};
enum StatusFlags : uint8_t {
    STATUS_SKIPPED_ITEMS = 1,
    STATUS_NEWER_SENDER = 2,
    STATUS_HOST_TOKEN_VALID = 4,
    STATUS_REASONS_TRUNCATED = 8,
};
enum TransportDefinitions {
    XU_INTENT = 28,
    XU_STATUS = 29,
    GMSL_INTENT_ADDRESS = 28672,
    GMSL_STATUS_ADDRESS = 29696,
    GMSL_INTENT_CID_OFFSET = 65,
    GMSL_STATUS_CID_OFFSET = 66,
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
    {0U, "DEVICE", 0U, "Device"},
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
