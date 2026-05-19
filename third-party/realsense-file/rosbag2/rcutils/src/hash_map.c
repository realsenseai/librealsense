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
#include <stdio.h>

#include "rcutils/allocator.h"
#include "rcutils/error_handling.h"
#include "rcutils/logging_macros.h"
#include "rcutils/types/array_list.h"
#include "rcutils/types/hash_map.h"
#include "rcutils/types/rcutils_ret.h"
#include "rcutils/macros.h"
#include "rcutils/visibility_control.h"

#define LOAD_FACTOR         (0.75)
#define BUCKET_INITIAL_CAP  ((size_t)2)

typedef struct rcutils_hash_map_entry_t
{
  size_t hashed_key;
  void * key;
  void * value;
} rcutils_hash_map_entry_t;

typedef struct rcutils_hash_map_impl_t
{
  // This is the array of buckets that will store the keypairs
  rcutils_array_list_t * map;
  size_t capacity;
  size_t size;
  size_t key_size;
  size_t data_size;
  rcutils_hash_map_key_hasher_t key_hashing_func;
  rcutils_hash_map_key_cmp_t key_cmp_func;
  rcutils_allocator_t allocator;
} rcutils_hash_map_impl_t;

// djb2 hash function
size_t rcutils_hash_map_string_hash_func(const void * key_str)
{
  const char ** ckey_ptr = (const char **) key_str;
  const char * ckey_str = *ckey_ptr;
  size_t hash = 5381;

  while ('\0' != *ckey_str) {
    const char c = *(ckey_str++);
    hash = ((hash << 5) + hash) + (size_t)c; /* hash * 33 + c */
  }

  return hash;
}

int rcutils_hash_map_string_cmp_func(const void * val1, const void * val2)
{
  const char ** cval1 = (const char **) val1;
  const char ** cval2 = (const char **) val2;
  return strcmp(*cval1, *cval2);
}

rcutils_hash_map_t
rcutils_get_zero_initialized_hash_map()
{
  static rcutils_hash_map_t zero_initialized_hash_map = {NULL};
  return zero_initialized_hash_map;
}

// Allocates a new zero initialized hashmap
static rcutils_ret_t hash_map_allocate_new_map(
  rcutils_array_list_t ** map, size_t capacity,
  const rcutils_allocator_t * allocator)
{
  *map = allocator->allocate(capacity * sizeof(rcutils_hash_map_impl_t), allocator->state);
  if (NULL == *map) {
    return RCUTILS_RET_BAD_ALLOC;
  }

  // Make sure the list for every bucket is zero initialized
  rcutils_array_list_t zero_list = rcutils_get_zero_initialized_array_list();
  for (size_t i = 0; i < capacity; ++i) {
    (*map)[i] = zero_list;
  }

  return RCUTILS_RET_OK;
}

// Deallocates the memory for a map entry
static void hash_map_deallocate_entry(
  rcutils_allocator_t * allocator,
  rcutils_hash_map_entry_t * entry)
{
  if (NULL != entry) {
    allocator->deallocate(entry->key, allocator->state);
    allocator->deallocate(entry->value, allocator->state);
    allocator->deallocate(entry, allocator->state);
  }
}

// Deallocates an existing hashmap
static rcutils_ret_t hash_map_deallocate_map(
  rcutils_array_list_t * map, size_t capacity,
  rcutils_allocator_t * allocator, bool dealloc_map_entries)
{
  rcutils_ret_t ret = RCUTILS_RET_OK;
  for (size_t i = 0; i < capacity && RCUTILS_RET_OK == ret; ++i) {
    // If an array list is allocated for this bucket then clean it up
    rcutils_array_list_t * bucket = &(map[i]);
    if (NULL != bucket->impl) {
      // if selected then deallocate the memory for the actual entries as well
      if (dealloc_map_entries) {
        size_t bucket_size = 0;
        ret = rcutils_array_list_get_size(bucket, &bucket_size);
        for (size_t b_i = 0; b_i < bucket_size && RCUTILS_RET_OK == ret; ++b_i) {
          rcutils_hash_map_entry_t * entry;
          ret = rcutils_array_list_get(bucket, b_i, &entry);
          if (RCUTILS_RET_OK == ret) {
            hash_map_deallocate_entry(allocator, entry);
          }
        }
      }

      // Cleanup the actual list itself
      if (RCUTILS_RET_OK == ret) {
        ret = rcutils_array_list_fini(bucket);
      }
    }
  }

  // Cleanup the memory allocated for the array of buckets
  if (RCUTILS_RET_OK == ret) {
    allocator->deallocate(map, allocator->state);
  }

  return ret;
}

