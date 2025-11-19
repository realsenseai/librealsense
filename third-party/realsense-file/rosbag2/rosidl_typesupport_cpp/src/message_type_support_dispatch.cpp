// Copyright 2016 Open Source Robotics Foundation, Inc.
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

#include "rosidl_typesupport_cpp/message_type_support_dispatch.hpp"

#include "type_support_dispatch.hpp"

namespace rosidl_typesupport_cpp
{

const rosidl_message_type_support_t *
get_message_typesupport_handle_function(
  const rosidl_message_type_support_t * handle, const char * identifier)
{
  return rosidl_typesupport_cpp::get_typesupport_handle_function<
    rosidl_message_type_support_t>(handle, identifier);
}

}  // namespace rosidl_typesupport_cpp
