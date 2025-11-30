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

#ifndef ROSBAG2_STORAGE__ROS_HELPER_HPP_
#define ROSBAG2_STORAGE__ROS_HELPER_HPP_

#include <memory>

#include "rcutils/types/uint8_array.h"

#include "rosbag2_storage/visibility_control.hpp"

namespace rosbag2_storage
{

ROSBAG2_STORAGE_PUBLIC
std::shared_ptr<rcutils_uint8_array_t>
make_serialized_message(const void * data, size_t size);

ROSBAG2_STORAGE_PUBLIC
std::shared_ptr<rcutils_uint8_array_t>
make_empty_serialized_message(size_t size);

}  // namespace rosbag2_storage

#endif  // ROSBAG2_STORAGE__ROS_HELPER_HPP_
