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

#ifdef __cplusplus
extern "C"
{
#endif

#include "rcutils/snprintf.h"

#include <errno.h>
#ifdef _WIN32
#include <string.h>
#endif
#include <stdarg.h>
#include <stdio.h>

int
rcutils_snprintf(char * buffer, size_t buffer_size, const char * format, ...)
{
  va_list args;
  va_start(args, format);
  int ret = rcutils_vsnprintf(buffer, buffer_size, format, args);
  va_end(args);
  return ret;
}

int
rcutils_vsnprintf(char * buffer, size_t buffer_size, const char * format, va_list args)
{
  RCUTILS_CAN_FAIL_WITH({errno = EINVAL; return -1;});

  if (NULL == format) {
    errno = EINVAL;
    return -1;
  }
  if (NULL == buffer && 0 == buffer_size) {
#ifndef _WIN32
    return vsnprintf(NULL, 0, format, args);
#else
    return _vscprintf(format, args);
#endif
  }
  if (NULL == buffer || 0 == buffer_size) {
    errno = EINVAL;
    return -1;
  }
  int ret;
#ifndef _WIN32
  ret = vsnprintf(buffer, buffer_size, format, args);
#else
  // errno isn't explicitly set to 0 when truncation occurs.
  errno = 0;
  ret = _vsnprintf_s(buffer, buffer_size, _TRUNCATE, format, args);
  if (-1 == ret && 0 == errno) {
    // This is the case where truncation has occurred, return how long it would have been.
    return _vscprintf(format, args);
  }
#endif
  return ret;
}

#ifdef __cplusplus
}
#endif
