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

#ifndef ROSIDL_TYPESUPPORT_CPP__ACTION_TYPE_SUPPORT_HPP_
#define ROSIDL_TYPESUPPORT_CPP__ACTION_TYPE_SUPPORT_HPP_

#include <rosidl_runtime_c/action_type_support_struct.h>
#include <rosidl_runtime_c/visibility_control.h>

namespace rosidl_typesupport_cpp
{

template<typename T>
const rosidl_action_type_support_t * get_action_type_support_handle();

}  // namespace rosidl_typesupport_cpp

#endif  // ROSIDL_TYPESUPPORT_CPP__ACTION_TYPE_SUPPORT_HPP_
