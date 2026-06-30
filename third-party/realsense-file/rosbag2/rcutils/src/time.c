// Copyright 2018 Open Source Robotics Foundation, Inc.
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

#if __cplusplus
extern "C"
{
#endif

#include "rcutils/time.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "rcutils/allocator.h"
#include "rcutils/error_handling.h"
#include "rcutils/snprintf.h"

rcutils_ret_t
rcutils_time_point_value_as_nanoseconds_string(
  const rcutils_time_point_value_t * time_point,
  char * str,
  size_t str_size)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(time_point, RCUTILS_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(str, RCUTILS_RET_INVALID_ARGUMENT);
  if (0 == str_size) {
    return RCUTILS_RET_OK;
  }
  if (rcutils_snprintf(str, str_size, "%.19" PRId64, *time_point) < 0) {
    RCUTILS_SET_ERROR_MSG("failed to format time point into string as nanoseconds");
    return RCUTILS_RET_ERROR;
  }
  return RCUTILS_RET_OK;
}

rcutils_ret_t
rcutils_time_point_value_as_date_string(
  const rcutils_time_point_value_t * time_point,
  char * str,
  size_t str_size)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(time_point, RCUTILS_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(str, RCUTILS_RET_INVALID_ARGUMENT);
  if (0 == str_size) {
    return RCUTILS_RET_OK;
  }
  // best to abs it to avoid issues with negative values in C89, see:
  //   https://stackoverflow.com/a/3604984/671658
  uint64_t abs_time_point = (uint64_t)llabs(*time_point);
  // break into two parts to avoid floating point error
  uint64_t seconds = abs_time_point / (1000u * 1000u * 1000u);
  uint64_t nanoseconds = abs_time_point % (1000u * 1000u * 1000u);
  // Make sure the buffer is large enough to hold the largest possible uint64_t
  char nanoseconds_str[21];

  if (rcutils_snprintf(nanoseconds_str, sizeof(nanoseconds_str), "%" PRIu64, nanoseconds) < 0) {
    RCUTILS_SET_ERROR_MSG("failed to format time point nanoseconds into string");
    return RCUTILS_RET_ERROR;
  }

  time_t now_t = (time_t)(seconds);
  struct tm ptm = {.tm_year = 0, .tm_mday = 0};
#ifdef _WIN32
  if (localtime_s(&ptm, &now_t) != 0) {
    RCUTILS_SET_ERROR_MSG("failed to get localtime");
    return RCUTILS_RET_ERROR;
  }
#else
  if (localtime_r(&now_t, &ptm) == NULL) {
    RCUTILS_SET_ERROR_MSG("failed to get localtime");
    return RCUTILS_RET_ERROR;
  }
#endif

  if (str_size < 32 || strftime(str, 32, "%Y-%m-%d %H:%M:%S", &ptm) == 0) {
    RCUTILS_SET_ERROR_MSG("failed to format time point into string as iso8601_date");
    return RCUTILS_RET_ERROR;
  }
  static const int date_end_position = 19;
  if (rcutils_snprintf(
      &str[date_end_position], str_size - date_end_position, ".%.3s",
      nanoseconds_str) < 0)
  {
    RCUTILS_SET_ERROR_MSG("failed to format time point into string as date_time_with_ms");
    return RCUTILS_RET_ERROR;
  }

  return RCUTILS_RET_OK;
}

rcutils_ret_t
rcutils_time_point_value_as_seconds_string(
  const rcutils_time_point_value_t * time_point,
  char * str,
  size_t str_size)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(time_point, RCUTILS_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(str, RCUTILS_RET_INVALID_ARGUMENT);
  if (0 == str_size) {
    return RCUTILS_RET_OK;
  }
  // best to abs it to avoid issues with negative values in C89, see:
  //   https://stackoverflow.com/a/3604984/671658
  uint64_t abs_time_point = (uint64_t)llabs(*time_point);
  // break into two parts to avoid floating point error
  uint64_t seconds = abs_time_point / (1000u * 1000u * 1000u);
  uint64_t nanoseconds = abs_time_point % (1000u * 1000u * 1000u);
  if (
    rcutils_snprintf(
      str, str_size, "%s%.10" PRId64 ".%.9" PRId64,
      (*time_point >= 0) ? "" : "-", seconds, nanoseconds) < 0)
  {
    RCUTILS_SET_ERROR_MSG("failed to format time point into string as float seconds");
    return RCUTILS_RET_ERROR;
  }
  return RCUTILS_RET_OK;
}

#if __cplusplus
}
#endif
