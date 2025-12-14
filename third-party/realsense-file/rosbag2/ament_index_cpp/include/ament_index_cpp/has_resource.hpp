// Copyright 2015 Open Source Robotics Foundation, Inc.
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

#ifndef AMENT_INDEX_CPP__HAS_RESOURCE_HPP_
#define AMENT_INDEX_CPP__HAS_RESOURCE_HPP_

#include <string>

#include "ament_index_cpp/visibility_control.h"

namespace ament_index_cpp
{

/// Check if resource exists and get its path
/**
 * \param[in] resource_type type of the resource
 * \param[in] resource_name name of the resource
 * \param[out] prefix_path the installation prefix of the given resource if found.
 * \return `true` if resource exists,`false` otherwise
 * \throws std::runtime_error if resource_type or resource_name are empty
 */
AMENT_INDEX_CPP_PUBLIC
bool
has_resource(
  const std::string & resource_type,
  const std::string & resource_name,
  std::string * prefix_path = nullptr);

}  // namespace ament_index_cpp

#endif  // AMENT_INDEX_CPP__HAS_RESOURCE_HPP_
