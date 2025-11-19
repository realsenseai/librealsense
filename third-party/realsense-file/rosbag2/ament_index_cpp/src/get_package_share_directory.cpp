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

#include "ament_index_cpp/get_package_share_directory.hpp"

#include <string>

#include "ament_index_cpp/get_package_prefix.hpp"

namespace ament_index_cpp
{

std::string
get_package_share_directory(const std::string & package_name)
{
  return get_package_prefix(package_name) + "/share/" + package_name;
}

}  // namespace ament_index_cpp
