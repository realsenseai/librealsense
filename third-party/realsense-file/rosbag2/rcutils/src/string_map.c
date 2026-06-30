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

#include "rcutils/types/string_map.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "./common.h"
#include "rcutils/strdup.h"
#include "rcutils/format_string.h"
#include "rcutils/types/rcutils_ret.h"

typedef struct rcutils_string_map_impl_t
{
  char ** keys;
  char ** values;
  size_t capacity;
  size_t size;
  rcutils_allocator_t allocator;
} rcutils_string_map_impl_t;

rcutils_string_map_t
rcutils_get_zero_initialized_string_map(void)
{
  static rcutils_string_map_t zero_initialized_string_map;
  zero_initialized_string_map.impl = NULL;
  return zero_initialized_string_map;
}

rcutils_ret_t
rcutils_string_map_init(
  rcutils_string_map_t * string_map,
  size_t initial_capacity,
  rcutils_allocator_t allocator)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(string_map, RCUTILS_RET_INVALID_ARGUMENT);
  if (string_map->impl != NULL) {
    RCUTILS_SET_ERROR_MSG("string_map already initialized");
    return RCUTILS_RET_STRING_MAP_ALREADY_INIT;
  }
  RCUTILS_CHECK_ALLOCATOR_WITH_MSG(
    &allocator, "invalid allocator", return RCUTILS_RET_INVALID_ARGUMENT)
  string_map->impl = allocator.allocate(sizeof(rcutils_string_map_impl_t), allocator.state);
  if (NULL == string_map->impl) {
    RCUTILS_SET_ERROR_MSG("failed to allocate memory for string map impl struct");
    return RCUTILS_RET_BAD_ALLOC;
  }
  string_map->impl->keys = NULL;
  string_map->impl->values = NULL;
  string_map->impl->capacity = 0;
  string_map->impl->size = 0;
  string_map->impl->allocator = allocator;
  rcutils_ret_t ret = rcutils_string_map_reserve(string_map, initial_capacity);
  if (ret != RCUTILS_RET_OK) {
    // error mesage is already set, clean up and return the ret
    allocator.deallocate(string_map->impl, allocator.state);
    string_map->impl = NULL;
    return ret;
  }
  return RCUTILS_RET_OK;
}

rcutils_ret_t
rcutils_string_map_fini(rcutils_string_map_t * string_map)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(string_map, RCUTILS_RET_INVALID_ARGUMENT);
  if (NULL == string_map->impl) {
    return RCUTILS_RET_OK;
  }
  rcutils_ret_t ret = rcutils_string_map_clear(string_map);
  if (ret != RCUTILS_RET_OK) {
    // error message already set
    return ret;
  }
  ret = rcutils_string_map_reserve(string_map, 0);
  if (ret != RCUTILS_RET_OK) {
    // error message already set
    return ret;
  }
  rcutils_allocator_t allocator = string_map->impl->allocator;

  allocator.deallocate(string_map->impl, allocator.state);
  string_map->impl = NULL;

  return RCUTILS_RET_OK;
}

rcutils_ret_t
rcutils_string_map_get_capacity(const rcutils_string_map_t * string_map, size_t * capacity)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(string_map, RCUTILS_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    string_map->impl, "invalid string map", return RCUTILS_RET_STRING_MAP_INVALID);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(capacity, RCUTILS_RET_INVALID_ARGUMENT);
  // *INDENT-OFF* (prevent uncrustify getting this wrong)
  *capacity = string_map->impl->capacity;
  // *INDENT-ON*
  return RCUTILS_RET_OK;
}

rcutils_ret_t
rcutils_string_map_get_size(const rcutils_string_map_t * string_map, size_t * size)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(string_map, RCUTILS_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    string_map->impl, "invalid string map", return RCUTILS_RET_STRING_MAP_INVALID);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(size, RCUTILS_RET_INVALID_ARGUMENT);
  {  // otherwise uncrustify moves the * to the end of the previous line...
    *size = string_map->impl->size;
  }
  return RCUTILS_RET_OK;
}

