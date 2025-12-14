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

#ifndef ROSBAG2_STORAGE__TOPIC_METADATA_HPP_
#define ROSBAG2_STORAGE__TOPIC_METADATA_HPP_

#include <string>

namespace rosbag2_storage
{

struct TopicMetadata
{
  std::string name;
  std::string type;
  std::string serialization_format;
  // Serialized std::vector<rclcpp::QoS> as a YAML string
  std::string offered_qos_profiles;

  bool operator==(const rosbag2_storage::TopicMetadata & rhs) const
  {
    return name == rhs.name && type == rhs.type && serialization_format == rhs.serialization_format;
  }
};

}  // namespace rosbag2_storage

#endif  // ROSBAG2_STORAGE__TOPIC_METADATA_HPP_
