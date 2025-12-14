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

#ifndef ROSIDL_RUNTIME_C__PRIMITIVES_SEQUENCE_FUNCTIONS_H_
#define ROSIDL_RUNTIME_C__PRIMITIVES_SEQUENCE_FUNCTIONS_H_

#include <stdbool.h>
#include <stddef.h>

#include "rosidl_runtime_c/primitives_sequence.h"
#include "rosidl_runtime_c/visibility_control.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * \def ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_INIT(STRUCT_NAME)
 *
 * \brief Allocate the memory for the sequence.
 *
 * Calling the function with an already allocated sequence will leak the
 * previously allocated memory.
 *
 * param sequence a pointer to a sequence struct
 * param size the number of items to allocate in the sequence, both sequence
 *   fields `size` and `capacity` are set to this parameter
 * return true if successful, false if the passed sequence pointer is null
 *   or the memory allocation failed
 */
#define ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_INIT(STRUCT_NAME) \
  /** See #ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_INIT(STRUCT_NAME) */ \
  ROSIDL_GENERATOR_C_PUBLIC \
  bool rosidl_runtime_c__ ## STRUCT_NAME ## __Sequence__init( \
    rosidl_runtime_c__ ## STRUCT_NAME ## __Sequence * sequence, size_t size);

/**
 * \def ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_FINI(STRUCT_NAME)
 *
 * \brief Deallocate the memory of the sequence.
 *
 * Calling the function with an already deallocated sequence is a no-op.
 *
 * param sequence a pointer to a sequence struct
 */
#define ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_FINI(STRUCT_NAME) \
  /** See #ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_FINI(STRUCT_NAME) */ \
  ROSIDL_GENERATOR_C_PUBLIC \
  void rosidl_runtime_c__ ## STRUCT_NAME ## __Sequence__fini( \
    rosidl_runtime_c__ ## STRUCT_NAME ## __Sequence * sequence);

/**
 * \def ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_EQ(STRUCT_NAME)
 *
 * \brief Check for sequence equality.
 *
 * param lhs a pointer to a sequence struct
 * param rhs a pointer to another sequence struct
 */
#define ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_EQ(STRUCT_NAME) \
  /** See #ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_EQ(STRUCT_NAME) */ \
  ROSIDL_GENERATOR_C_PUBLIC \
  bool rosidl_runtime_c__ ## STRUCT_NAME ## __Sequence__are_equal( \
    const rosidl_runtime_c__ ## STRUCT_NAME ## __Sequence * lhs, \
    const rosidl_runtime_c__ ## STRUCT_NAME ## __Sequence * rhs);

/**
 * \def ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_COPY(STRUCT_NAME)
 *
 * \brief Copy the sequence.
 *
 * param input a pointer to the sequence to copy from
 * param output a pointer to an initialized sequence to copy to
 * return true if successful, false if either pointer is null or memory
 *   allocation fails.
 */
#define ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_COPY(STRUCT_NAME) \
  /** See #ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_COPY(STRUCT_NAME) */ \
  ROSIDL_GENERATOR_C_PUBLIC \
  bool rosidl_runtime_c__ ## STRUCT_NAME ## __Sequence__copy( \
    const rosidl_runtime_c__ ## STRUCT_NAME ## __Sequence * input, \
    rosidl_runtime_c__ ## STRUCT_NAME ## __Sequence * output);

/**
 * \def ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_FUNCTIONS(STRUCT_NAME)
 *
 * \brief See #ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_INIT(STRUCT_NAME),
 * #ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_FINI(STRUCT_NAME),
 * #ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_EQ(STRUCT_NAME), and
 * #ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_COPY(STRUCT_NAME).
 */
#define ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_FUNCTIONS(STRUCT_NAME) \
  ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_INIT(STRUCT_NAME) \
  ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_FINI(STRUCT_NAME) \
  ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_EQ(STRUCT_NAME) \
  ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_COPY(STRUCT_NAME)

/**
 * \defgroup primitives_sequence_functions__basic_types Sequence functions for all basic types.
 */
/**@{*/
ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_FUNCTIONS(float)
ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_FUNCTIONS(double)
ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_FUNCTIONS(long_double)
ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_FUNCTIONS(char)
ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_FUNCTIONS(wchar)
ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_FUNCTIONS(boolean)
ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_FUNCTIONS(octet)
ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_FUNCTIONS(uint8)
ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_FUNCTIONS(int8)
ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_FUNCTIONS(uint16)
ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_FUNCTIONS(int16)
ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_FUNCTIONS(uint32)
ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_FUNCTIONS(int32)
ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_FUNCTIONS(uint64)
ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_FUNCTIONS(int64)
/**@}*/

