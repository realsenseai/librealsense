// License: Apache 2.0. Copyright (C) 2026 RealSense, Inc.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace librealsense {
std::vector<uint8_t> encode_intent_json(const std::string &text,
                                       const std::vector<uint8_t> &status = {});
std::string decode_intent_json(const std::vector<uint8_t> &wire,
                               const std::vector<uint8_t> &status);
} // namespace librealsense
