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

#include <stdarg.h>
#include "rcutils/error_handling.h"
#include "rcutils/types/char_array.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

rcutils_char_array_t
rcutils_get_zero_initialized_char_array(void)
{
  static rcutils_char_array_t char_array = {
    .buffer = NULL,
    .owns_buffer = true,
    .buffer_length = 0u,
    .buffer_capacity = 0u
  };
  char_array.allocator = rcutils_get_zero_initialized_allocator();
  return char_array;
}

rcutils_ret_t
rcutils_char_array_init(
  rcutils_char_array_t * char_array,
  size_t buffer_capacity,
  const rcutils_allocator_t * allocator)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(char_array, RCUTILS_RET_ERROR);
  RCUTILS_CHECK_ALLOCATOR_WITH_MSG(
    allocator, "char array has no valid allocator",
    return RCUTILS_RET_ERROR);

  char_array->owns_buffer = true;
  char_array->buffer_length = 0lu;
  char_array->buffer_capacity = buffer_capacity;
  char_array->allocator = *allocator;

  if (buffer_capacity > 0lu) {
    char_array->buffer =
      (char *)allocator->allocate(buffer_capacity * sizeof(char), allocator->state);
    RCUTILS_CHECK_FOR_NULL_WITH_MSG(
      char_array->buffer,
      "failed to allocate memory for char array",
      char_array->buffer_capacity = 0lu;
      char_array->buffer_length = 0lu;
      return RCUTILS_RET_BAD_ALLOC);
    char_array->buffer[0] = '\0';
  }

  return RCUTILS_RET_OK;
}

rcutils_ret_t
rcutils_char_array_fini(rcutils_char_array_t * char_array)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(char_array, RCUTILS_RET_ERROR);

  if (char_array->owns_buffer) {
    rcutils_allocator_t * allocator = &char_array->allocator;
    RCUTILS_CHECK_ALLOCATOR_WITH_MSG(
      allocator, "char array has no valid allocator",
      return RCUTILS_RET_ERROR);

    allocator->deallocate(char_array->buffer, allocator->state);
  }

  char_array->buffer = NULL;
  char_array->buffer_length = 0lu;
  char_array->buffer_capacity = 0lu;

  return RCUTILS_RET_OK;
}

rcutils_ret_t
rcutils_char_array_resize(rcutils_char_array_t * char_array, size_t new_size)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(char_array, RCUTILS_RET_ERROR);

  if (0lu == new_size) {
    RCUTILS_SET_ERROR_MSG("new size of char_array has to be greater than zero");
    return RCUTILS_RET_INVALID_ARGUMENT;
  }

  rcutils_allocator_t * allocator = &char_array->allocator;
  RCUTILS_CHECK_ALLOCATOR_WITH_MSG(
    allocator, "char array has no valid allocator",
    return RCUTILS_RET_ERROR);

  if (new_size == char_array->buffer_capacity) {
    // nothing to do here
    return RCUTILS_RET_OK;
  }

  char * old_buf = char_array->buffer;
  size_t old_size = char_array->buffer_capacity;
  size_t old_length = char_array->buffer_length;

  if (char_array->owns_buffer) {  // we own the buffer, we can do whatever we want
    char * new_buf = rcutils_reallocf(char_array->buffer, new_size * sizeof(char), allocator);
    RCUTILS_CHECK_FOR_NULL_WITH_MSG(
      new_buf,
      "failed to reallocate memory for char array",
      return RCUTILS_RET_BAD_ALLOC);
    char_array->buffer = new_buf;
  } else {  // we don't realloc memory we don't own. instead, we alloc some new space
    rcutils_ret_t ret = rcutils_char_array_init(char_array, new_size, allocator);
    if (ret != RCUTILS_RET_OK) {
      return ret;
    }
    size_t n = MIN(new_size, old_size);
    memcpy(char_array->buffer, old_buf, n);
    char_array->buffer[n - 1] = '\0';  // always have an ending
  }

  char_array->buffer_capacity = new_size;
  char_array->buffer_length = MIN(new_size, old_length);

  return RCUTILS_RET_OK;
}

