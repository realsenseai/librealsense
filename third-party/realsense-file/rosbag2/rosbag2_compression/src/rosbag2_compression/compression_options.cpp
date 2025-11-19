// Copyright 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <algorithm>
#include <string>

#include "rosbag2_compression/compression_options.hpp"
#include "logging.hpp"

namespace rosbag2_compression
{

namespace
{

constexpr const char kCompressionModeNoneStr[] = "NONE";
constexpr const char kCompressionModeFileStr[] = "FILE";
constexpr const char kCompressionModeMessageStr[] = "MESSAGE";

std::string to_upper(const std::string & text)
{
  std::string uppercase_text = text;
  std::transform(uppercase_text.begin(), uppercase_text.end(), uppercase_text.begin(), ::toupper);
  return uppercase_text;
}
}  // namespace

CompressionMode compression_mode_from_string(const std::string & compression_mode)
{
  const auto compression_mode_upper = to_upper(compression_mode);
  if (compression_mode.empty() || compression_mode_upper == kCompressionModeNoneStr) {
    return CompressionMode::NONE;
  } else if (compression_mode_upper == kCompressionModeFileStr) {
    return CompressionMode::FILE;
  } else if (compression_mode_upper == kCompressionModeMessageStr) {
    return CompressionMode::MESSAGE;
  } else {
    ROSBAG2_COMPRESSION_LOG_ERROR_STREAM(
      "CompressionMode: \"" << compression_mode << "\" is not supported!");
    return CompressionMode::NONE;
  }
}

std::string compression_mode_to_string(const CompressionMode compression_mode)
{
  switch (compression_mode) {
    case CompressionMode::NONE:
      return kCompressionModeNoneStr;
    case CompressionMode::FILE:
      return kCompressionModeFileStr;
    case CompressionMode::MESSAGE:
      return kCompressionModeMessageStr;
    default:
      ROSBAG2_COMPRESSION_LOG_ERROR_STREAM("CompressionMode not supported!");
      return kCompressionModeNoneStr;
  }
}
}  // namespace rosbag2_compression
