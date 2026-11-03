// License: Apache 2.0. Copyright (C) 2026 RealSense, Inc.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace librealsense {
std::vector<uint8_t> encode_intent_json(const std::string &text);
std::string decode_intent_json(const std::vector<uint8_t> &wire,
                               const std::vector<uint8_t> &status);
class d500_depth_sensor;
class device_interface;
void verify_intent_before_start(device_interface &device);
void register_intent_options(d500_depth_sensor &sensor);
} // namespace librealsense
