// Copyright 2018 Open Source Robotics Foundation, Inc.
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

#ifndef ROSBAG2_STORAGE__STORAGE_INTERFACES__BASE_WRITE_INTERFACE_HPP_
#define ROSBAG2_STORAGE__STORAGE_INTERFACES__BASE_WRITE_INTERFACE_HPP_

#include <memory>
#include <string>
#include <vector>

#include "rosbag2_storage/serialized_bag_message.hpp"
#include "rosbag2_storage/bag_metadata.hpp"
#include "rosbag2_storage/topic_metadata.hpp"
#include "rosbag2_storage/visibility_control.hpp"

namespace rosbag2_storage
{
namespace storage_interfaces
{

class ROSBAG2_STORAGE_PUBLIC BaseWriteInterface
{
public:
  virtual ~BaseWriteInterface() = default;

  virtual void write(std::shared_ptr<const SerializedBagMessage> msg) = 0;

  virtual void write(const std::vector<std::shared_ptr<const SerializedBagMessage>> & msg) = 0;

  virtual void create_topic(const TopicMetadata & topic) = 0;

  virtual void remove_topic(const TopicMetadata & topic) = 0;
};

}  // namespace storage_interfaces
}  // namespace rosbag2_storage

#endif  // ROSBAG2_STORAGE__STORAGE_INTERFACES__BASE_WRITE_INTERFACE_HPP_
