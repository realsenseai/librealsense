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

#include "ament_index_cpp/get_resource.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "ament_index_cpp/get_search_paths.hpp"

namespace ament_index_cpp
{

bool
get_resource(
  const std::string & resource_type,
  const std::string & resource_name,
  std::string & content,
  std::string * prefix_path)
{
  if (resource_type.empty()) {
    throw std::runtime_error("ament_index_cpp::get_resource() resource type must not be empty");
  }
  if (resource_name.empty()) {
    throw std::runtime_error("ament_index_cpp::get_resource() resource name must not be empty");
  }
  auto paths = get_search_paths();
  for (auto path : paths) {
    auto resource_path = path + "/share/ament_index/resource_index/" +
      resource_type + "/" + resource_name;
    std::ifstream s(resource_path);
    if (s.is_open()) {
      std::stringstream buffer;
      buffer << s.rdbuf();
      content = buffer.str();
      if (prefix_path) {
        *prefix_path = path;
      }
      return true;
    }
  }
  return false;
}

}  // namespace ament_index_cpp
