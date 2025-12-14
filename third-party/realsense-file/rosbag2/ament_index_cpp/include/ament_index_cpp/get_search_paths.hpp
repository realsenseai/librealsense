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

#ifndef AMENT_INDEX_CPP__GET_SEARCH_PATHS_HPP_
#define AMENT_INDEX_CPP__GET_SEARCH_PATHS_HPP_

#include <list>
#include <string>

#include "ament_index_cpp/visibility_control.h"

namespace ament_index_cpp
{
/// Get the searcheable paths in the environment variable AMENT_PREFIX_PATH.
/**
 * \return a list with the searcheable paths
 * \throws runtime_error if the environment variable AMENT_PREFIX_PATH is not set
 */
AMENT_INDEX_CPP_PUBLIC
std::list<std::string>
get_search_paths();

}  // namespace ament_index_cpp

#endif  // AMENT_INDEX_CPP__GET_SEARCH_PATHS_HPP_
