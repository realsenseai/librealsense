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

#ifndef RCUTILS__ENV_H_
#define RCUTILS__ENV_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>

#include "rcutils/macros.h"
#include "rcutils/visibility_control.h"

// TODO(cottsay): Deprecate get_env.h and eventually merge it here
#include "rcutils/get_env.h"

/// Set or un-set a process-scoped environment variable.
/**
 * This function modifies the environment variables for the current process by
 * copying given string values into the process' global environment variable
 * store.
 *
 * \par Thread Safety:
 * This function is not thread-safe, particularly when called concurrently with
 * ::rcutils_get_env. Take care not to modify the environment variables while
 * another thread might be reading or writing environment variables.
 *
 * \par Platform Consistency:
 * The behavior when setting a variable to an empty string (`""`) differs
 * between platforms. On Windows, the variable is un-set (as if \p env_value was
 * `NULL`), while on other platforms the variable is set to an empty string as
 * expected.
 *
 * \param[in] env_name Name of the environment variable to modify.
 * \param[in] env_value Value to set the environment variable to, or `NULL` to
 *   un-set.
 * \return `True` if success
 * \return `False` if env_name is invalid or NULL
 * \return `False` on failure
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
bool
rcutils_set_env(const char * env_name, const char * env_value);

#ifdef __cplusplus
}
#endif

#endif  // RCUTILS__ENV_H_
