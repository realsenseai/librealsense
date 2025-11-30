// Copyright 2017 Open Source Robotics Foundation, Inc.
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

#include "ament_index_cpp/get_package_prefix.hpp"

#include <stdexcept>
#include <string>

#include "ament_index_cpp/get_resource.hpp"
#include "ament_index_cpp/get_search_paths.hpp"

namespace ament_index_cpp
{

static
std::string
format_package_not_found_error_message(const std::string & package_name)
{
  std::string message = "package '" + package_name + "' not found, searching: [";
  auto search_paths = get_search_paths();
  for (const auto & path : search_paths) {
    message += path + ", ";
  }
  if (search_paths.size() > 0) {
    message = message.substr(0, message.size() - 2);
  }
  return message + "]";
}

PackageNotFoundError::PackageNotFoundError(const std::string & _package_name)
: std::out_of_range(format_package_not_found_error_message(_package_name)),
  package_name(_package_name)
{}

PackageNotFoundError::~PackageNotFoundError() {}

std::string
get_package_prefix(const std::string & package_name)
{
  std::string content;
  std::string prefix_path;
  if (!get_resource("packages", package_name, content, &prefix_path)) {
    throw PackageNotFoundError(package_name);
  }
  return prefix_path;
}

}  // namespace ament_index_cpp
