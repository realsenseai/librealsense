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

#ifndef RCUTILS__TYPES__ARRAY_LIST_H_
#define RCUTILS__TYPES__ARRAY_LIST_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <string.h>

#include "rcutils/allocator.h"
#include "rcutils/macros.h"
#include "rcutils/types/rcutils_ret.h"
#include "rcutils/visibility_control.h"

struct rcutils_array_list_impl_t;

typedef struct RCUTILS_PUBLIC_TYPE rcutils_array_list_t
{
  struct rcutils_array_list_impl_t * impl;
} rcutils_array_list_t;

/**
 * Validates that an rcutils_array_list_t* points to a valid array list.
 * \param[in] array_list A pointer to an rcutils_array_list_t
 * \return RCUTILS_RET_INVALID_ARGUMENT if array_list is null
 * \return RCUTILS_RET_NOT_INITIALIZED if array_list is not initialized
 */
#define ARRAY_LIST_VALIDATE_ARRAY_LIST(array_list) \
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(array_list, RCUTILS_RET_INVALID_ARGUMENT); \
  if (NULL == array_list->impl) { \
    RCUTILS_SET_ERROR_MSG("array_list is not initialized"); \
    return RCUTILS_RET_NOT_INITIALIZED; \
  }

/// Return an empty array_list struct.
/**
 * This function returns an empty and zero initialized array_list struct.
 * Calling rcutils_array_list_fini() on any non-initialized instance leads
 * to undefined behavior.
 * Every instance of array_list_t has to either be zero_initialized with this
 * function or manually allocated.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | Yes
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * Example:
 *
 * ```c
 * rcutils_array_list_t foo;
 * rcutils_array_list_fini(&foo); // undefined behavior!
 *
 * rcutils_array_list_t bar = rcutils_get_zero_initialized_array_list();
 * rcutils_array_list_fini(&bar); // ok
 * ```
 */
RCUTILS_PUBLIC
rcutils_array_list_t
rcutils_get_zero_initialized_array_list(void);

/// Initialize an array list with a given initial capacity.
/**
 * This function will initialize a given, zero initialized, array_list to
 * a given size.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | Yes
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * Example:
 *
 * ```c
 * rcutils_allocator_t allocator = rcutils_get_default_allocator();
 * rcutils_array_list_t array_list = rcutils_get_zero_initialized_array_list();
 * rcutils_ret_t ret = rcutils_array_list_init(&array_list, 2, sizeof(int), &allocator);
 * if (ret != RCUTILS_RET_OK) {
 *   // ... error handling
 * }
 * int data = 42;
 * int out_data = 0;
 * ret = rcutils_array_list_add(&array_list, &data);
 * data++;
 * ret = rcutils_array_list_get(&array_list, 0, &out_data);
 * assert(42 == out_data);
 * ret = rcutils_array_list_fini(&array_list);
 * ```
 *
 * \param[inout] array_list object to be initialized
 * \param[in] initial_capacity the initial capacity to allocate in the list
 * \param[in] data_size the size (in bytes) of the data object being stored in the list
 * \param[in] allocator to be used to allocate and deallocate memory
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_INVALID_ARGUMENT` for invalid arguments, or
 * \return `RCUTILS_RET_BAD_ALLOC` if memory allocation fails, or
 * \return `RCUTILS_RET_ERROR` if an unknown error occurs
 */
RCUTILS_PUBLIC
rcutils_ret_t
rcutils_array_list_init(
  rcutils_array_list_t * array_list,
  size_t initial_capacity,
  size_t data_size,
  const rcutils_allocator_t * allocator);

/// Finalize an array list, reclaiming all resources.
/**
 * This function reclaims any memory owned by the array list.
 *
 * The allocator used to initialize the array list is used to deallocate each
 * entry in the list and the list itself.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param[inout] array_list object to be finalized
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_INVALID_ARGUMENT` for invalid arguments, or
 * \return `RCUTILS_RET_ERROR` if an unknown error occurs
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_array_list_fini(rcutils_array_list_t * array_list);

/// Adds an entry to the list
/**
 * This function adds the provided data to the end of the list. A shallow copy of
 * the provided data is made to store in the list instead of just storing
 * the pointer to the provided data.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | Yes
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param[in] array_list to add the data to
 * \param[in] data a pointer to the data to add to the list
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_INVALID_ARGUMENT` for invalid arguments, or
 * \return `RCUTILS_RET_BAD_ALLOC` if memory allocation fails, or
 * \return `RCUTILS_RET_ERROR` if an unknown error occurs
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_array_list_add(rcutils_array_list_t * array_list, const void * data);

/// Sets an entry in the list to the provided data
/**
 * This function sets the provided data at the specified index in the list.
 * A shallow copy of the provided data is made to store in the list instead
 * of just storing the pointer to the provided data.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param[in] array_list to add the data to
 * \param[in] index the position in the list to set the data
 * \param[in] data a pointer to the data that will be set in the list
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_INVALID_ARGUMENT` for invalid arguments, or
 * \return `RCUTILS_RET_INVALID_ARGUMENT` if index out of bounds, or
 * \return `RCUTILS_RET_ERROR` if an unknown error occurs
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_array_list_set(rcutils_array_list_t * array_list, size_t index, const void * data);

/// Removes an entry in the list at the provided index
/**
 * This function removes data from the list at the specified index. The capacity
 * of the list will never decrease when entries are removed.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param[in] array_list to add the data to
 * \param[in] index the index of the item to remove from the list
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_INVALID_ARGUMENT` for invalid arguments, or
 * \return `RCUTILS_RET_INVALID_ARGUMENT` if index out of bounds, or
 * \return `RCUTILS_RET_ERROR` if an unknown error occurs
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_array_list_remove(rcutils_array_list_t * array_list, size_t index);

/// Retrieves an entry in the list at the provided index
/**
 * This function retrieves a copy of the data stored in the list at the provided index.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 *
 * \param[in] array_list to add the data to
 * \param[in] index the index at which to get the data
 * \param[out] data a copy of the data stored in the list
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_INVALID_ARGUMENT` for invalid arguments, or
 * \return `RCUTILS_RET_ERROR` if an unknown error occurs
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_array_list_get(const rcutils_array_list_t * array_list, size_t index, void * data);

/// Retrieves the size of the provided array_list
/**
 * This function retrieves the number of items in the provided array list
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 *
 * \param[in] array_list list to get the size of
 * \param[out] size The number of items currently stored in the list
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_INVALID_ARGUMENT` for invalid arguments, or
 * \return `RCUTILS_RET_ERROR` if an unknown error occurs
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_array_list_get_size(const rcutils_array_list_t * array_list, size_t * size);

#ifdef __cplusplus
}
#endif

#endif  // RCUTILS__TYPES__ARRAY_LIST_H_
