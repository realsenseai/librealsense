// License: Apache 2.0. Copyright (C) 2026 RealSense, Inc.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace librealsense {
// status: FW GLOBAL_INTENT_STATUS; items newer than its minor are omitted with a warning.
// firmware: FW GLOBAL_INTENT GET_CUR. Sensors and branches it does not list are omitted
// with a warning (Light D, design v0.4 section 7.10, HR-4).
std::vector<uint8_t> encode_intent_json(const std::string &text,
                                       const std::vector<uint8_t> &status = {},
                                       const std::vector<uint8_t> &firmware = {});
std::string decode_intent_json(const std::vector<uint8_t> &wire,
                               const std::vector<uint8_t> &status);
// Names last_result and every REJECT_REASON of a GLOBAL_INTENT_STATUS transfer (design 9.6).
std::string describe_intent_status(const std::vector<uint8_t> &status);
class d500_depth_sensor;
class device_interface;
void verify_intent_before_start(device_interface &device);
// Empty unless the device STATUS holds a stream-admission rejection.
std::string intent_rejection(device_interface &device) noexcept;
// Call only from a catch block: rethrows, appending intent_rejection() to backend errors.
[[noreturn]] void rethrow_with_intent_rejection(device_interface &device);
void register_intent_options(d500_depth_sensor &sensor);
} // namespace librealsense