// Inserts a new entry into a bucket
static rcutils_ret_t hash_map_insert_entry(
  rcutils_array_list_t * map,
  size_t bucket_index,
  const rcutils_hash_map_entry_t * entry,
  rcutils_allocator_t * allocator)
{
  rcutils_array_list_t * bucket = &(map[bucket_index]);
  rcutils_ret_t ret = RCUTILS_RET_OK;

  // If we have initialized this bucket yet then do so
  if (NULL == bucket->impl) {
    ret = rcutils_array_list_init(
      bucket, BUCKET_INITIAL_CAP, sizeof(rcutils_hash_map_entry_t *), allocator);
  }

  if (RCUTILS_RET_OK == ret) {
    ret = rcutils_array_list_add(bucket, &entry);
  }

  return ret;
}

// Checks if map is already past its load factor and grows it if so
static rcutils_ret_t hash_map_check_and_grow_map(rcutils_hash_map_t * hash_map)
{
  rcutils_ret_t ret = RCUTILS_RET_OK;
  if (hash_map->impl->size >= (size_t)(LOAD_FACTOR * (double)hash_map->impl->capacity)) {
    size_t new_capacity = 2 * hash_map->impl->capacity;
    rcutils_array_list_t * new_map = NULL;

    ret = hash_map_allocate_new_map(&new_map, new_capacity, &hash_map->impl->allocator);
    if (RCUTILS_RET_OK != ret) {
      return ret;
    }

    for (size_t map_index = 0;
      map_index < hash_map->impl->capacity && RCUTILS_RET_OK == ret;
      ++map_index)
    {
      rcutils_array_list_t * bucket = &(hash_map->impl->map[map_index]);
      // Is this a valid bucket with entries
      if (NULL != bucket->impl) {
        size_t bucket_size = 0;
        ret = rcutils_array_list_get_size(bucket, &bucket_size);
        if (RCUTILS_RET_OK != ret) {
          return ret;
        }

        for (size_t bucket_index = 0;
          bucket_index < bucket_size && RCUTILS_RET_OK == ret;
          ++bucket_index)
        {
          rcutils_hash_map_entry_t * entry = NULL;
          ret = rcutils_array_list_get(bucket, bucket_index, &entry);
          if (RCUTILS_RET_OK == ret) {
            size_t new_index = entry->hashed_key % new_capacity;
            ret = hash_map_insert_entry(new_map, new_index, entry, &hash_map->impl->allocator);
          }
        }
      }
    }

    // Something went wrong above after we allocated the new map. Try to clean it up
    if (RCUTILS_RET_OK != ret) {
      hash_map_deallocate_map(new_map, new_capacity, &hash_map->impl->allocator, false);
      return ret;
    }

    // Cleanup the old map and swap in the new one
    ret = hash_map_deallocate_map(
      hash_map->impl->map, hash_map->impl->capacity, &hash_map->impl->allocator, false);
    // everything worked up to this point, so if we fail to dealloc the old map still set the new
    hash_map->impl->map = new_map;
    hash_map->impl->capacity = new_capacity;
  }

  return ret;
}

