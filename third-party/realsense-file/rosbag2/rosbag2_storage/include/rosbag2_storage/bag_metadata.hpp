// Copyright 2018, Bosch Software Innovations GmbH.
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

#ifndef ROSBAG2_STORAGE__BAG_METADATA_HPP_
#define ROSBAG2_STORAGE__BAG_METADATA_HPP_

#include <chrono>
#include <string>
#include <vector>
#include <utility>

#include "rosbag2_storage/topic_metadata.hpp"

namespace rosbag2_storage
{

struct TopicInformation
{
  TopicMetadata topic_metadata;
  size_t message_count;
};

struct BagMetadata
{
  int version = 4;  // upgrade this number when changing the content of the struct
  uint64_t bag_size = 0;  // Will not be serialized
  std::string storage_identifier;
  std::vector<std::string> relative_file_paths;
  std::chrono::nanoseconds duration;
  std::chrono::time_point<std::chrono::high_resolution_clock> starting_time;
  uint64_t message_count;
  std::vector<TopicInformation> topics_with_message_count;
  std::string compression_format;
  std::string compression_mode;
};

}  // namespace rosbag2_storage

#endif  // ROSBAG2_STORAGE__BAG_METADATA_HPP_
