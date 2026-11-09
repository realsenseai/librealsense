// Generated from schema/dictionary.json by tools/generate.py; do not edit.
#pragma once
#include <stdint.h>
namespace rsintent {
static const char dictionary_sha256[] = "79831d9570cbc541c04f1a21ece50ade3df57cd1fb9502c6bb9e4e8440b85d80";
static const unsigned major = 1U;
static const unsigned minor = 0U;
static const unsigned patch = 0U;
static const unsigned wire_size = 1024U;
static const unsigned status_size = 32U;
static const unsigned max_branch_id = 15U;
static const unsigned max_sensor_id = 5U;
static const unsigned max_streams = 8U;
static const unsigned envelope_size = 8U;
static const unsigned record_header_size = 4U;
static const unsigned max_tlv_size = 1016U;
static const unsigned sensor_record_bytes = 12U;
static const unsigned enabled_stream_bytes = 44U;
static const unsigned disabled_stream_bytes = 20U;
static const unsigned rule_revision = 3U;
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
struct SensorDefinition { uint8_t id; const char* name; };
static const SensorDefinition sensors[] = {
    {1U, "STEREO"},
    {2U, "COLOR"},
};
struct SkuDefinition { uint16_t pid; const char* name; };
static const SkuDefinition skus[] = {
    {0x0c06U, "D585F_3C"},
    {0x0c07U, "D585_2C_PROTO"},
    {0x0c08U, "D585_3C_PROTO"},
};
// One row per (branch, SKU); format is the branch default FourCC.
struct BranchDefinition { uint8_t id, sensor; uint16_t sku; uint32_t format; const char* name; };
static const BranchDefinition branches[] = {
    {1U, 1U, 3078U, 0x2036315aU, "DEPTH"},
    {1U, 1U, 3079U, 0x2036315aU, "DEPTH"},
    {1U, 1U, 3080U, 0x2036315aU, "DEPTH"},
    {4U, 1U, 3078U, 0x20493859U, "IR_LR"},
    {4U, 1U, 3079U, 0x20493859U, "IR_LR"},
    {4U, 1U, 3080U, 0x20493859U, "IR_LR"},
    {5U, 1U, 3079U, 0x3231564eU, "RGB_L"},
    {6U, 1U, 3079U, 0x3231564eU, "RGB_R"},
    {7U, 2U, 3078U, 0x32595559U, "RGB"},
    {7U, 2U, 3080U, 0x32595559U, "RGB"},
};
struct FormatDefinition { uint8_t branch; uint32_t format; };
static const FormatDefinition formats[] = {
    {1U, 0x2036315aU},
    {4U, 0x20493859U},
    {5U, 0x3231564eU},
    {6U, 0x3231564eU},
    {7U, 0x32595559U},
    {7U, 0x3231564eU},
};
// C1-owned rule content; consumed by IntentRules.cpp, not by the codec.
struct ProfileDefinition { uint16_t width, height, fps; };
static const ProfileDefinition profiles[] = {
    {1280U, 720U, 30U},
};
} // namespace rsintent
