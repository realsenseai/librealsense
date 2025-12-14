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

#ifndef RCUTILS__TYPES__HASH_MAP_H_
#define RCUTILS__TYPES__HASH_MAP_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <string.h>

#include "rcutils/allocator.h"
#include "rcutils/types/rcutils_ret.h"
#include "rcutils/macros.h"
#include "rcutils/visibility_control.h"

struct rcutils_hash_map_impl_t;

typedef struct RCUTILS_PUBLIC_TYPE rcutils_hash_map_t
{
  struct rcutils_hash_map_impl_t * impl;
} rcutils_hash_map_t;

/// The function signature for a key hashing function.
/**
 * \param[in] key The key that needs to be hashed
 * \return A hash value for the provided string
 */
typedef size_t (* rcutils_hash_map_key_hasher_t)(
  const void *  // key to hash
);

/// The function signature for a key comparison function.
/**
 * \param[in] val1 The first value to compare
 * \param[in] val2 The second value to compare
 * \return A negative number if val1 < val2, or
 * \return A positve number if val1 > val2, or
 * \return Zero if val1 == val2
 */
typedef int (* rcutils_hash_map_key_cmp_t)(
  const void *,  // val1
  const void *  // val2
);

/**
 * Validates that an rcutils_hash_map_t* points to a valid hash map.
 * \param[in] map A pointer to an rcutils_hash_map_t
 * \return RCUTILS_RET_INVALID_ARGUMENT if map is null
 * \return RCUTILS_RET_NOT_INITIALIZED if map is not initialized
 */
#define HASH_MAP_VALIDATE_HASH_MAP(map) \
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(map, RCUTILS_RET_INVALID_ARGUMENT); \
  if (NULL == map->impl) { \
    RCUTILS_SET_ERROR_MSG("map is not initialized"); \
    return RCUTILS_RET_NOT_INITIALIZED; \
  }

/// A hashing function for a null terminated c string.
/**
 * A hashing function for a null terminated c string.
 * Should be used when your key is just a pointer to a c-string
 */
RCUTILS_PUBLIC
size_t
rcutils_hash_map_string_hash_func(const void * key_str);

/// A comparison function for a null terminated c string.
/**
 * A comparison function for a null terminated c string.
 * Should be used when your key is just a pointer to a c-string
 */
RCUTILS_PUBLIC
int
rcutils_hash_map_string_cmp_func(const void * val1, const void * val2);

/// Return an empty hash_map struct.
/*
 * This function returns an empty and zero initialized hash_map struct.
 * All hash maps should be initialized with this or manually initialized
 * before being used.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * Example:
 * ```c
 * // Do not do this:
 * // rcutils_hash_map_t foo;
 * // rcutils_hash_map_fini(&foo); // undefined behavior!
 *
 * // Do this instead:
 * rcutils_hash_map_t bar = rcutils_get_zero_initialized_hash_map();
 * rcutils_hash_map_fini(&bar); // ok
 * ```
 * */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_hash_map_t
rcutils_get_zero_initialized_hash_map();

