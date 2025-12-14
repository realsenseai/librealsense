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

#ifndef AMENT_INDEX_CPP__GET_PACKAGE_PREFIX_HPP_
#define AMENT_INDEX_CPP__GET_PACKAGE_PREFIX_HPP_

#include <stdexcept>
#include <string>

#include "ament_index_cpp/visibility_control.h"

namespace ament_index_cpp
{

/// Thrown when a package is not found.
class PackageNotFoundError : public std::out_of_range
{
public:
  /// Constuctor of an exception class to notify that a pacakge is not found
  /*
  * \param[in] package_name used in the exception message
  */
  AMENT_INDEX_CPP_PUBLIC
  explicit PackageNotFoundError(const std::string & package_name);

  AMENT_INDEX_CPP_PUBLIC
  virtual ~PackageNotFoundError();

  /// package name used in the exception message
  const std::string package_name;
};

/// Return the installation prefix of the given package if found.
/**
 * \param[in] package_name the name of the package to locate.
 * \return installation prefix path in which the package was found.
 * \throws PackageNotFoundError when the given package is not found.
 */
AMENT_INDEX_CPP_PUBLIC
std::string
get_package_prefix(const std::string & package_name);

}  // namespace ament_index_cpp

#endif  // AMENT_INDEX_CPP__GET_PACKAGE_PREFIX_HPP_