rcutils_ret_t
rcutils_string_map_reserve(rcutils_string_map_t * string_map, size_t capacity)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(string_map, RCUTILS_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    string_map->impl, "invalid string map", return RCUTILS_RET_STRING_MAP_INVALID);
  rcutils_allocator_t allocator = string_map->impl->allocator;
  // short circuit, if requested capacity is less than the size of the map
  if (capacity < string_map->impl->size) {
    // set the capacity to the current size instead
    return rcutils_string_map_reserve(string_map, string_map->impl->size);
  }
  if (capacity == string_map->impl->capacity) {
    // if requested capacity is equal to the current capacity, nothing to do
    return RCUTILS_RET_OK;
  } else if (capacity == 0) {
    // if the requested capacity is zero, then make sure the existing keys and values are free'd
    allocator.deallocate(string_map->impl->keys, allocator.state);
    string_map->impl->keys = NULL;
    allocator.deallocate(string_map->impl->values, allocator.state);
    string_map->impl->values = NULL;
    // falls through to normal function end
  } else {
    // if the capacity non-zero and different, use realloc to increase/shrink the size
    // note that realloc when the pointer is NULL is the same as malloc
    // note also that realloc will shrink the space if needed

    // ensure that reallocate won't overflow capacity
    if (capacity > (SIZE_MAX / sizeof(char *))) {
      RCUTILS_SET_ERROR_MSG("requested capacity for string_map too large");
      return RCUTILS_RET_BAD_ALLOC;
    }

    // resize the keys, assigning the result only if it succeeds
    char ** new_keys =
      allocator.reallocate(string_map->impl->keys, capacity * sizeof(char *), allocator.state);
    if (NULL == new_keys) {
      RCUTILS_SET_ERROR_MSG("failed to allocate memory for string_map keys");
      return RCUTILS_RET_BAD_ALLOC;
    }
    string_map->impl->keys = new_keys;

    // resize the values, assigning the result only if it succeeds
    char ** new_values =
      allocator.reallocate(string_map->impl->values, capacity * sizeof(char *), allocator.state);
    if (NULL == new_values) {
      RCUTILS_SET_ERROR_MSG("failed to allocate memory for string_map values");
      return RCUTILS_RET_BAD_ALLOC;
    }
    string_map->impl->values = new_values;

    // zero out the new memory, if there is any (expanded instead of shrunk)
    if (capacity > string_map->impl->capacity) {
      size_t i = string_map->impl->capacity;
      for (; i < capacity; ++i) {
        string_map->impl->keys[i] = NULL;
        string_map->impl->values[i] = NULL;
      }
    }
    // falls through to normal function end
  }
  string_map->impl->capacity = capacity;
  return RCUTILS_RET_OK;
}

static void
__remove_key_and_value_at_index(rcutils_string_map_impl_t * string_map_impl, size_t index)
{
  rcutils_allocator_t allocator = string_map_impl->allocator;
  allocator.deallocate(string_map_impl->keys[index], allocator.state);
  string_map_impl->keys[index] = NULL;
  allocator.deallocate(string_map_impl->values[index], allocator.state);
  string_map_impl->values[index] = NULL;
  string_map_impl->size--;
}

rcutils_ret_t
rcutils_string_map_clear(rcutils_string_map_t * string_map)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(string_map, RCUTILS_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    string_map->impl, "invalid string map", return RCUTILS_RET_STRING_MAP_INVALID);
  size_t i = 0;
  for (; i < string_map->impl->capacity; ++i) {
    if (string_map->impl->keys[i] != NULL) {
      __remove_key_and_value_at_index(string_map->impl, i);
    }
  }
  return RCUTILS_RET_OK;
}

