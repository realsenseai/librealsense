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

#ifndef RCUTILS__TYPES__UINT8_ARRAY_H_
#define RCUTILS__TYPES__UINT8_ARRAY_H_

#if __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#include "rcutils/allocator.h"
#include "rcutils/types/rcutils_ret.h"
#include "rcutils/visibility_control.h"

typedef struct RCUTILS_PUBLIC_TYPE rcutils_uint8_array_t
{
  uint8_t * buffer;
  size_t buffer_length;
  size_t buffer_capacity;
  rcutils_allocator_t allocator;
} rcutils_uint8_array_t;

/// Return a zero initialized uint8 array struct.
/**
 * \return rcutils_uint8_array_t a zero initialized uint8 array struct
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_uint8_array_t
rcutils_get_zero_initialized_uint8_array(void);

/// Initialize a zero initialized uint8 array struct.
/**
 * This function may leak if the uint8 array struct is already initialized.
 * If the capacity is set to 0, no memory is allocated and the internal buffer
 * is still NULL.
 *
 * \param[inout] uint8_array a pointer to the to be initialized uint8 array struct
 * \param[in] buffer_capacity the size of the memory to allocate for the byte stream
 * \param[in] allocator the allocator to use for the memory allocation
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_INVALID_ARGUMENTS` if any arguments are invalid, or
 * \return 'RCUTILS_RET_BAD_ALLOC` if no memory could be allocated correctly
 * \return `RCUTILS_RET_ERROR` if an unexpected error occurs
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_uint8_array_init(
  rcutils_uint8_array_t * uint8_array,
  size_t buffer_capacity,
  const rcutils_allocator_t * allocator);

/// Finalize a uint8 array struct.
/**
 * Cleans up and deallocates any resources used in a rcutils_uint8_array_t.
 * The array passed to this function needs to have been initialized by
 * rcutils_uint8_array_init().
 * Passing an uninitialized instance to this function leads to undefined
 * behavior.
 *
 * \param[in] uint8_array pointer to the rcutils_uint8_array_t to be cleaned up
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_INVALID_ARGUMENTS` if the uint8_array argument is invalid
 * \return `RCUTILS_RET_ERROR` if an unexpected error occurs
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_uint8_array_fini(rcutils_uint8_array_t * uint8_array);

/// Resize the internal buffer of the uint8 array.
/**
 * The internal buffer of the uint8 array can be resized dynamically if needed.
 * If the new size is smaller than the current capacity, then the memory is
 * truncated.
 * Be aware, that this might deallocate the memory and therefore invalidates any
 * pointers to this storage.
 *
 * \param[inout] uint8_array pointer to the instance of rcutils_uint8_array_t which is
 * being resized
 * \param[in] new_size the new size of the internal buffer
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_INVALID_ARGUMENT` if new_size is set to zero
 * \return `RCUTILS_RET_BAD_ALLOC` if memory allocation failed, or
 * \return `RCUTILS_RET_ERROR` if an unexpected error occurs
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_uint8_array_resize(rcutils_uint8_array_t * uint8_array, size_t new_size);

#if __cplusplus
}
#endif

#endif  // RCUTILS__TYPES__UINT8_ARRAY_H_