/// Initialize a rcutils_hash_map_t, allocating space for given capacity.
/**
 * This function initializes the rcutils_hash_map_t with a given initial
 * capacity for entries.
 * Note this does not allocate space for keys or values in the hash_map, just the
 * arrays of pointers to the keys and values.
 * rcutils_hash_map_set() should still be used when assigning values.
 *
 * The hash_map argument should point to allocated memory and should have
 * been zero initialized with rcutils_get_zero_initialized_hash_map().
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
 * ```c
 * rcutils_hash_map_t hash_map = rcutils_get_zero_initialized_hash_map();
 * rcutils_ret_t ret =
 *   rcutils_hash_map_init(&hash_map, 10, rcutils_get_default_allocator());
 * if (ret != RCUTILS_RET_OK) {
 *   // ... do error handling
 * }
 * // ... use the hash_map and when done:
 * ret = rcutils_hash_map_fini(&hash_map);
 * if (ret != RCUTILS_RET_OK) {
 *   // ... do error handling
 * }
 * ```
 *
 * \param[inout] hash_map rcutils_hash_map_t to be initialized
 * \param[in] initial_capacity the amount of initial capacity for the hash_map
 * \param[in] key_size the size (in bytes) of the key used to index the data
 * \param[in] data_size the size (in bytes) of the data being stored
 * \param[in] key_hashing_func a function that returns a hashed value for a key
 * \param[in] key_cmp_func a function used to compare keys
 * \param[in] allocator the allocator to use through out the lifetime of the hash_map
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_INVALID_ARGUMENT` for invalid arguments, or
 * \return `RCUTILS_RET_BAD_ALLOC` if memory allocation fails, or
 * \return `RCUTILS_RET_STRING_MAP_ALREADY_INIT` if already initialized, or
 * \return `RCUTILS_RET_ERROR` if an unknown error occurs
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_hash_map_init(
  rcutils_hash_map_t * hash_map,
  size_t initial_capacity,
  size_t key_size,
  size_t data_size,
  rcutils_hash_map_key_hasher_t key_hashing_func,
  rcutils_hash_map_key_cmp_t key_cmp_func,
  const rcutils_allocator_t * allocator);

/// Finalize the previously initialized hash_map struct.
/**
 * This function will free any resources which were created when initializing
 * or when calling rcutils_hash_map_set().
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param[inout] hash_map rcutils_hash_map_t to be finalized
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_INVALID_ARGUMENT` for invalid arguments, or
 * \return `RCUTILS_RET_ERROR` if an unknown error occurs
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_hash_map_fini(rcutils_hash_map_t * hash_map);

/// Get the current capacity of the hash_map.
/**
 * This function will return the internal capacity of the hash_map, which is the
 * number of buckets the hash_map uses to sort the keys.
 * The capacity does not indicate how many key value pairs are stored in the
 * hash_map, the rcutils_hash_map_get_size() function can provide that, nor the
 * maximum number that can be stored without increasing the capacity.
 * The capacity can be set initially with rcutils_hash_map_init().
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param[in] hash_map rcutils_hash_map_t to be queried
 * \param[out] capacity capacity of the hash_map
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_INVALID_ARGUMENT` for invalid arguments, or
 * \return `RCUTILS_RET_NOT_INITIALIZED` if the hash_map is invalid, or
 * \return `RCUTILS_RET_ERROR` if an unknown error occurs
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_hash_map_get_capacity(const rcutils_hash_map_t * hash_map, size_t * capacity);

/// Get the current size of the hash_map.
/**
 * This function will return the internal size of the hash_map, which is the
 * current number of key value pairs in the hash_map.
 * The size is changed when calling rcutils_hash_map_set() or rcutils_hash_map_unset().
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param[in] hash_map rcutils_hash_map_t to be queried
 * \param[out] size size of the hash_map
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_INVALID_ARGUMENT` for invalid arguments, or
 * \return `RCUTILS_RET_NOT_INITIALIZED` if the hash_map is invalid, or
 * \return `RCUTILS_RET_ERROR` if an unknown error occurs
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_hash_map_get_size(const rcutils_hash_map_t * hash_map, size_t * size);

/// Set a key value pair in the hash_map, increasing capacity if necessary.
/**
 * If the key already exists in the map then the value is updated to the new value
 * provided. If it does not already exist then a new entry is added for the new key
 * and value. The capacity will be increased if needed.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | Yes
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param[inout] hash_map rcutils_hash_map_t to be updated
 * \param[in] key hash_map key
 * \param[in] value value for given hash_map key
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_INVALID_ARGUMENT` for invalid arguments, or
 * \return `RCUTILS_RET_BAD_ALLOC` if memory allocation fails, or
 * \return `RCUTILS_RET_NOT_INITIALIZED` if the hash_map is invalid, or
 * \return `RCUTILS_RET_ERROR` if an unknown error occurs
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_hash_map_set(rcutils_hash_map_t * hash_map, const void * key, const void * value);

/// Unset a key value pair in the hash_map.
/**
 * Unsets the key value pair in the hash_map and frees any internal resources allocated
 * for the entry. This function will never decrease the capacity when removing keys.
 * If the given key is not found, RCUTILS_RET_STRING_KEY_NOT_FOUND is returned.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param[inout] hash_map rcutils_hash_map_t to be updated
 * \param[in] key hash_map key, must be null terminated c string
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_INVALID_ARGUMENT` for invalid arguments, or
 * \return `RCUTILS_RET_NOT_INITIALIZED` if the hash_map is invalid, or
 * \return `RCUTILS_RET_STRING_KEY_NOT_FOUND` if the key is not found in the map, or
 * \return `RCUTILS_RET_ERROR` if an unknown error occurs
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_hash_map_unset(rcutils_hash_map_t * hash_map, const void * key);

/// Get whether or not a key exists.
/**
 * Returns true if the provided key exists in the hash_map or false if it does not or
 * if the hash_map or key are invalid.
 * In all cases no error message is set.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param[in] hash_map rcutils_hash_map_t to be searched
 * \param[in] key hash_map key, must be null terminated c string
 * \return `true` if key is in the hash_map, or
 * \return `false` if key is not in the hash_map, or
 * \return `false` for invalid arguments, or
 * \return `false` if the hash_map is invalid
 */
