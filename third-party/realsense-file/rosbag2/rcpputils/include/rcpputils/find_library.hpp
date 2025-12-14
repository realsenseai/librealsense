// Copyright 2019 Open Source Robotics Foundation, Inc.
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

/*! \file find_library.hpp
 * \brief Find library located in OS's library paths environment variable.
 */

#ifndef RCPPUTILS__FIND_LIBRARY_HPP_
#define RCPPUTILS__FIND_LIBRARY_HPP_

#include <cerrno>
#include <string>

#include "rcpputils/visibility_control.hpp"

namespace rcpputils
{

/// Find a library located in the OS's specified environment variable for library paths.
/**
 *
 * The environment variable and file format per platform:
 *  * Linux: `${LD_LIBRARY_PATH}`, `lib{}.so`
 *  * Apple: `${DYLD_LIBRARY_PATH}`, `lib{}.dyld`
 *  * Windows: `%PATH%`, `{}.dll`
 *
 * \param[in] library_name Name of the library to find.
 * \return The absolute filesystem path, including the appropriate prefix and extension
 * \throws std::runtime_error if an error is encountered when accessing environment variables.
 */
RCPPUTILS_PUBLIC
std::string find_library_path(const std::string & library_name);

}  // namespace rcpputils

#endif  // RCPPUTILS__FIND_LIBRARY_HPP_
