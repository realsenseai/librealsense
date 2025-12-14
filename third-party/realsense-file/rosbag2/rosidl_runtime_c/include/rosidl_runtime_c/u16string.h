// Copyright 2015-2018 Open Source Robotics Foundation, Inc.
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

#ifndef ROSIDL_RUNTIME_C__U16STRING_H_
#define ROSIDL_RUNTIME_C__U16STRING_H_

#include <stddef.h>

#include "rosidl_runtime_c/primitives_sequence.h"

/// An array of 16-bit characters terminated by a null character.
typedef struct rosidl_runtime_c__U16String
{
  /// The pointer to the first character, the sequence ends with a null character.
  uint_least16_t * data;  // using uint_least16_t to match a C++ std::u16string
  /// The length of the u16string (excluding the null character).
  size_t size;
  /// The capacity represents the number of allocated characters (including the null character).
  size_t capacity;
} rosidl_runtime_c__U16String;

ROSIDL_RUNTIME_C__PRIMITIVE_SEQUENCE(U16String, rosidl_runtime_c__U16String)

#endif  // ROSIDL_RUNTIME_C__U16STRING_H_
