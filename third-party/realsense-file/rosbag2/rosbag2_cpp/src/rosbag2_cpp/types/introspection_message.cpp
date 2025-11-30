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

#include "rosbag2_cpp/types/introspection_message.hpp"

#include <memory>
#include <string>
#include <vector>

#include "rcutils/allocator.h"
#include "rcutils/strdup.h"
#include "rcutils/time.h"

#include "rosidl_typesupport_cpp/message_type_support.hpp"

#include "rosidl_typesupport_introspection_cpp/field_types.hpp"
#include "rosidl_typesupport_introspection_cpp/message_introspection.hpp"

namespace rosbag2_cpp
{

std::shared_ptr<rosbag2_introspection_message_t>
allocate_introspection_message(
  const rosidl_message_type_support_t * introspection_ts, const rcutils_allocator_t * allocator)
{
  auto intro_ts_members = static_cast<const rosidl_typesupport_introspection_cpp::MessageMembers *>(
    introspection_ts->data);
  auto raw_ros2_message = new rosbag2_introspection_message_t();
  raw_ros2_message->allocator = *allocator;
  raw_ros2_message->topic_name = nullptr;
  raw_ros2_message->message = raw_ros2_message->allocator.zero_allocate(
    1, intro_ts_members->size_of_, raw_ros2_message->allocator.state);
  intro_ts_members->init_function(
    raw_ros2_message->message, rosidl_runtime_cpp::MessageInitialization::ALL);

  auto deleter = [introspection_ts](rosbag2_introspection_message_t * msg) {
      deallocate_introspection_message(msg, introspection_ts);
    };
  return std::shared_ptr<rosbag2_introspection_message_t>(raw_ros2_message, deleter);
}

void introspection_message_set_topic_name(
  rosbag2_introspection_message_t * msg, const char * topic_name)
{
  if (msg->topic_name) {
    msg->allocator.deallocate(msg->topic_name, msg->allocator.state);
    msg->topic_name = nullptr;
  }
  msg->topic_name = rcutils_strdup(topic_name, msg->allocator);
}

void deallocate_introspection_message(
  rosbag2_introspection_message_t * msg,
  const rosidl_message_type_support_t * introspection_ts)
{
  auto intro_ts_members = static_cast<const rosidl_typesupport_introspection_cpp::MessageMembers *>(
    introspection_ts->data);
  intro_ts_members->fini_function(msg->message);
  msg->allocator.deallocate(msg->message, msg->allocator.state);
  msg->allocator.deallocate(msg->topic_name, msg->allocator.state);
  delete msg;
}

}  // namespace rosbag2_cpp
