// Generated from dictionary.json; do not edit.
#pragma once
#include <stdint.h>
namespace rsintent {
static const char dictionary_sha256[] = "39af11d1e450ea064d20ac7f708f8006f746369318469e82409b1e6bd929025f";
static const unsigned major = 1U;
static const unsigned minor = 0U;
static const unsigned wire_size = 1024U;
static const unsigned rule_revision = 1U;
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
struct BranchDefinition { uint8_t id, sensor; uint16_t sku; uint32_t format; const char* name; };
static const BranchDefinition branches[] = {
    {1U, 1U, 3076U, 0x2036315aU, "DEPTH"},
    {1U, 1U, 3077U, 0x2036315aU, "DEPTH"},
    {2U, 1U, 3076U, 0x20203859U, "IR_L"},
    {2U, 1U, 3077U, 0x20203859U, "IR_L"},
    {4U, 1U, 3076U, 0x20493859U, "IR_LR"},
    {4U, 1U, 3077U, 0x20493859U, "IR_LR"},
    {5U, 1U, 3076U, 0x3231564eU, "RGB_L"},
    {6U, 1U, 3076U, 0x3231564eU, "RGB_R"},
    {7U, 2U, 3077U, 0x32595559U, "RGB"},
};
struct ProfileDefinition { uint16_t width, height, fps; };
static const ProfileDefinition profiles[] = {
    {1280U, 720U, 30U},
};
} // namespace rsintent
