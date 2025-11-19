// Copyright 2015 Open Source Robotics Foundation, Inc.
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

#ifndef RCUTILS__ALLOCATOR_H_
#define RCUTILS__ALLOCATOR_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>

#include "rcutils/macros.h"
#include "rcutils/types/rcutils_ret.h"
#include "rcutils/visibility_control.h"

/// Encapsulation of an allocator.
/**
 * The default allocator uses malloc(), free(), calloc(), and realloc().
 * It can be obtained using rcutils_get_default_allocator().
 *
 * The allocator should be trivially copyable.
 * Meaning that the struct should continue to work after being assignment
 * copied into a new struct.
 * Specifically the object pointed to by the state pointer should remain valid
 * until all uses of the allocator have been made.
 * Particular care should be taken when giving an allocator to functions like
 * rcutils_*_init() where it is stored within another object and used later.
 * Developers should note that, while the fields of a const-qualified allocator
 * struct cannot be modified, the state of the allocator can be modified.
 */
typedef struct rcutils_allocator_t
{
  /// Allocate memory, given a size and the `state` pointer.
  /** An error should be indicated by returning `NULL`. */
  void * (*allocate)(size_t size, void * state);
  /// Deallocate previously allocated memory, mimicking free().
  /** Also takes the `state` pointer. */
  void (* deallocate)(void * pointer, void * state);
  /// Reallocate if possible, otherwise it deallocates and allocates.
  /**
   * Also takes the `state` pointer.
   *
   * If unsupported then do deallocate and then allocate.
   * This should behave as realloc() does, as opposed to posix's
   * [reallocf](https://linux.die.net/man/3/reallocf), i.e. the memory given
   * by pointer will not be free'd automatically if realloc() fails.
   * For reallocf-like behavior use rcutils_reallocf().
   * This function must be able to take an input pointer of `NULL` and succeed.
   */
  void * (*reallocate)(void * pointer, size_t size, void * state);
  /// Allocate memory with all elements set to zero, given a number of elements and their size.
  /** An error should be indicated by returning `NULL`. */
  void * (*zero_allocate)(size_t number_of_elements, size_t size_of_element, void * state);
  /// Implementation defined state storage.
  /**
   * This is passed as the final parameter to other allocator functions.
   * Note that the contents of the state can be modified even in const-qualified
   * allocator objects.
   */
  void * state;
} rcutils_allocator_t;

/// Return a zero initialized allocator.
/**
 * Note that this is an invalid allocator and should only be used as a placeholder.
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_allocator_t
rcutils_get_zero_initialized_allocator(void);

/// Return a properly initialized rcutils_allocator_t with default values.
/**
 * This defaults to:
 *
 * - allocate = wraps malloc()
 * - deallocate = wraps free()
 * - reallocate = wraps realloc()
 * - zero_allocate = wraps calloc()
 * - state = `NULL`
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | Yes
 * Uses Atomics       | No
 * Lock-Free          | Yes
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_allocator_t
rcutils_get_default_allocator(void);

/// Return true if the given allocator has non-null function pointers.
/**
 * Will also return false if the allocator pointer is null.
 *
 * \param[in] allocator to be checked by the function
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
bool
rcutils_allocator_is_valid(const rcutils_allocator_t * allocator);

#define RCUTILS_CHECK_ALLOCATOR(allocator, fail_statement) \
  if (!rcutils_allocator_is_valid(allocator)) { \
    fail_statement; \
  }

#define RCUTILS_CHECK_ALLOCATOR_WITH_MSG(allocator, msg, fail_statement) \
  if (!rcutils_allocator_is_valid(allocator)) { \
    RCUTILS_SET_ERROR_MSG(msg); \
    fail_statement; \
  }

/// Emulate the behavior of [reallocf](https://linux.die.net/man/3/reallocf).
/**
 * This function will return `NULL` if the allocator is `NULL` or has `NULL` for
 * function pointer fields.
 * \param[inout] pointer to the memory which will be reallocated
 * \param[in] size in bytes
 * \param[in] allocator to be used to allocate and deallocate memory
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
void *
rcutils_reallocf(void * pointer, size_t size, rcutils_allocator_t * allocator);

#ifdef __cplusplus
}
#endif

#endif  // RCUTILS__ALLOCATOR_H_