rcutils_ret_t
rcutils_string_map_set(rcutils_string_map_t * string_map, const char * key, const char * value)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(string_map, RCUTILS_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    string_map->impl, "invalid string map", return RCUTILS_RET_STRING_MAP_INVALID);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(key, RCUTILS_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(value, RCUTILS_RET_INVALID_ARGUMENT);
  rcutils_ret_t ret = rcutils_string_map_set_no_resize(string_map, key, value);
  // if it fails due to not enough space, resize and try again
  if (ret == RCUTILS_RET_NOT_ENOUGH_SPACE) {
    rcutils_reset_error();
    // default to doubling the size of the map's capacity
    size_t new_capacity = (string_map->impl->capacity) ? 2 * string_map->impl->capacity : 1;
    ret = rcutils_string_map_reserve(string_map, new_capacity);
    if (ret != RCUTILS_RET_OK) {
      // error message is already set
      return ret;
    }
    // try again
    return rcutils_string_map_set_no_resize(string_map, key, value);
  }
  return ret;
}

static bool
__get_index_of_key_if_exists(
  rcutils_string_map_impl_t * string_map_impl,
  const char * key,
  size_t key_length,
  size_t * index)
{
  size_t i = 0;
  for (; i < string_map_impl->capacity; ++i) {
    if (NULL == string_map_impl->keys[i]) {
      continue;
    }
    size_t cmp_count = strlen(string_map_impl->keys[i]);
    if (key_length > cmp_count) {
      cmp_count = key_length;
    }
    if (strncmp(key, string_map_impl->keys[i], cmp_count) == 0) {
      *index = i;
      return true;
    }
  }
  return false;
}

rcutils_ret_t
rcutils_string_map_set_no_resize(
  rcutils_string_map_t * string_map,
  const char * key,
  const char * value)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(string_map, RCUTILS_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    string_map->impl, "invalid string map", return RCUTILS_RET_STRING_MAP_INVALID);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(key, RCUTILS_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(value, RCUTILS_RET_INVALID_ARGUMENT);
  rcutils_allocator_t allocator = string_map->impl->allocator;
  size_t key_index;
  bool should_free_key_on_error = false;
  bool key_exists = __get_index_of_key_if_exists(string_map->impl, key, strlen(key), &key_index);
  if (!key_exists) {
    // create space for, and store the key if it doesn't exist yet
    assert(string_map->impl->size <= string_map->impl->capacity);  // defensive, should not happen
    if (string_map->impl->size == string_map->impl->capacity) {
      return RCUTILS_RET_NOT_ENOUGH_SPACE;
    }
    for (key_index = 0; key_index < string_map->impl->capacity; ++key_index) {
      if (NULL == string_map->impl->keys[key_index]) {
        break;
      }
    }
    assert(key_index < string_map->impl->capacity);  // defensive, this should not happen
    string_map->impl->keys[key_index] = rcutils_strdup(key, allocator);
    if (NULL == string_map->impl->keys[key_index]) {
      RCUTILS_SET_ERROR_MSG("failed to allocate memory for key");
      return RCUTILS_RET_BAD_ALLOC;
    }
    should_free_key_on_error = true;
  }
  // at this point the key is in the map, waiting for the value to set/overwritten
  char * original_value = string_map->impl->values[key_index];
  char * new_value = rcutils_strdup(value, allocator);
  if (NULL == new_value) {
    RCUTILS_SET_ERROR_MSG("failed to allocate memory for key");
    if (should_free_key_on_error) {
      allocator.deallocate(string_map->impl->keys[key_index], allocator.state);
      string_map->impl->keys[key_index] = NULL;
    }
    return RCUTILS_RET_BAD_ALLOC;
  }
  string_map->impl->values[key_index] = new_value;
  if (original_value != NULL) {
    // clean up the old value if not NULL
    allocator.deallocate(original_value, allocator.state);
  }
  if (!key_exists) {
    // if the key didn't exist, then we had to add it, so increase the size
    string_map->impl->size++;
  }
  return RCUTILS_RET_OK;
}

