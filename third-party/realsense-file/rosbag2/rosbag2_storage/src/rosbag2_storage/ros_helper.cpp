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

#include "rosbag2_storage/ros_helper.hpp"

#include <memory>
#include <string>

#include "rcutils/types.h"
#include "rosbag2_storage/logging.hpp"

namespace rosbag2_storage
{

static rcutils_allocator_t allocator = rcutils_get_default_allocator();

std::shared_ptr<rcutils_uint8_array_t>
make_serialized_message(const void * data, size_t size)
{
  auto serialized_message = make_empty_serialized_message(size);
  memcpy(serialized_message->buffer, data, size);
  serialized_message->buffer_length = size;

  return serialized_message;
}

std::shared_ptr<rcutils_uint8_array_t>
make_empty_serialized_message(size_t size)
{
  auto msg = new rcutils_uint8_array_t;
  *msg = rcutils_get_zero_initialized_uint8_array();
  auto ret = rcutils_uint8_array_init(msg, size, &allocator);
  if (ret != RCUTILS_RET_OK) {
    throw std::runtime_error(
            "Error allocating resources for serialized message: " +
            std::string(rcutils_get_error_string().str));
  }

  auto serialized_message = std::shared_ptr<rcutils_uint8_array_t>(
    msg,
    [](rcutils_uint8_array_t * msg) {
      int error = rcutils_uint8_array_fini(msg);
      delete msg;
      if (error != RCUTILS_RET_OK) {
        ROSBAG2_STORAGE_LOG_ERROR_STREAM(
          "Leaking memory. Error: " << rcutils_get_error_string().str);
      }
    });

  return serialized_message;
}

}  // namespace rosbag2_storage