rcutils_ret_t
rcutils_hash_map_init(
  rcutils_hash_map_t * hash_map,
  size_t initial_capacity,
  size_t key_size,
  size_t data_size,
  rcutils_hash_map_key_hasher_t key_hashing_func,
  rcutils_hash_map_key_cmp_t key_cmp_func,
  const rcutils_allocator_t * allocator)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(hash_map, RCUTILS_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(key_hashing_func, RCUTILS_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(key_cmp_func, RCUTILS_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ALLOCATOR(allocator, return RCUTILS_RET_INVALID_ARGUMENT);
  if (1 > initial_capacity) {
    RCUTILS_SET_ERROR_MSG("initial_capacity cannot be less than 1");
    return RCUTILS_RET_INVALID_ARGUMENT;
  } else if (1 > key_size) {
    RCUTILS_SET_ERROR_MSG("key_size cannot be less than 1");
    return RCUTILS_RET_INVALID_ARGUMENT;
  } else if (1 > data_size) {
    RCUTILS_SET_ERROR_MSG("data_size cannot be less than 1");
    return RCUTILS_RET_INVALID_ARGUMENT;
  }

  hash_map->impl = allocator->allocate(sizeof(rcutils_hash_map_impl_t), allocator->state);
  if (NULL == hash_map->impl) {
    RCUTILS_SET_ERROR_MSG("failed to allocate memory for hash map impl");
    return RCUTILS_RET_BAD_ALLOC;
  }

  hash_map->impl->capacity = initial_capacity;
  hash_map->impl->size = 0;
  hash_map->impl->key_size = key_size;
  hash_map->impl->data_size = data_size;
  hash_map->impl->key_hashing_func = key_hashing_func;
  hash_map->impl->key_cmp_func = key_cmp_func;

  rcutils_ret_t ret = hash_map_allocate_new_map(&hash_map->impl->map, initial_capacity, allocator);
  if (RCUTILS_RET_OK != ret) {
    // Cleanup allocated memory before we return failure
    allocator->deallocate(hash_map->impl, allocator->state);
    hash_map->impl = NULL;
    RCUTILS_SET_ERROR_MSG("failed to allocate memory for map data");
    return ret;
  }

  hash_map->impl->allocator = *allocator;

  return RCUTILS_RET_OK;
}

rcutils_ret_t
rcutils_hash_map_fini(rcutils_hash_map_t * hash_map)
{
  HASH_MAP_VALIDATE_HASH_MAP(hash_map);
  rcutils_ret_t ret = hash_map_deallocate_map(
    hash_map->impl->map, hash_map->impl->capacity, &hash_map->impl->allocator, true);

  if (RCUTILS_RET_OK == ret) {
    hash_map->impl->allocator.deallocate(hash_map->impl, hash_map->impl->allocator.state);
    hash_map->impl = NULL;
  }

  return ret;
}

rcutils_ret_t
rcutils_hash_map_get_capacity(const rcutils_hash_map_t * hash_map, size_t * capacity)
{
  HASH_MAP_VALIDATE_HASH_MAP(hash_map);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(capacity, RCUTILS_RET_INVALID_ARGUMENT);
  *capacity = hash_map->impl->capacity;
  return RCUTILS_RET_OK;
}

rcutils_ret_t
rcutils_hash_map_get_size(const rcutils_hash_map_t * hash_map, size_t * size)
{
  HASH_MAP_VALIDATE_HASH_MAP(hash_map);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(size, RCUTILS_RET_INVALID_ARGUMENT);
  *size = hash_map->impl->size;
  return RCUTILS_RET_OK;
}

/// Returns true if found or false if it doesn't exist.
/// key_hash and map_index will always be set correctly
static bool hash_map_find(
  const rcutils_hash_map_t * hash_map,   // [in] The hash_map to look up in
  const void * key,   // [in] The key to lookup
  size_t * key_hash,   // [out] The key's hashed value
  size_t * map_index,   // [out] The index into the array of buckets
  size_t * bucket_index,   // [out] The index of the entry in its bucket
  rcutils_hash_map_entry_t ** entry)   // [out] Will be set to a pointer to the entry's data
{
  size_t bucket_size = 0;
  rcutils_hash_map_entry_t * bucket_entry = NULL;

  *key_hash = hash_map->impl->key_hashing_func(key);
  *map_index = (*key_hash) % hash_map->impl->capacity;

  // Find the bucket the entry should be in check that it's valid
  rcutils_array_list_t * bucket = &(hash_map->impl->map[*map_index]);
  if (NULL == bucket->impl) {
    return false;
  }
  if (RCUTILS_RET_OK != rcutils_array_list_get_size(bucket, &bucket_size)) {
    return false;
  }
  for (size_t i = 0; i < bucket_size; ++i) {
    if (RCUTILS_RET_OK != rcutils_array_list_get(bucket, i, &bucket_entry)) {
      return false;
    }
    // Check that the hashes match first as that will be the quicker comparison to quick fail on
    if (bucket_entry->hashed_key == *key_hash &&
      (0 == hash_map->impl->key_cmp_func(bucket_entry->key, key)))
    {
      *bucket_index = i;
      *entry = bucket_entry;
      return true;
    }
  }

  return false;
}

rcutils_ret_t
rcutils_hash_map_set(rcutils_hash_map_t * hash_map, const void * key, const void * value)
{
  HASH_MAP_VALIDATE_HASH_MAP(hash_map);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(key, RCUTILS_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(value, RCUTILS_RET_INVALID_ARGUMENT);

  size_t key_hash = 0, map_index = 0, bucket_index = 0;
  bool already_exists = false;
  rcutils_hash_map_entry_t * entry = NULL;
  rcutils_ret_t ret = RCUTILS_RET_OK;

  already_exists = hash_map_find(hash_map, key, &key_hash, &map_index, &bucket_index, &entry);

  if (already_exists) {
    // Just update the existing value to match the new value
    memcpy(entry->value, value, hash_map->impl->data_size);
  } else {
    // We need to create a new entry in the map
    rcutils_allocator_t * allocator = &hash_map->impl->allocator;

    // Start by trying to allocate the memory we need for the new entry
    entry = allocator->allocate(sizeof(rcutils_hash_map_entry_t), allocator->state);
    if (NULL == entry) {
      return RCUTILS_RET_BAD_ALLOC;
    }
    entry->key = allocator->allocate(hash_map->impl->key_size, allocator->state);
    entry->value = allocator->allocate(hash_map->impl->data_size, allocator->state);
    if (NULL == entry->key || NULL == entry->value) {
      ret = RCUTILS_RET_BAD_ALLOC;
    }

    // Set the entry data and try to insert into the bucket
    if (RCUTILS_RET_OK == ret) {
      entry->hashed_key = key_hash;
      memcpy(entry->value, value, hash_map->impl->data_size);
      memcpy(entry->key, key, hash_map->impl->key_size);

      bucket_index = key_hash % hash_map->impl->capacity;
      ret = hash_map_insert_entry(hash_map->impl->map, bucket_index, entry, allocator);
    }

    if (RCUTILS_RET_OK != ret) {
      // If something went wrong somewhere then cleanup the memory we've allocated
      hash_map_deallocate_entry(allocator, entry);
      return ret;
    }
    hash_map->impl->size++;
  }

  // Time to check if we've exceeded our Load Factor and grow the map if so
  ret = hash_map_check_and_grow_map(hash_map);
  // Just log on this failure because the map can continue to operate with degraded performance
  RCUTILS_LOG_ERROR_EXPRESSION(RCUTILS_RET_OK != ret, "Failed to grow hash_map. Reason: %d", ret);

  return RCUTILS_RET_OK;
}

rcutils_ret_t
rcutils_hash_map_unset(rcutils_hash_map_t * hash_map, const void * key)
{
  HASH_MAP_VALIDATE_HASH_MAP(hash_map);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(key, RCUTILS_RET_INVALID_ARGUMENT);

  size_t key_hash = 0, map_index = 0, bucket_index = 0;
  bool already_exists = false;
  rcutils_hash_map_entry_t * entry = NULL;

  already_exists = hash_map_find(hash_map, key, &key_hash, &map_index, &bucket_index, &entry);

  if (!already_exists) {
    // The entry isn't in the map, so just exit
    return RCUTILS_RET_OK;
  }

  // Remove the entry from its bucket and deallocate it
  rcutils_array_list_t * bucket = &(hash_map->impl->map[map_index]);
  if (RCUTILS_RET_OK == rcutils_array_list_remove(bucket, bucket_index)) {
    hash_map->impl->size--;
    hash_map_deallocate_entry(&hash_map->impl->allocator, entry);
  }

  return RCUTILS_RET_OK;
}

bool
rcutils_hash_map_key_exists(const rcutils_hash_map_t * hash_map, const void * key)
{
  // Verify input
  if (NULL == hash_map) {
    return false;
  } else if (NULL == hash_map->impl) {
    return false;
  } else if (NULL == key) {
    return false;
  }

  size_t key_hash = 0, map_index = 0, bucket_index = 0;
  bool already_exists = false;
  rcutils_hash_map_entry_t * entry = NULL;

  already_exists = hash_map_find(hash_map, key, &key_hash, &map_index, &bucket_index, &entry);

  return already_exists;
}

rcutils_ret_t
rcutils_hash_map_get(const rcutils_hash_map_t * hash_map, const void * key, void * data)
{
  HASH_MAP_VALIDATE_HASH_MAP(hash_map);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(key, RCUTILS_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(data, RCUTILS_RET_INVALID_ARGUMENT);

  size_t key_hash = 0, map_index = 0, bucket_index = 0;
  bool already_exists = false;
  rcutils_hash_map_entry_t * entry = NULL;

  already_exists = hash_map_find(hash_map, key, &key_hash, &map_index, &bucket_index, &entry);

  if (already_exists) {
    memcpy(data, entry->value, hash_map->impl->data_size);
    return RCUTILS_RET_OK;
  }

  return RCUTILS_RET_NOT_FOUND;
}

rcutils_ret_t
rcutils_hash_map_get_next_key_and_data(
  const rcutils_hash_map_t * hash_map,
  const void * previous_key,
  void * key,
  void * data)
{
  HASH_MAP_VALIDATE_HASH_MAP(hash_map);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(key, RCUTILS_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(data, RCUTILS_RET_INVALID_ARGUMENT);

  size_t key_hash = 0, map_index = 0, bucket_index = 0;
  bool already_exists = false;
  rcutils_hash_map_entry_t * entry = NULL;
  rcutils_ret_t ret = RCUTILS_RET_OK;

  if (NULL != previous_key) {
    already_exists = hash_map_find(hash_map, key, &key_hash, &map_index, &bucket_index, &entry);
    if (!already_exists) {
      return RCUTILS_RET_NOT_FOUND;
    }
    bucket_index++;  // We want to start our search from the next object
  }

  for (; map_index < hash_map->impl->capacity; ++map_index) {
    rcutils_array_list_t * bucket = &(hash_map->impl->map[map_index]);
    if (NULL != bucket->impl) {
      size_t bucket_size = 0;
      ret = rcutils_array_list_get_size(bucket, &bucket_size);
      if (RCUTILS_RET_OK != ret) {
        return ret;
      }

      // Check if the next index in this bucket is valid and if so we've found the next item
      if (bucket_index < bucket_size) {
        rcutils_hash_map_entry_t * bucket_entry = NULL;
        ret = rcutils_array_list_get(bucket, bucket_index, &bucket_entry);
        if (RCUTILS_RET_OK == ret) {
          memcpy(key, bucket_entry->key, hash_map->impl->key_size);
          memcpy(data, bucket_entry->value, hash_map->impl->data_size);
        }

        return ret;
      }
    }
    // After the first bucket the next entry must be at the start of the next bucket with entries
    bucket_index = 0;
  }

  return RCUTILS_RET_HASH_MAP_NO_MORE_ENTRIES;
}


#ifdef __cplusplus
}
#endif
