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

/** \mainpage ament_index_cpp: C++ API to access the ament resource index.
 *
 * This package contains functions to operate with the resource index
 *
 * - Get installation prefix of an installed package
 *   - get_package_prefix()
 *   - ament_index_cpp/get_package_prefix.hpp
 * - Get share directory of an installed package
 *   - get_package_share_directory()
 *   - ament_index_cpp/get_package_share_directory.hpp
 * - Get a map of package names to their installation prefix
 *   - get_packages_with_prefixes()
 *   - ament_index_cpp/get_packages_with_prefixes.hpp
 * - Get the content and path of a resource
 *   - get_resource()
 *   - ament_index_cpp/get_resource.hpp
 * - Get a map of resources based on a resource type
 *   - get_resources()
 *   - ament_index_cpp/get_resources.hpp
 * - Check if resource exists and get its path
 *   - has_resource()
 *   - ament_index_cpp/has_resource.hpp
 * - Macros for controlling symbol visibility on the library
 *   - ament_index_cpp/visibility_control.h
 */

#ifndef AMENT_INDEX_CPP__GET_RESOURCE_HPP_
#define AMENT_INDEX_CPP__GET_RESOURCE_HPP_

#include <string>

#include "ament_index_cpp/visibility_control.h"

namespace ament_index_cpp
{
/// Get a the content and path of a resource
/**
 * \param[in] resource_type type of the resource
 * \param[in] resource_name name of the resource
 * \param[out] content content of the resource
 * \param[out] prefix_path the installation prefix of the given resource if found.
 * \return `true` is the resource exists, `false` otherwise.
 * \throws std::runtime_error if resource_type or resource_name are empty.
 */
AMENT_INDEX_CPP_PUBLIC
bool
get_resource(
  const std::string & resource_type,
  const std::string & resource_name,
  std::string & content,
  std::string * prefix_path = nullptr);

}  // namespace ament_index_cpp

#endif  // AMENT_INDEX_CPP__GET_RESOURCE_HPP_