rcutils_ret_t
rcutils_char_array_expand_as_needed(rcutils_char_array_t * char_array, size_t new_size)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(char_array, RCUTILS_RET_ERROR);

  if (new_size <= char_array->buffer_capacity) {
    return RCUTILS_RET_OK;
  }

  return rcutils_char_array_resize(char_array, new_size);
}

static int
_rcutils_char_array_vsprintf(rcutils_char_array_t * char_array, const char * format, va_list args)
{
  va_list args_clone;
  va_copy(args_clone, args);

  // when doing size calculation, remember the return value of vsnprintf excludes terminating null
  // byte
  int size = vsnprintf(char_array->buffer, char_array->buffer_capacity, format, args_clone);

  va_end(args_clone);

  return size;
}

rcutils_ret_t
rcutils_char_array_vsprintf(rcutils_char_array_t * char_array, const char * format, va_list args)
{
  int size = _rcutils_char_array_vsprintf(char_array, format, args);

  if (size < 0) {
    RCUTILS_SET_ERROR_MSG("vsprintf on char array failed");
    return RCUTILS_RET_ERROR;
  }

  size_t new_size = (size_t) size + 1;  // with the terminating null byte

  if (new_size > char_array->buffer_capacity) {
    rcutils_ret_t ret = rcutils_char_array_expand_as_needed(char_array, new_size);
    if (ret != RCUTILS_RET_OK) {
      RCUTILS_SET_ERROR_MSG("char array failed to expand");
      return ret;
    }

    if (_rcutils_char_array_vsprintf(char_array, format, args) != size) {
      if (rcutils_char_array_fini(char_array) == RCUTILS_RET_OK) {
        RCUTILS_SET_ERROR_MSG("vsprintf on resized char array failed");
      } else {
        RCUTILS_SET_ERROR_MSG("vsprintf on resized char array failed; clean up failed too");
      }
      return RCUTILS_RET_ERROR;
    }
  }

  char_array->buffer_length = new_size;

  return RCUTILS_RET_OK;
}

rcutils_ret_t
rcutils_char_array_memcpy(rcutils_char_array_t * char_array, const char * src, size_t n)
{
  rcutils_ret_t ret = rcutils_char_array_expand_as_needed(char_array, n);
  if (ret != RCUTILS_RET_OK) {
    RCUTILS_SET_ERROR_MSG("char array failed to expand");
    return ret;
  }
  memcpy(char_array->buffer, src, n);
  char_array->buffer_length = n;
  return RCUTILS_RET_OK;
}

rcutils_ret_t
rcutils_char_array_strcpy(rcutils_char_array_t * char_array, const char * src)
{
  return rcutils_char_array_memcpy(char_array, src, strlen(src) + 1);
}

rcutils_ret_t
rcutils_char_array_strncat(rcutils_char_array_t * char_array, const char * src, size_t n)
{
  size_t current_strlen = strlen(char_array->buffer);
  size_t new_length = current_strlen + n + 1;
  rcutils_ret_t ret = rcutils_char_array_expand_as_needed(char_array, new_length);
  if (ret != RCUTILS_RET_OK) {
    RCUTILS_SET_ERROR_MSG("char array failed to expand");
    return ret;
  }
#ifndef _WIN32
  strncat(char_array->buffer, src, n);
#else
  errno_t err = strncat_s(char_array->buffer, new_length, src, n);
  if (0 != err) {
    RCUTILS_SET_ERROR_MSG("strncat_s failed");
    return RCUTILS_RET_ERROR;
  }
#endif
  char_array->buffer_length = new_length;
  return RCUTILS_RET_OK;
}

rcutils_ret_t
rcutils_char_array_strcat(rcutils_char_array_t * char_array, const char * src)
{
  return rcutils_char_array_strncat(char_array, src, strlen(src));
}