rcutils_ret_t
rcutils_string_map_unset(rcutils_string_map_t * string_map, const char * key)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(string_map, RCUTILS_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    string_map->impl, "invalid string map", return RCUTILS_RET_STRING_MAP_INVALID);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(key, RCUTILS_RET_INVALID_ARGUMENT);
  size_t key_index;
  if (!__get_index_of_key_if_exists(string_map->impl, key, strlen(key), &key_index)) {
    RCUTILS_SET_ERROR_MSG_WITH_FORMAT_STRING("key '%s' not found", key);
    return RCUTILS_RET_STRING_KEY_NOT_FOUND;
  }
  __remove_key_and_value_at_index(string_map->impl, key_index);
  return RCUTILS_RET_OK;
}

bool
rcutils_string_map_key_exists(const rcutils_string_map_t * string_map, const char * key)
{
  if (NULL == key) {
    return false;
  }
  return rcutils_string_map_key_existsn(string_map, key, strlen(key));
}

bool
rcutils_string_map_key_existsn(
  const rcutils_string_map_t * string_map,
  const char * key,
  size_t key_length)
{
  if (NULL == string_map || NULL == string_map->impl || NULL == key) {
    return false;
  }
  size_t key_index;
  bool key_exists = __get_index_of_key_if_exists(string_map->impl, key, key_length, &key_index);
  return key_exists;
}

const char *
rcutils_string_map_get(const rcutils_string_map_t * string_map, const char * key)
{
  if (NULL == key) {
    return NULL;
  }
  return rcutils_string_map_getn(string_map, key, strlen(key));
}

const char *
rcutils_string_map_getn(
  const rcutils_string_map_t * string_map,
  const char * key,
  size_t key_length)
{
  if (NULL == string_map || NULL == string_map->impl || NULL == key) {
    return NULL;
  }
  size_t key_index;
  if (__get_index_of_key_if_exists(string_map->impl, key, key_length, &key_index)) {
    return string_map->impl->values[key_index];
  }
  return NULL;
}

const char *
rcutils_string_map_get_next_key(
  const rcutils_string_map_t * string_map,
  const char * key)
{
  if (NULL == string_map || !string_map->impl) {
    return NULL;
  }
  if (string_map->impl->size == 0) {
    return NULL;
  }
  size_t start_index = 0;
  if (key != NULL) {
    // if given a key, try to find it
    bool given_key_found = false;
    size_t i = 0;
    for (; i < string_map->impl->capacity; ++i) {
      if (string_map->impl->keys[i] == key) {
        given_key_found = true;
        // given key found at index i, start there + 1
        start_index = i + 1;
      }
    }
    if (!given_key_found) {
      // given key not found, cannot return next key with that
      return NULL;
    }
  }
  // iterate through the storage and look for another non-NULL key to return
  size_t i = start_index;
  for (; i < string_map->impl->capacity; ++i) {
    if (string_map->impl->keys[i] != NULL) {
      // next key found, return it
      return string_map->impl->keys[i];
    }
  }
  // next key (or first key) not found
  return NULL;
}

rcutils_ret_t
rcutils_string_map_copy(
  const rcutils_string_map_t * src_string_map,
  rcutils_string_map_t * dst_string_map)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(src_string_map, RCUTILS_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(dst_string_map, RCUTILS_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    src_string_map->impl, "source string map is invalid", return RCUTILS_RET_STRING_MAP_INVALID);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    dst_string_map->impl, "destination string map is invalid",
    return RCUTILS_RET_STRING_MAP_INVALID);
  const char * key = rcutils_string_map_get_next_key(src_string_map, NULL);
  while (key != NULL) {
    const char * value = rcutils_string_map_get(src_string_map, key);
    if (NULL == value) {
      RCUTILS_SET_ERROR_MSG("unable to get value for known key, should not happen");
      return RCUTILS_RET_ERROR;
    }
    rcutils_ret_t ret = rcutils_string_map_set(dst_string_map, key, value);
    if (ret != RCUTILS_RET_OK) {
      // error message already set
      return ret;
    }
    key = rcutils_string_map_get_next_key(src_string_map, key);
  }
  return RCUTILS_RET_OK;
}

#ifdef __cplusplus
}
#endif
