// Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

#ifndef ROSBAG2_COMPRESSION__ZSTD_DECOMPRESSOR_HPP_
#define ROSBAG2_COMPRESSION__ZSTD_DECOMPRESSOR_HPP_

#include <zstd.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "rosbag2_compression/base_decompressor_interface.hpp"
#include "rosbag2_compression/visibility_control.hpp"

namespace rosbag2_compression
{

/**
 * A BaseDecompressorInterface that is used to decompress bagfiles stored using ZStandard compression.
 *
 * ZstdDecompressor should only be initialized by Reader.
 */
class ROSBAG2_COMPRESSION_PUBLIC ZstdDecompressor : public BaseDecompressorInterface
{
public:
  ZstdDecompressor() = default;

  ~ZstdDecompressor() = default;

  std::string decompress_uri(const std::string & uri) override;

  void decompress_serialized_bag_message(
    rosbag2_storage::SerializedBagMessage * bag_message) override;

  std::string get_decompression_identifier() const override;
};

}  // namespace rosbag2_compression

#endif  // ROSBAG2_COMPRESSION__ZSTD_DECOMPRESSOR_HPP_
