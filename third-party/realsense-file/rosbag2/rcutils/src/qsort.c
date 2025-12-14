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

#include <stdlib.h>

#include "rcutils/error_handling.h"
#include "rcutils/qsort.h"

rcutils_ret_t
rcutils_qsort(void * ptr, size_t count, size_t size, int (* comp)(const void *, const void *))
{
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    comp, "comp is null", return RCUTILS_RET_INVALID_ARGUMENT);

  if (1 >= count) {
    return RCUTILS_RET_OK;
  }

  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    ptr, "ptr is null", return RCUTILS_RET_INVALID_ARGUMENT);

  qsort(ptr, count, size, comp);

  return RCUTILS_RET_OK;
}

#ifdef __cplusplus
}
#endif
