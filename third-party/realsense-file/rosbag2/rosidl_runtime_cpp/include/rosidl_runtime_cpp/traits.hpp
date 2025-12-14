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

#ifndef ROSIDL_RUNTIME_CPP__TRAITS_HPP_
#define ROSIDL_RUNTIME_CPP__TRAITS_HPP_

#include <type_traits>

namespace rosidl_generator_traits
{

template<typename T>
inline const char * data_type();

template<typename T>
inline const char * name();

template<typename T>
struct has_fixed_size : std::false_type {};

template<typename T>
struct has_bounded_size : std::false_type {};

template<typename T>
struct is_message : std::false_type {};

template<typename T>
struct is_service : std::false_type {};

template<typename T>
struct is_service_request : std::false_type {};

template<typename T>
struct is_service_response : std::false_type {};

template<typename T>
struct is_action : std::false_type {};

template<typename T>
struct is_action_goal : std::false_type {};

template<typename T>
struct is_action_result : std::false_type {};

template<typename T>
struct is_action_feedback : std::false_type {};

}  // namespace rosidl_generator_traits

#endif  // ROSIDL_RUNTIME_CPP__TRAITS_HPP_
