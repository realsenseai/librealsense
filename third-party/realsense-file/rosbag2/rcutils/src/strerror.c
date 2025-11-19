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

#ifdef __cplusplus
extern "C"
{
#endif

#include <errno.h>
#include <string.h>

#include "rcutils/strerror.h"

void
rcutils_strerror(char * buffer, size_t buffer_length)
{
#if defined(_WIN32)
  strerror_s(buffer, buffer_length, errno);
#elif defined(_GNU_SOURCE) && (!defined(ANDROID) || __ANDROID_API__ >= 23)
  /* GNU-specific */
  char * msg = strerror_r(errno, buffer, buffer_length);
  if (msg != buffer) {
    strncpy(buffer, msg, buffer_length);
    buffer[buffer_length - 1] = '\0';
  }
#else
  /* XSI-compliant */
  int error_status = strerror_r(errno, buffer, buffer_length);
  if (error_status != 0) {
    strncpy(buffer, "Failed to get error", buffer_length);
    buffer[buffer_length - 1] = '\0';
  }
#endif
}

#ifdef __cplusplus
}
#endif
