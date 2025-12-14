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

#ifndef RCUTILS__CMDLINE_PARSER_H_
#define RCUTILS__CMDLINE_PARSER_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>

#include "rcutils/visibility_control.h"

/// Return `true` if the option is defined in the command line arguments or `false` otherwise.
/**
* \param[in] begin first element to check in the array
* \param[in] end last element to check in the array
* \param[in] option string to find in the array of arguments
* \return if the option exists returns true, otherwise returns false.
 */
RCUTILS_PUBLIC
bool
rcutils_cli_option_exist(char ** begin, char ** end, const char * option);

/// Return the value for a specific option of the command line arguments.
/**
* \param[in] begin first element to check in the array
* \param[in] end last element to check in the array
* \param[in] option string to find in the array of arguments
* \return the value for a specific option of the command line arguments or `NULL` if the option
* doesn't exist
 */
RCUTILS_PUBLIC
char *
rcutils_cli_get_option(char ** begin, char ** end, const char * option);

#ifdef __cplusplus
}
#endif

#endif  // RCUTILS__CMDLINE_PARSER_H_
