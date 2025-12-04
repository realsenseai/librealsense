// Copyright 2019 Open Source Robotics Foundation, Inc.
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

#ifndef ROSBAG2_CPP__WRITER_INTERFACES__BASE_WRITER_INTERFACE_HPP_
#define ROSBAG2_CPP__WRITER_INTERFACES__BASE_WRITER_INTERFACE_HPP_

#include <memory>

#include "rosbag2_cpp/converter_options.hpp"
#include "rosbag2_cpp/storage_options.hpp"
#include "rosbag2_cpp/visibility_control.hpp"

#include "rosbag2_storage/serialized_bag_message.hpp"
#include "rosbag2_storage/topic_metadata.hpp"

namespace rosbag2_cpp
{
namespace writer_interfaces
{

class ROSBAG2_CPP_PUBLIC BaseWriterInterface
{
public:
  virtual ~BaseWriterInterface() {}

  virtual void open(
    const StorageOptions & storage_options, const ConverterOptions & converter_options) = 0;

  virtual void reset() = 0;

  virtual void create_topic(const rosbag2_storage::TopicMetadata & topic_with_type) = 0;

  virtual void remove_topic(const rosbag2_storage::TopicMetadata & topic_with_type) = 0;

  virtual void write(std::shared_ptr<rosbag2_storage::SerializedBagMessage> message) = 0;
};

}  // namespace writer_interfaces
}  // namespace rosbag2_cpp

#endif  // ROSBAG2_CPP__WRITER_INTERFACES__BASE_WRITER_INTERFACE_HPP_
