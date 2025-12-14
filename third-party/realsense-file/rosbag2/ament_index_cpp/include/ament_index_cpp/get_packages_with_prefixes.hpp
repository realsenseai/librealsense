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

#ifndef AMENT_INDEX_CPP__GET_PACKAGES_WITH_PREFIXES_HPP_
#define AMENT_INDEX_CPP__GET_PACKAGES_WITH_PREFIXES_HPP_

#include <map>
#include <string>

#include "ament_index_cpp/visibility_control.h"

namespace ament_index_cpp
{

/// Return a map of package names to their installation prefix.
AMENT_INDEX_CPP_PUBLIC
std::map<std::string, std::string>
get_packages_with_prefixes();

}  // namespace ament_index_cpp

#endif  // AMENT_INDEX_CPP__GET_PACKAGES_WITH_PREFIXES_HPP_
