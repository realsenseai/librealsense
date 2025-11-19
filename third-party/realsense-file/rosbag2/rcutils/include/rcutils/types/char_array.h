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

#ifndef RCUTILS__TYPES__CHAR_ARRAY_H_
#define RCUTILS__TYPES__CHAR_ARRAY_H_

#if __cplusplus
extern "C"
{
#endif

#include <stdarg.h>

#include "rcutils/allocator.h"
#include "rcutils/types/rcutils_ret.h"
#include "rcutils/visibility_control.h"

typedef struct RCUTILS_PUBLIC_TYPE rcutils_char_array_t
{
  char * buffer;

  /**
   * if this is true, we may safely free/realloc the buffer as needed;
   * otherwise we will leave the buffer alone and alloc new memory if
   * more space is needed
   */
  bool owns_buffer;

  size_t buffer_length;
  size_t buffer_capacity;
  rcutils_allocator_t allocator;
} rcutils_char_array_t;

/// Return a zero initialized char array struct.
/**
 * \return rcutils_char_array_t a zero initialized char array struct
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_char_array_t
rcutils_get_zero_initialized_char_array(void);

/// Initialize a zero initialized char array struct.
/**
 * This function may leak if the char array struct is already
 * pre-initialized.
 * If the capacity is set to 0, no memory is allocated and the internal buffer
 * is still NULL.
 *
 * \param[in] char_array a pointer to the to be initialized char array struct
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
rcutils_char_array_init(
  rcutils_char_array_t * char_array,
  size_t buffer_capacity,
  const rcutils_allocator_t * allocator);

/// Finalize a char array struct.
/**
 * Cleans up and deallocates any resources owned by rcutils_char_array_t.
 * The array passed to this function needs to have been initialized by
 * rcutils_char_array_init().
 * If .owns_buffer is false, this function has no effect because that
 * implies that the char_array does not own the internal buffer.
 * Passing an uninitialized instance to this function leads to undefined
 * behavior.
 *
 * \param[in] char_array pointer to the rcutils_char_array_t to be cleaned up
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_INVALID_ARGUMENTS` if the char_array argument is invalid
 * \return `RCUTILS_RET_ERROR` if an unexpected error occurs
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_char_array_fini(rcutils_char_array_t * char_array);

/// Resize the internal buffer of the char array.
/**
 * The internal buffer of the char array can be resized dynamically if needed.
 * If the new size is smaller than the current capacity, then the memory is
 * truncated.
 * Be aware, that this will deallocate the memory and therefore invalidates any
 * pointers to this storage.
 * If the new size is larger, new memory is getting allocated and the existing
 * content is copied over.
 * Note that if the array doesn't own the current buffer the function just
 * allocates a new block of memory and copies the contents of the old buffer
 * instead of resizing the existing buffer.
 *
 * \param[in] char_array pointer to the instance of rcutils_char_array_t which is being resized
 * \param[in] new_size the new size of the internal buffer
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_INVALID_ARGUMENT` if new_size is set to zero
 * \return `RCUTILS_RET_BAD_ALLOC` if memory allocation failed, or
 * \return `RCUTILS_RET_ERROR` if an unexpected error occurs
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_char_array_resize(rcutils_char_array_t * char_array, size_t new_size);

/// Expand the internal buffer of the char array.
/**
 * This function is equivalent to `rcutils_char_array_resize` except that it resizes
 * the internal buffer only when it is not big enough.
 * If the buffer is already big enough for `new_size`, it returns `RCUTILS_RET_OK` without
 * doing anything.
 *
 * \param[inout] char_array pointer to the instance of rcutils_char_array_t which is being resized
 * \param[in] new_size the new size of the internal buffer
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_BAD_ALLOC` if memory allocation failed, or
 * \return `RCUTILS_RET_ERROR` if an unexpected error occurs
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_char_array_expand_as_needed(rcutils_char_array_t * char_array, size_t new_size);

/// Produce output according to format and args.
/**
 * This function is equivalent to `vsprintf(char_array->buffer, format, args)`
 * except that the buffer grows as needed so a user doesn't have to deal with
 * memory management.
 * The `va_list args` will be cloned before being used, so a user can safely
 * use it again after calling this function.
 *
 * \param[inout] char_array pointer to the instance of rcutils_char_array_t which is being
 * written to
 * \param[in] format the format string used by the underlying `vsnprintf`
 * \param[in] args the `va_list` used by the underlying `vsnprintf`
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_BAD_ALLOC` if memory allocation failed, or
 * \return `RCUTILS_RET_ERROR` if an unexpected error occurs
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_char_array_vsprintf(rcutils_char_array_t * char_array, const char * format, va_list args);

/// Append a string (or part of it) to the string in buffer.
/**
 * This function treats the internal buffer as a string and appends the src string to it.
 * If src is longer than n, n bytes will be used and an extra null byte will be appended.
 * It is virtually equivalent to `strncat(char_array->buffer, src, n)` except that the buffer
 * grows as needed so a user doesn't have to deal with memory management.
 *
 * \param[inout] char_array pointer to the instance of rcutils_char_array_t which is being appended to
 * \param[in] src the string to be appended to the end of the string in buffer
 * \param[in] n it uses at most n bytes from the src string
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_BAD_ALLOC` if memory allocation failed, or
 * \return `RCUTILS_RET_ERROR` if an unexpected error occurs
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_char_array_strncat(rcutils_char_array_t * char_array, const char * src, size_t n);

/// Append a string to the string in buffer.
/**
 * This function treats the internal buffer as a string and appends the src string to it.
 * It is virtually equivalent to `strcat(char_array->buffer, src)` except that the buffer
 * grows as needed. That is to say, a user can safely use it without doing calculation or
 * checks on the sizes of the src and buffer.
 *
 * \param[inout] char_array pointer to the instance of rcutils_char_array_t which is being
 * appended to
 * \param[in] src the string to be appended to the end of the string in buffer
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_BAD_ALLOC` if memory allocation failed, or
 * \return `RCUTILS_RET_ERROR` if an unexpected error occurs
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_char_array_strcat(rcutils_char_array_t * char_array, const char * src);

/// Copy memory to buffer.
/**
 * This function is equivalent to `memcpy(char_array->buffer, src, n)` except that the buffer
 * grows as needed so a user doesn't have to worry about overflow.
 *
 * \param[inout] char_array pointer to the instance of rcutils_char_array_t which is being resized
 * \param[in] src the memory to be copied from
 * \param[in] n a total of n bytes will be copied
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_BAD_ALLOC` if memory allocation failed, or
 * \return `RCUTILS_RET_ERROR` if an unexpected error occurs
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_char_array_memcpy(rcutils_char_array_t * char_array, const char * src, size_t n);

/// Copy a string to buffer.
/**
 * This function is equivalent to `strcpy(char_array->buffer, src)` except that the buffer
 * grows as needed so that `src` will fit without overflow.
 *
 * \param[inout] char_array pointer to the instance of rcutils_char_array_t which is being
 * copied to
 * \param[in] src the string to be copied from
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_BAD_ALLOC` if memory allocation failed, or
 * \return `RCUTILS_RET_ERROR` if an unexpected error occurs
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_char_array_strcpy(rcutils_char_array_t * char_array, const char * src);

#if __cplusplus
}
#endif

#endif  // RCUTILS__TYPES__CHAR_ARRAY_H_
