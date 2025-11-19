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

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rcutils/allocator.h>

#include "rosidl_runtime_c/primitives_sequence_functions.h"

#define ROSIDL_GENERATOR_C__DEFINE_PRIMITIVE_SEQUENCE_FUNCTIONS(STRUCT_NAME, TYPE_NAME) \
  bool rosidl_runtime_c__ ## STRUCT_NAME ## __Sequence__init( \
    rosidl_runtime_c__ ## STRUCT_NAME ## __Sequence * sequence, size_t size) \
  { \
    if (!sequence) { \
      return false; \
    } \
    TYPE_NAME * data = NULL; \
    if (size) { \
      rcutils_allocator_t allocator = rcutils_get_default_allocator(); \
      data = allocator.allocate(sizeof(TYPE_NAME) * size, allocator.state); \
      if (!data) { \
        return false; \
      } \
    } \
    sequence->data = data; \
    sequence->size = size; \
    sequence->capacity = size; \
    return true; \
  } \
 \
  void rosidl_runtime_c__ ## STRUCT_NAME ## __Sequence__fini( \
    rosidl_runtime_c__ ## STRUCT_NAME ## __Sequence * sequence) \
  { \
    if (!sequence) { \
      return; \
    } \
    if (sequence->data) { \
      /* ensure that data and capacity values are consistent */ \
      assert(sequence->capacity > 0); \
      rcutils_allocator_t allocator = rcutils_get_default_allocator(); \
      allocator.deallocate(sequence->data, allocator.state); \
      sequence->data = NULL; \
      sequence->size = 0; \
      sequence->capacity = 0; \
    } else { \
      /* ensure that data, size, and capacity values are consistent */ \
      assert(0 == sequence->size); \
      assert(0 == sequence->capacity); \
    } \
  } \
 \
  bool rosidl_runtime_c__ ## STRUCT_NAME ## __Sequence__are_equal( \
    const rosidl_runtime_c__ ## STRUCT_NAME ## __Sequence * lhs, \
    const rosidl_runtime_c__ ## STRUCT_NAME ## __Sequence * rhs) \
  { \
    if (!lhs || !rhs) { \
      return false; \
    } \
    if (lhs->size != rhs->size) { \
      return false; \
    } \
    for (size_t i = 0; i < lhs->size; ++i) { \
      if (lhs->data[i] != rhs->data[i]) { \
        return false; \
      } \
    } \
    return true; \
  } \
 \
  bool rosidl_runtime_c__ ## STRUCT_NAME ## __Sequence__copy( \
    const rosidl_runtime_c__ ## STRUCT_NAME ## __Sequence * input, \
    rosidl_runtime_c__ ## STRUCT_NAME ## __Sequence * output) \
  { \
    if (!input || !output) { \
      return false; \
    } \
    if (output->capacity < input->size) { \
      TYPE_NAME * data = (TYPE_NAME *)realloc( \
        output->data, sizeof(TYPE_NAME) * input->size); \
      if (!data) { \
        return false; \
      } \
      output->data = data; \
      output->capacity = input->size; \
    } \
    memcpy(output->data, input->data, sizeof(TYPE_NAME) * input->size); \
    output->size = input->size; \
    return true; \
  }


// array functions for all basic types
ROSIDL_GENERATOR_C__DEFINE_PRIMITIVE_SEQUENCE_FUNCTIONS(float, float)
ROSIDL_GENERATOR_C__DEFINE_PRIMITIVE_SEQUENCE_FUNCTIONS(double, double)
ROSIDL_GENERATOR_C__DEFINE_PRIMITIVE_SEQUENCE_FUNCTIONS(long_double, long double)
ROSIDL_GENERATOR_C__DEFINE_PRIMITIVE_SEQUENCE_FUNCTIONS(char, signed char)
ROSIDL_GENERATOR_C__DEFINE_PRIMITIVE_SEQUENCE_FUNCTIONS(wchar, uint16_t)
ROSIDL_GENERATOR_C__DEFINE_PRIMITIVE_SEQUENCE_FUNCTIONS(boolean, bool)
ROSIDL_GENERATOR_C__DEFINE_PRIMITIVE_SEQUENCE_FUNCTIONS(octet, uint8_t)
ROSIDL_GENERATOR_C__DEFINE_PRIMITIVE_SEQUENCE_FUNCTIONS(uint8, uint8_t)
ROSIDL_GENERATOR_C__DEFINE_PRIMITIVE_SEQUENCE_FUNCTIONS(int8, int8_t)
ROSIDL_GENERATOR_C__DEFINE_PRIMITIVE_SEQUENCE_FUNCTIONS(uint16, uint16_t)
ROSIDL_GENERATOR_C__DEFINE_PRIMITIVE_SEQUENCE_FUNCTIONS(int16, int16_t)
ROSIDL_GENERATOR_C__DEFINE_PRIMITIVE_SEQUENCE_FUNCTIONS(uint32, uint32_t)
ROSIDL_GENERATOR_C__DEFINE_PRIMITIVE_SEQUENCE_FUNCTIONS(int32, int32_t)
ROSIDL_GENERATOR_C__DEFINE_PRIMITIVE_SEQUENCE_FUNCTIONS(uint64, uint64_t)
ROSIDL_GENERATOR_C__DEFINE_PRIMITIVE_SEQUENCE_FUNCTIONS(int64, int64_t)

