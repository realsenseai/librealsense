// Copyright 2018-2019 Open Source Robotics Foundation, Inc.
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

#include <string.h>

#include "rcutils/allocator.h"
#include "rcutils/error_handling.h"
#include "rcutils/macros.h"
#include "rcutils/types/array_list.h"
#include "rcutils/types/rcutils_ret.h"
#include "rcutils/visibility_control.h"

#define ARRAY_LIST_VALIDATE_INDEX_IN_BOUNDS(array_list, index) \
  if (array_list->impl->size <= index) { \
    RCUTILS_SET_ERROR_MSG("index is out of bounds of the list"); \
    return RCUTILS_RET_INVALID_ARGUMENT; \
  }

typedef struct rcutils_array_list_impl_t
{
  size_t size;
  size_t capacity;
  void * list;
  size_t data_size;
  rcutils_allocator_t allocator;
} rcutils_array_list_impl_t;

rcutils_array_list_t
rcutils_get_zero_initialized_array_list(void)
{
  static rcutils_array_list_t zero_initialized_array_list = {NULL};
  return zero_initialized_array_list;
}

rcutils_ret_t
rcutils_array_list_init(
  rcutils_array_list_t * array_list,
  size_t initial_capacity,
  size_t data_size,
  const rcutils_allocator_t * allocator)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(array_list, RCUTILS_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ALLOCATOR(allocator, return RCUTILS_RET_INVALID_ARGUMENT);
  if (NULL != array_list->impl) {
    RCUTILS_SET_ERROR_MSG("array_list is already initialized");
    return RCUTILS_RET_INVALID_ARGUMENT;
  } else if (1 > initial_capacity) {
    RCUTILS_SET_ERROR_MSG("initial_capacity cannot be less than 1");
    return RCUTILS_RET_INVALID_ARGUMENT;
  } else if (1 > data_size) {
    RCUTILS_SET_ERROR_MSG("data_size cannot be less than 1");
    return RCUTILS_RET_INVALID_ARGUMENT;
  }

  array_list->impl = allocator->allocate(sizeof(rcutils_array_list_impl_t), allocator->state);
  if (NULL == array_list->impl) {
    RCUTILS_SET_ERROR_MSG("failed to allocate memory for array list impl");
    return RCUTILS_RET_BAD_ALLOC;
  }

  array_list->impl->capacity = initial_capacity;
  array_list->impl->size = 0;
  array_list->impl->data_size = data_size;
  array_list->impl->list = allocator->allocate(initial_capacity * data_size, allocator->state);
  if (NULL == array_list->impl->list) {
    allocator->deallocate(array_list->impl, allocator->state);
    array_list->impl = NULL;
    RCUTILS_SET_ERROR_MSG("failed to allocate memory for array list data");
    return RCUTILS_RET_BAD_ALLOC;
  }
  array_list->impl->allocator = *allocator;

  return RCUTILS_RET_OK;
}

rcutils_ret_t
rcutils_array_list_fini(rcutils_array_list_t * array_list)
{
  ARRAY_LIST_VALIDATE_ARRAY_LIST(array_list);

  array_list->impl->allocator.deallocate(array_list->impl->list, array_list->impl->allocator.state);
  array_list->impl->allocator.deallocate(array_list->impl, array_list->impl->allocator.state);
  array_list->impl = NULL;

  return RCUTILS_RET_OK;
}

static rcutils_ret_t rcutils_array_list_increase_capacity(rcutils_array_list_t * array_list)
{
  size_t new_capacity = 2 * array_list->impl->capacity;
  size_t new_size = array_list->impl->data_size * new_capacity;
  void * new_list = array_list->impl->allocator.reallocate(
    array_list->impl->list,
    new_size,
    array_list->impl->allocator.state);
  if (NULL == new_list) {
    return RCUTILS_RET_BAD_ALLOC;
  }
  array_list->impl->list = new_list;
  array_list->impl->capacity = new_capacity;
  return RCUTILS_RET_OK;
}

static uint8_t * rcutils_array_list_get_pointer_for_index(
  const rcutils_array_list_t * array_list,
  size_t index)
{
  uint8_t * list_start = array_list->impl->list;
  uint8_t * index_ptr = list_start + (array_list->impl->data_size * index);
  return index_ptr;
}

rcutils_ret_t
rcutils_array_list_add(rcutils_array_list_t * array_list, const void * data)
{
  ARRAY_LIST_VALIDATE_ARRAY_LIST(array_list);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(data, RCUTILS_RET_INVALID_ARGUMENT);
  rcutils_ret_t ret = RCUTILS_RET_OK;

  if (array_list->impl->size + 1 > array_list->impl->capacity) {
    ret = rcutils_array_list_increase_capacity(array_list);
    if (RCUTILS_RET_OK != ret) {
      return ret;
    }
  }

  uint8_t * index_ptr =
    rcutils_array_list_get_pointer_for_index(array_list, array_list->impl->size);
  memcpy(index_ptr, data, array_list->impl->data_size);

  array_list->impl->size++;
  return ret;
}

rcutils_ret_t
rcutils_array_list_set(rcutils_array_list_t * array_list, size_t index, const void * data)
{
  ARRAY_LIST_VALIDATE_ARRAY_LIST(array_list);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(data, RCUTILS_RET_INVALID_ARGUMENT);
  ARRAY_LIST_VALIDATE_INDEX_IN_BOUNDS(array_list, index);

  uint8_t * index_ptr = rcutils_array_list_get_pointer_for_index(array_list, index);
  memcpy(index_ptr, data, array_list->impl->data_size);

  return RCUTILS_RET_OK;
}

rcutils_ret_t
rcutils_array_list_remove(rcutils_array_list_t * array_list, size_t index)
{
  ARRAY_LIST_VALIDATE_ARRAY_LIST(array_list);
  ARRAY_LIST_VALIDATE_INDEX_IN_BOUNDS(array_list, index);

  // Shift all the data in the list to replace the missing data
  for (size_t i = index; i < (array_list->impl->size - 1); ++i) {
    uint8_t * dst_ptr = rcutils_array_list_get_pointer_for_index(array_list, i);
    uint8_t * src_ptr = rcutils_array_list_get_pointer_for_index(array_list, i + 1);
    memcpy(dst_ptr, src_ptr, array_list->impl->data_size);
  }

  array_list->impl->size--;
  return RCUTILS_RET_OK;
}

rcutils_ret_t
rcutils_array_list_get(const rcutils_array_list_t * array_list, size_t index, void * data)
{
  ARRAY_LIST_VALIDATE_ARRAY_LIST(array_list);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(data, RCUTILS_RET_INVALID_ARGUMENT);
  ARRAY_LIST_VALIDATE_INDEX_IN_BOUNDS(array_list, index);

  uint8_t * index_ptr = rcutils_array_list_get_pointer_for_index(array_list, index);
  memcpy(data, index_ptr, array_list->impl->data_size);

  return RCUTILS_RET_OK;
}

rcutils_ret_t
rcutils_array_list_get_size(const rcutils_array_list_t * array_list, size_t * size)
{
  ARRAY_LIST_VALIDATE_ARRAY_LIST(array_list);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(size, RCUTILS_RET_INVALID_ARGUMENT);
  *size = array_list->impl->size;
  return RCUTILS_RET_OK;
}

#ifdef __cplusplus
}
#endif
