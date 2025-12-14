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
#include <stdlib.h>

#include "rcutils/env.h"
#include "rcutils/error_handling.h"

// TODO(cottsay): Move the stuff in get_env.c in here

bool
rcutils_set_env(const char * env_name, const char * env_value)
{
  RCUTILS_CAN_RETURN_WITH_ERROR_OF(false);

  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    env_name, "env_name is null", return false);

#ifdef _WIN32
  if (NULL == env_value) {
    env_value = "";
  }
  if (0 != _putenv_s(env_name, env_value)) {
    RCUTILS_SET_ERROR_MSG_WITH_FORMAT_STRING("_putenv_s failed: %d", errno);
    return false;
  }
#else
  if (NULL == env_value) {
    if (0 != unsetenv(env_name)) {
      RCUTILS_SET_ERROR_MSG_WITH_FORMAT_STRING("unsetenv failed: %d", errno);
      return false;
    }
  } else {
    if (0 != setenv(env_name, env_value, 1)) {
      RCUTILS_SET_ERROR_MSG_WITH_FORMAT_STRING("setenv failed: %d", errno);
      return false;
    }
  }
#endif

  return true;
}

#ifdef __cplusplus
}
#endif
