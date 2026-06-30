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

#ifndef RCUTILS__SPLIT_H_
#define RCUTILS__SPLIT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "rcutils/allocator.h"
#include "rcutils/types.h"
#include "rcutils/visibility_control.h"

/// Split a given string with the specified delimiter
/**
 * \param[in] str string to split
 * \param[in] delimiter on where to split
 * \param[in] allocator for allocating new memory for the output array
 * \param[out] string_array with the split tokens
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_INVALID_ARGUMENT` for invalid arguments, or
 * \return `RCUTILS_RET_BAD_ALLOC` if memory allocation fails, or
 * \return `RCUTILS_RET_ERROR` if an unknown error occurs
 */
RCUTILS_PUBLIC
rcutils_ret_t
rcutils_split(
  const char * str,
  char delimiter,
  rcutils_allocator_t allocator,
  rcutils_string_array_t * string_array);

/// Split a given string on the last occurrence of the specified delimiter
/**
 * \param[in] str string to split
 * \param[in] delimiter on where to split
 * \param[in] allocator for allocating new memory for the output array
 * \param[out] string_array with the split tokens
 * \returns array with split token, NULL in case of error
 */
RCUTILS_PUBLIC
rcutils_ret_t
rcutils_split_last(
  const char * str,
  char delimiter,
  rcutils_allocator_t allocator,
  rcutils_string_array_t * string_array);

#ifdef __cplusplus
}
#endif

#endif  // RCUTILS__SPLIT_H_
