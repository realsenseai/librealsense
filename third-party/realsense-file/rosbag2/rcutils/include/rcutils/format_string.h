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

#ifndef RCUTILS__FORMAT_STRING_H_
#define RCUTILS__FORMAT_STRING_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <string.h>

#include "rcutils/allocator.h"
#include "rcutils/macros.h"
#include "rcutils/visibility_control.h"

/// Return a newly allocated string, created with a format string.
/**
 * This function is identical to rcutils_format_string_limit() except it has an
 * implicit limit of 2048.
 * For longer format strings, see rcutils_format_string_limit().
 */
#define rcutils_format_string(allocator, format_string, ...) \
  rcutils_format_string_limit(allocator, 2048, format_string, __VA_ARGS__)

/// Return a newly allocated string, created with a format string up to a limit.
/**
 * This function uses snprintf_s to determine the length of the resulting
 * string and allocates storage for the resulting string, formats it, and
 * then returns the result.
 *
 * This function can fail and therefore return null if the format_string is
 * null or if memory allocation fails or if snprintf_s fails.
 * An error message is not set in any case.
 *
 * Output strings that would be longer than the given limit are truncated.
 *
 * All returned strings are null terminated.
 *
 * The format string is passed to snprintf_s(), see its documentation for
 * how to use the format string.
 *
 * The returned string must be deallocated using the same allocator given once
 * it is no longer needed.
 *
 * \see rcutils_snprintf()
 *
 * \param[in] allocator the allocator to use for allocation
 * \param[in] limit maximum length of the output string
 * \param[in] format_string format of the output, must be null terminated
 * \returns output string or null if there was an error
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
char *
rcutils_format_string_limit(
  rcutils_allocator_t allocator,
  size_t limit,
  const char * format_string,
  ...)
RCUTILS_ATTRIBUTE_PRINTF_FORMAT(3, 4);

#ifdef __cplusplus
}
#endif

#endif  // RCUTILS__FORMAT_STRING_H_