RCUTILS_PUBLIC
bool
rcutils_hash_map_key_exists(const rcutils_hash_map_t * hash_map, const void * key);

/// Get value given a key.
/**
 * This function can be used to retrieve a shallow copy of the stored data. The data
 * pointer must point to a section of memory large enough to copy the full size of
 * the data being stored, which is specified when the hash_map in initialized.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param[in] hash_map rcutils_hash_map_t to be searched
 * \param[in] key hash_map key to look up the data for
 * \param[out] data A copy of the data stored in the map
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_INVALID_ARGUMENT` for invalid arguments, or
 * \return `RCUTILS_RET_NOT_INITIALIZED` if the hash_map is invalid, or
 * \return `RCUTILS_RET_NOT_FOUND` if the key doesn't exist in the map, or
 * \return `RCUTILS_RET_ERROR` if an unknown error occurs
 */
RCUTILS_PUBLIC
rcutils_ret_t
rcutils_hash_map_get(const rcutils_hash_map_t * hash_map, const void * key, void * data);

/// Get the next key in the hash_map, unless NULL is given, then get the first key.
/**
 * This function allows you to iteratively get each key/value pair in the hash_map.
 *
 * If NULL is given for the previous_key, then the first key in the hash_map is returned.
 * If that returned key is given as the previous_key for the next call to this function,
 * then the next key in the hash_map is returned.
 * If there are no more keys in the hash_map or if the given key is not in the hash_map,
 * an error will be returned.
 *
 * The order of the keys in the hash_map is arbitrary and if the hash_map is modified
 * between calls to this function the behavior is undefined.
 * If the hash_map is modified then iteration should begin again by passing NULL to
 * get the first key again.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * Example:
 * ```c
 * printf("entries in the hash_map:\n");
 * int key = 0, data = 0;
 * rcutils_ret_t status = rcutils_hash_map_get_next_key(&hash_map, NULL, &key, &data);
 * while (RCUTILS_RET_OK == status) {
 *   printf("%i: %i\n", key, data);
 *   status = rcutils_hash_map_get_next_key(&hash_map, &key, &key, &data);
 * }
 * ```
 *
 * \param[in] hash_map rcutils_hash_map_t to be queried
 * \param[in] previous_key NULL to get the first key or the previous key to get the next for
 * \param[out] key A copy of the next key in the sequence
 * \param[out] data A copy of the next data in the sequence
 * \return `RCUTILS_RET_OK` if successful, or
 * \return `RCUTILS_RET_INVALID_ARGUMENT` for invalid arguments, or
 * \return `RCUTILS_RET_NOT_INITIALIZED` if the hash_map is invalid, or
 * \return `RCUTILS_RET_NOT_FOUND` if the previous_key doesn't exist in the map, or
 * \return `RCUTILS_RET_HASH_MAP_NO_MORE_ENTRIES` if there is no more data beyound the previous_key, or
 * \return `RCUTILS_RET_ERROR` if an unknown error occurs
 */
RCUTILS_PUBLIC
rcutils_ret_t
rcutils_hash_map_get_next_key_and_data(
  const rcutils_hash_map_t * hash_map,
  const void * previous_key,
  void * key,
  void * data);


#ifdef __cplusplus
}
#endif

#endif  // RCUTILS__TYPES__HASH_MAP_H_