/**
 * \defgroup primitives_sequence_functions__legacy Sequence functions for legacy types for backward compatibility.
 */
/**@{*/
/** See #ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_INIT(bool) */
ROSIDL_GENERATOR_C_PUBLIC
bool rosidl_runtime_c__bool__Sequence__init(
  rosidl_runtime_c__boolean__Sequence * sequence, size_t size);
/** See #ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_FINI(bool) */
ROSIDL_GENERATOR_C_PUBLIC
void rosidl_runtime_c__bool__Sequence__fini(
  rosidl_runtime_c__boolean__Sequence * sequence);
/** See #ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_EQ(bool) */
ROSIDL_GENERATOR_C_PUBLIC
bool rosidl_runtime_c__bool__Sequence__are_equal(
  const rosidl_runtime_c__boolean__Sequence * lhs,
  const rosidl_runtime_c__boolean__Sequence * rhs);
/** See #ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_COPY(bool) */
ROSIDL_GENERATOR_C_PUBLIC
bool rosidl_runtime_c__bool__Sequence__copy(
  const rosidl_runtime_c__boolean__Sequence * input,
  rosidl_runtime_c__boolean__Sequence * output);

/** See #ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_INIT(byte) */
ROSIDL_GENERATOR_C_PUBLIC
bool rosidl_runtime_c__byte__Sequence__init(
  rosidl_runtime_c__octet__Sequence * sequence, size_t size);
/** See #ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_FINI(byte) */
ROSIDL_GENERATOR_C_PUBLIC
void rosidl_runtime_c__byte__Sequence__fini(
  rosidl_runtime_c__octet__Sequence * sequence);
/** See #ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_EQ(byte) */
ROSIDL_GENERATOR_C_PUBLIC
bool rosidl_runtime_c__byte__Sequence__are_equal(
  const rosidl_runtime_c__octet__Sequence * lhs,
  const rosidl_runtime_c__octet__Sequence * rhs);
/** See #ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_COPY(byte) */
ROSIDL_GENERATOR_C_PUBLIC
bool rosidl_runtime_c__byte__Sequence__copy(
  const rosidl_runtime_c__octet__Sequence * input,
  rosidl_runtime_c__octet__Sequence * output);

/** See #ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_INIT(float32) */
ROSIDL_GENERATOR_C_PUBLIC
bool rosidl_runtime_c__float32__Sequence__init(
  rosidl_runtime_c__float__Sequence * sequence, size_t size);
/** See #ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_FINI(float32) */
ROSIDL_GENERATOR_C_PUBLIC
void rosidl_runtime_c__float32__Sequence__fini(
  rosidl_runtime_c__float__Sequence * sequence);
/** See #ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_EQ(float32) */
ROSIDL_GENERATOR_C_PUBLIC
bool rosidl_runtime_c__float32__Sequence__are_equal(
  const rosidl_runtime_c__float__Sequence * lhs,
  const rosidl_runtime_c__float__Sequence * rhs);
/** See #ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_COPY(float32) */
ROSIDL_GENERATOR_C_PUBLIC
bool rosidl_runtime_c__float32__Sequence__copy(
  const rosidl_runtime_c__float__Sequence * input,
  rosidl_runtime_c__float__Sequence * output);

/** See #ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_INIT(float64) */
ROSIDL_GENERATOR_C_PUBLIC
bool rosidl_runtime_c__float64__Sequence__init(
  rosidl_runtime_c__double__Sequence * sequence, size_t size);
/** See #ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_FINI(float64) */
ROSIDL_GENERATOR_C_PUBLIC
void rosidl_runtime_c__float64__Sequence__fini(
  rosidl_runtime_c__double__Sequence * sequence);
/** See #ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_EQ(float64) */
ROSIDL_GENERATOR_C_PUBLIC
bool rosidl_runtime_c__float64__Sequence__are_equal(
  const rosidl_runtime_c__double__Sequence * lhs,
  const rosidl_runtime_c__double__Sequence * rhs);
/** See #ROSIDL_RUNTIME_C__DECLARE_PRIMITIVE_SEQUENCE_COPY(float64) */
ROSIDL_GENERATOR_C_PUBLIC
bool rosidl_runtime_c__float64__Sequence__copy(
  const rosidl_runtime_c__double__Sequence * input,
  rosidl_runtime_c__double__Sequence * output);
/**@}*/

#ifdef __cplusplus
}
#endif

#endif  // ROSIDL_RUNTIME_C__PRIMITIVES_SEQUENCE_FUNCTIONS_H_
