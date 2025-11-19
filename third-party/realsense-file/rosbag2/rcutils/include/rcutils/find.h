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

#ifndef RCUTILS__FIND_H_
#define RCUTILS__FIND_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "rcutils/types.h"
#include "rcutils/visibility_control.h"

/// Return the first index of a character in a string.
/**
 * Search in a string for the first occurence of a delimiter.
 *
 * \param[in] str null terminated c string to search
 * \param[in] delimiter the character to search for
 * \returns the index of the first occurence of the delimiter if found, or
 * \returns `SIZE_MAX` for invalid arguments or if the delimiter is not found
 */
RCUTILS_PUBLIC
size_t
rcutils_find(const char * str, char delimiter);

/// Return the first index of a character in a string of specified length.
/**
 * Identical to rcutils_find_first() but without relying on the string to be a
 * null terminated c string.
 *
 * \param[in] str string to search
 * \param[in] delimiter the character to search for
 * \param[in] string_length length of the string to search
 * \returns the index of the first occurence of the delimiter if found, or
 * \returns `SIZE_MAX` for invalid arguments or if the delimiter is not found
 */
RCUTILS_PUBLIC
size_t
rcutils_findn(const char * str, char delimiter, size_t string_length);

/// Return the last index of a character in a string.
/**
 * Search in a string for the last occurence of a delimiter.
 *
 * \param[in] str null terminated c string to search
 * \param[in] delimiter the character to search for
 * \returns the index of the last occurence of the delimiter if found, or
 * \returns `SIZE_MAX` for invalid arguments or if the delimiter is not found
 */
RCUTILS_PUBLIC
size_t
rcutils_find_last(const char * str, char delimiter);

/// Return the last index of a character in a string of specifed length.
/**
 * Identical to rcutils_find_last() but without relying on the string to be a
 * null terminated c string.
 *
 * \param[in] str string to search
 * \param[in] delimiter the character to search for
 * \param[in] string_length length of the string to search
 * \returns the index of the last occurence of the delimiter if found, or
 * \returns `SIZE_MAX` for invalid arguments or if the delimiter is not found
 */
RCUTILS_PUBLIC
size_t
rcutils_find_lastn(const char * str, char delimiter, size_t string_length);

#ifdef __cplusplus
}
#endif

#endif  // RCUTILS__FIND_H_
