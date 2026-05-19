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

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdlib.h>
#include <string.h>

#include "rcutils/allocator.h"
#include "rcutils/error_handling.h"
#include "rcutils/types/string_array.h"
#include "rcutils/types/rcutils_ret.h"

rcutils_string_array_t
rcutils_get_zero_initialized_string_array(void)
{
  static rcutils_string_array_t array = {
    .size = 0,
    .data = NULL,
  };
  array.allocator = rcutils_get_zero_initialized_allocator();
  return array;
}

rcutils_ret_t
rcutils_string_array_init(
  rcutils_string_array_t * string_array,
  size_t size,
  const rcutils_allocator_t * allocator)
{
  RCUTILS_CAN_RETURN_WITH_ERROR_OF(RCUTILS_RET_INVALID_ARGUMENT);
  RCUTILS_CAN_RETURN_WITH_ERROR_OF(RCUTILS_RET_BAD_ALLOC);

  if (NULL == allocator) {
    RCUTILS_SET_ERROR_MSG("allocator is null");
    return RCUTILS_RET_INVALID_ARGUMENT;
  }
  if (NULL == string_array) {
    RCUTILS_SET_ERROR_MSG("string_array is null");
    return RCUTILS_RET_INVALID_ARGUMENT;
  }
  string_array->size = size;
  string_array->data = allocator->zero_allocate(size, sizeof(char *), allocator->state);
  if (NULL == string_array->data && 0 != size) {
    RCUTILS_SET_ERROR_MSG("failed to allocate string array");
    return RCUTILS_RET_BAD_ALLOC;
  }
  string_array->allocator = *allocator;
  return RCUTILS_RET_OK;
}

rcutils_ret_t
rcutils_string_array_fini(rcutils_string_array_t * string_array)
{
  RCUTILS_CAN_RETURN_WITH_ERROR_OF(RCUTILS_RET_INVALID_ARGUMENT);

  if (NULL == string_array) {
    RCUTILS_SET_ERROR_MSG("string_array is null");
    return RCUTILS_RET_INVALID_ARGUMENT;
  }

  if (NULL == string_array->data) {
    return RCUTILS_RET_OK;
  }

  rcutils_allocator_t * allocator = &string_array->allocator;
  if (!rcutils_allocator_is_valid(allocator)) {
    RCUTILS_SET_ERROR_MSG("allocator is invalid");
    return RCUTILS_RET_INVALID_ARGUMENT;
  }
  size_t i;
  for (i = 0; i < string_array->size; ++i) {
    allocator->deallocate(string_array->data[i], allocator->state);
    string_array->data[i] = NULL;
  }
  allocator->deallocate(string_array->data, allocator->state);
  string_array->data = NULL;
  string_array->size = 0;

  return RCUTILS_RET_OK;
}

rcutils_ret_t
rcutils_string_array_cmp(
  const rcutils_string_array_t * lhs,
  const rcutils_string_array_t * rhs,
  int * res)
{
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    lhs, "lhs string array is null", return RCUTILS_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    rhs, "rhs string array is null", return RCUTILS_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    res, "res argument is null", return RCUTILS_RET_INVALID_ARGUMENT);

  size_t smallest_size = lhs->size;
  if (rhs->size < smallest_size) {
    smallest_size = rhs->size;
  }

  if (smallest_size > 0) {
    RCUTILS_CHECK_FOR_NULL_WITH_MSG(
      lhs->data, "lhs->data is null", return RCUTILS_RET_INVALID_ARGUMENT);
    RCUTILS_CHECK_FOR_NULL_WITH_MSG(
      rhs->data, "rhs->data is null", return RCUTILS_RET_INVALID_ARGUMENT);
  }

  for (size_t i = 0; i < smallest_size; ++i) {
    RCUTILS_CHECK_FOR_NULL_WITH_MSG(
      lhs->data[i], "lhs array element is null", return RCUTILS_RET_ERROR);
    RCUTILS_CHECK_FOR_NULL_WITH_MSG(
      rhs->data[i], "rhs array element is null", return RCUTILS_RET_ERROR);
    // Loop until we find a pair of strings that are not equal
    int strcmp_res = strcmp(lhs->data[i], rhs->data[i]);
    if (0 != strcmp_res) {
      *res = strcmp_res;
      return RCUTILS_RET_OK;
    }
  }

  // If all strings equal, compare array sizes
  *res = 0;
  if (lhs->size < rhs->size) {
    *res = -1;
  } else if (lhs->size > rhs->size) {
    *res = 1;
  }
  return RCUTILS_RET_OK;
}

rcutils_ret_t
rcutils_string_array_resize(
  rcutils_string_array_t * string_array,
  size_t new_size)
{
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    string_array, "string_array is null", return RCUTILS_RET_INVALID_ARGUMENT);

  if (string_array->size == new_size) {
    return RCUTILS_RET_OK;
  }

  rcutils_allocator_t * allocator = &string_array->allocator;
  RCUTILS_CHECK_ALLOCATOR_WITH_MSG(
    allocator, "allocator is invalid", return RCUTILS_RET_INVALID_ARGUMENT);

  // Stash entries being removed
  rcutils_string_array_t to_reclaim = rcutils_get_zero_initialized_string_array();
  if (new_size < string_array->size) {
    size_t num_removed = string_array->size - new_size;
    rcutils_ret_t ret = rcutils_string_array_init(&to_reclaim, num_removed, allocator);
    if (RCUTILS_RET_OK != ret) {
      // rcutils_string_array_init should have already set an error message
      return ret;
    }
    memcpy(
      to_reclaim.data, &string_array->data[new_size],
      to_reclaim.size * sizeof(char *));
  }

  char ** new_data = allocator->reallocate(
    string_array->data, new_size * sizeof(char *), allocator->state);
  if (NULL == new_data && 0 != new_size) {
    RCUTILS_SET_ERROR_MSG("failed to allocate string array");
    for (size_t i = 0; i < to_reclaim.size; ++i) {
      to_reclaim.data[i] = NULL;
    }
    rcutils_ret_t ret = rcutils_string_array_fini(&to_reclaim);
    if (RCUTILS_RET_OK != ret) {
      RCUTILS_SET_ERROR_MSG("memory was leaked during error handling");
    }
    return RCUTILS_RET_BAD_ALLOC;
  }
  string_array->data = new_data;

  // Zero-initialize new entries
  for (size_t i = string_array->size; i < new_size; ++i) {
    string_array->data[i] = NULL;
  }

  string_array->size = new_size;

  // Lastly, reclaim removed entries
  return rcutils_string_array_fini(&to_reclaim);
}

int
rcutils_string_array_sort_compare(const void * lhs, const void * rhs)
{
  const char * left = *(const char **)lhs;
  const char * right = *(const char **)rhs;
  if (NULL == left) {
    return NULL == right ? 0 : 1;
  } else if (NULL == right) {
    return -1;
  }
  return strcmp(left, right);
}

#ifdef __cplusplus
}
#endif
