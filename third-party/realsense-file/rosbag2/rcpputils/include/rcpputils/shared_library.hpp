// Copyright 2020 Open Source Robotics Foundation, Inc.
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

#ifndef RCPPUTILS__SHARED_LIBRARY_HPP_
#define RCPPUTILS__SHARED_LIBRARY_HPP_

#include <string>
#include <stdexcept>

#include "rcutils/shared_library.h"
#include "rcpputils/visibility_control.hpp"

namespace rcpputils
{

/**
 * This class is an abstraction of rcutils shared library to be able to used it
 *  with modern C++.
 **/
class SharedLibrary
{
public:
  /// The library is loaded in the constructor.
  /**
   * \param[in] library_path The library string path.
   * \throws std::bad_alloc if allocating storage for the callback fails
   * \throws std::runtime_error if there are some invalid arguments or the library
   * was not load properly
   */
  RCPPUTILS_PUBLIC
  explicit SharedLibrary(const std::string & library_path);

  /// The library is unloaded in the deconstructor
  RCPPUTILS_PUBLIC
  virtual ~SharedLibrary();

  /// Unload library
  /**
  * \throws std::runtime_error if the library is not unloaded properly
   */
  RCPPUTILS_PUBLIC
  void
  unload_library();

  /// Return true if the shared library contains a specific symbol name otherwise returns false.
  /**
   * \param[in] symbol_name name of the symbol inside the shared library
   * \return if symbols exists returns true, otherwise returns false.
   */
  RCPPUTILS_PUBLIC
  bool
  has_symbol(const std::string & symbol_name);

  /// Return shared library symbol pointer.
  /**
   * \param[in] symbol_name name of the symbol inside the shared library
   * \return shared library symbol pointer, if the symbol doesn't exist then throws a
   * runtime_error exception
   * \throws std::runtime_error if the symbol doesn't exist in the shared library
   */
  RCPPUTILS_PUBLIC
  void *
  get_symbol(const std::string & symbol_name);

  /// Return shared library path
  /**
   * \return shared library path or it throws an std::runtime_error if it's not defined
   * \throws std::runtime_error if the path is NULL
   */
  RCPPUTILS_PUBLIC
  std::string
  get_library_path();

private:
  rcutils_shared_library_t lib;
};

/// Get the platform specific library name
/**
 * The maximum file name size is 1024 characters, if the input library_name is bigger than
 * this value then the method throws an exception.
 *
 * \param[in] library_name library base name (without prefix and extension)
 * \param[in] debug if true the library will return a debug library name, otherwise
 * it returns a normal library path
 * \return platform specific library name
 * \throws std::runtime_error if it's not able to create the library name
 */
RCPPUTILS_PUBLIC
std::string get_platform_library_name(std::string library_name, bool debug = false);

}  // namespace rcpputils

#endif  // RCPPUTILS__SHARED_LIBRARY_HPP_
