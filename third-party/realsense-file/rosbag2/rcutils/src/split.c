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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rcutils/error_handling.h"
#include "rcutils/format_string.h"
#include "rcutils/logging_macros.h"
#include "rcutils/split.h"
#include "rcutils/types.h"

rcutils_ret_t
rcutils_split(
  const char * str,
  char delimiter,
  rcutils_allocator_t allocator,
  rcutils_string_array_t * string_array)
{
  if (NULL == string_array) {
    RCUTILS_SET_ERROR_MSG("string_array is null");
    return RCUTILS_RET_INVALID_ARGUMENT;
  }
  if (NULL == str || strlen(str) == 0) {
    *string_array = rcutils_get_zero_initialized_string_array();
    return RCUTILS_RET_OK;
  }
  string_array->allocator = allocator;

  size_t string_size = strlen(str);

  // does it start with a delimiter?
  size_t lhs_offset = 0;
  if (str[0] == delimiter) {
    lhs_offset = 1;
  }

  // does it end with a delimiter?
  size_t rhs_offset = 0;
  if (str[string_size - 1] == delimiter) {
    rhs_offset = 1;
  }

  string_array->size = 1;
  for (size_t i = lhs_offset; i < string_size - rhs_offset; ++i) {
    if (str[i] == delimiter) {
      ++string_array->size;
    }
  }
  // TODO(wjwwood): refactor this function so it can use rcutils_string_array_init() instead
  string_array->data = allocator.allocate(string_array->size * sizeof(char *), allocator.state);
  if (NULL == string_array->data) {
    goto fail;
  }

  size_t token_counter = 0;
  size_t lhs = 0 + lhs_offset;
  size_t rhs = 0 + lhs_offset;
  for (; rhs < string_size - rhs_offset; ++rhs) {
    if (str[rhs] == delimiter) {
      // in case we have two consecutive delimiters
      // we ignore these and diminish the size of the array
      if (rhs - lhs < 1) {
        --string_array->size;
        string_array->data[string_array->size] = NULL;
      } else {
        // +2 (1+1) because lhs is index, not actual position
        // and nullterminating
        string_array->data[token_counter] =
          allocator.allocate((rhs - lhs + 2) * sizeof(char), allocator.state);
        if (NULL == string_array->data[token_counter]) {
          string_array->size = token_counter;
          goto fail;
        }
        snprintf(string_array->data[token_counter], (rhs - lhs + 1), "%s", str + lhs);
        ++token_counter;
      }
      lhs = rhs;
      ++lhs;
    }
  }

  // take care of trailing token
  if (rhs - lhs < 1) {
    --string_array->size;
    string_array->data[string_array->size] = NULL;
  } else {
    string_array->data[token_counter] =
      allocator.allocate((rhs - lhs + 2) * sizeof(char), allocator.state);
    snprintf(string_array->data[token_counter], (rhs - lhs + 1), "%s", str + lhs);
  }

  return RCUTILS_RET_OK;

fail:
  if (rcutils_string_array_fini(string_array) != RCUTILS_RET_OK) {
    RCUTILS_SAFE_FWRITE_TO_STDERR("failed to finalize string array during error handling: ");
    RCUTILS_SAFE_FWRITE_TO_STDERR(rcutils_get_error_string().str);
    RCUTILS_SAFE_FWRITE_TO_STDERR("\n");
    rcutils_reset_error();
  }

  RCUTILS_SET_ERROR_MSG("unable to allocate memory for string array data");
  return RCUTILS_RET_ERROR;
}

rcutils_ret_t
rcutils_split_last(
  const char * str,
  char delimiter,
  rcutils_allocator_t allocator,
  rcutils_string_array_t * string_array)
{
  if (NULL == str || strlen(str) == 0) {
    *string_array = rcutils_get_zero_initialized_string_array();
    return RCUTILS_RET_OK;
  }

  size_t string_size = strlen(str);

  // does it start with a delimiter?
  size_t lhs_offset = 0;
  if (str[0] == delimiter) {
    lhs_offset = 1;
  }

  // does it end with a delimiter?
  size_t rhs_offset = 0;
  if (str[string_size - 1] == delimiter) {
    rhs_offset = 1;
  }

  size_t found_last = string_size;
  for (size_t i = lhs_offset; i < string_size - rhs_offset; ++i) {
    if (str[i] == delimiter) {
      found_last = i;
    }
  }

  rcutils_ret_t result_error;
  if (found_last == string_size) {
    rcutils_ret_t ret = rcutils_string_array_init(string_array, 1, &allocator);
    if (ret != RCUTILS_RET_OK) {
      result_error = ret;
      goto fail;
    }
    string_array->data[0] =
      allocator.allocate((found_last - lhs_offset + 2) * sizeof(char), allocator.state);
    if (NULL == string_array->data[0]) {
      result_error = RCUTILS_RET_BAD_ALLOC;
      goto fail;
    }
    snprintf(string_array->data[0], found_last - lhs_offset + 1, "%s", str + lhs_offset);
  } else {
    rcutils_ret_t ret = rcutils_string_array_init(string_array, 2, &allocator);
    if (ret != RCUTILS_RET_OK) {
      result_error = ret;
      goto fail;
    }

    /*
     * The extra +1 after 'found_last' is to compensate its position
     * found_last is the index of the the last position the delimiter was found
     * and not the actual position in terms of counting from 1
     */
    size_t inner_rhs_offset = (str[found_last - 1] == delimiter) ? 1 : 0;
    string_array->data[0] = allocator.allocate(
      (found_last + 1 - lhs_offset - inner_rhs_offset + 1) * sizeof(char),
      allocator.state);
    if (NULL == string_array->data[0]) {
      result_error = RCUTILS_RET_BAD_ALLOC;
      goto fail;
    }
    snprintf(
      string_array->data[0], found_last + 1 - lhs_offset - inner_rhs_offset,
      "%s", str + lhs_offset);

    string_array->data[1] = allocator.allocate(
      (string_size - found_last - rhs_offset + 1) * sizeof(char),
      allocator.state);
    if (NULL == string_array->data[1]) {
      result_error = RCUTILS_RET_BAD_ALLOC;
      goto fail;
    }
    snprintf(
      string_array->data[1], string_size - found_last - rhs_offset, "%s",
      str + found_last + 1);
  }

  return RCUTILS_RET_OK;

fail:
  if (rcutils_string_array_fini(string_array) != RCUTILS_RET_OK) {
    RCUTILS_LOG_ERROR(
      "failed to clean up on error (leaking memory): '%s'", rcutils_get_error_string().str);
  }
  return result_error;
}

#ifdef __cplusplus
}
#endif