// emulate legacy API
bool rosidl_runtime_c__bool__Sequence__init(
  rosidl_runtime_c__boolean__Sequence * sequence, size_t size)
{
  return rosidl_runtime_c__boolean__Sequence__init(
    sequence, size);
}
void rosidl_runtime_c__bool__Sequence__fini(
  rosidl_runtime_c__boolean__Sequence * sequence)
{
  rosidl_runtime_c__boolean__Sequence__fini(
    sequence);
}
bool rosidl_runtime_c__bool__Sequence__are_equal(
  const rosidl_runtime_c__boolean__Sequence * lhs,
  const rosidl_runtime_c__boolean__Sequence * rhs)
{
  return rosidl_runtime_c__boolean__Sequence__are_equal(lhs, rhs);
}
bool rosidl_runtime_c__bool__Sequence__copy(
  const rosidl_runtime_c__boolean__Sequence * input,
  rosidl_runtime_c__boolean__Sequence * output)
{
  return rosidl_runtime_c__boolean__Sequence__copy(input, output);
}

bool rosidl_runtime_c__byte__Sequence__init(
  rosidl_runtime_c__octet__Sequence * sequence, size_t size)
{
  return rosidl_runtime_c__octet__Sequence__init(
    sequence, size);
}
void rosidl_runtime_c__byte__Sequence__fini(
  rosidl_runtime_c__octet__Sequence * sequence)
{
  rosidl_runtime_c__octet__Sequence__fini(
    sequence);
}
bool rosidl_runtime_c__byte__Sequence__are_equal(
  const rosidl_runtime_c__octet__Sequence * lhs,
  const rosidl_runtime_c__octet__Sequence * rhs)
{
  return rosidl_runtime_c__octet__Sequence__are_equal(lhs, rhs);
}
bool rosidl_runtime_c__byte__Sequence__copy(
  const rosidl_runtime_c__octet__Sequence * input,
  rosidl_runtime_c__octet__Sequence * output)
{
  return rosidl_runtime_c__octet__Sequence__copy(input, output);
}

bool rosidl_runtime_c__float32__Sequence__init(
  rosidl_runtime_c__float__Sequence * sequence, size_t size)
{
  return rosidl_runtime_c__float__Sequence__init(
    sequence, size);
}
void rosidl_runtime_c__float32__Sequence__fini(
  rosidl_runtime_c__float__Sequence * sequence)
{
  rosidl_runtime_c__float__Sequence__fini(
    sequence);
}
bool rosidl_runtime_c__float32__Sequence__are_equal(
  const rosidl_runtime_c__float__Sequence * lhs,
  const rosidl_runtime_c__float__Sequence * rhs)
{
  return rosidl_runtime_c__float__Sequence__are_equal(lhs, rhs);
}
bool rosidl_runtime_c__float32__Sequence__copy(
  const rosidl_runtime_c__float__Sequence * input,
  rosidl_runtime_c__float__Sequence * output)
{
  return rosidl_runtime_c__float__Sequence__copy(input, output);
}

bool rosidl_runtime_c__float64__Sequence__init(
  rosidl_runtime_c__double__Sequence * sequence, size_t size)
{
  return rosidl_runtime_c__double__Sequence__init(
    sequence, size);
}
void rosidl_runtime_c__float64__Sequence__fini(
  rosidl_runtime_c__double__Sequence * sequence)
{
  rosidl_runtime_c__double__Sequence__fini(
    sequence);
}
bool rosidl_runtime_c__float64__Sequence__are_equal(
  const rosidl_runtime_c__double__Sequence * lhs,
  const rosidl_runtime_c__double__Sequence * rhs)
{
  return rosidl_runtime_c__double__Sequence__are_equal(lhs, rhs);
}
bool rosidl_runtime_c__float64__Sequence__copy(
  const rosidl_runtime_c__double__Sequence * input,
  rosidl_runtime_c__double__Sequence * output)
{
  return rosidl_runtime_c__double__Sequence__copy(input, output);
}
