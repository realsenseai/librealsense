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

#include "rosidl_runtime_c/string_functions.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <rcutils/allocator.h>
#include "rcutils/macros.h"

bool
rosidl_runtime_c__String__init(rosidl_runtime_c__String * str)
{
  RCUTILS_CAN_RETURN_WITH_ERROR_OF(false);

  if (!str) {
    return false;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  str->data = allocator.allocate(1, allocator.state);
  if (!str->data) {
    return false;
  }
  str->data[0] = '\0';
  str->size = 0;
  str->capacity = 1;
  return true;
}

void
rosidl_runtime_c__String__fini(rosidl_runtime_c__String * str)
{
  if (!str) {
    return;
  }
  if (str->data) {
    /* ensure that data and capacity values are consistent */
    if (str->capacity <= 0) {
      fprintf(
        stderr, "Unexpected condition: string capacity was zero for allocated data! "
        "Exiting.\n");
      exit(-1);
    }
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    allocator.deallocate(str->data, allocator.state);
    str->data = NULL;
    str->size = 0;
    str->capacity = 0;
  } else {
    /* ensure that data, size, and capacity values are consistent */
    if (0 != str->size) {
      fprintf(
        stderr, "Unexpected condition: string size was non-zero for deallocated data! "
        "Exiting.\n");
      exit(-1);
    }
    if (0 != str->capacity) {
      fprintf(
        stderr, "Unexpected behavior: string capacity was non-zero for deallocated data! "
        "Exiting.\n");
      exit(-1);
    }
  }
}

bool
rosidl_runtime_c__String__are_equal(
  const rosidl_runtime_c__String * lhs,
  const rosidl_runtime_c__String * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  if (lhs->size != rhs->size) {
    return false;
  }
  return memcmp(lhs->data, rhs->data, lhs->size) == 0;
}

bool
rosidl_runtime_c__String__assignn(
  rosidl_runtime_c__String * str, const char * value, size_t n)
{
  if (!str) {
    return false;
  }
  // a NULL value is not valid
  if (!value) {
    return false;
  }
  // since n + 1 bytes are being allocated n can't be the maximum value
  if (n == SIZE_MAX) {
    return false;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  char * data = allocator.reallocate(str->data, n + 1, allocator.state);
  if (!data) {
    return false;
  }
  memcpy(data, value, n);
  data[n] = '\0';
  str->data = data;
  str->size = n;
  str->capacity = n + 1;
  return true;
}

bool
rosidl_runtime_c__String__assign(
  rosidl_runtime_c__String * str, const char * value)
{
  if (!value) {
    // strlen is not defined for nullptr, let assignn take care of other bad values
    return false;
  }
  return rosidl_runtime_c__String__assignn(
    str, value, strlen(value));
}

bool
rosidl_runtime_c__String__copy(
  const rosidl_runtime_c__String * input,
  rosidl_runtime_c__String * output)
{
  if (!input) {
    return false;
  }
  return rosidl_runtime_c__String__assignn(
    output, input->data, input->size);
}

bool
rosidl_runtime_c__String__Sequence__init(
  rosidl_runtime_c__String__Sequence * sequence, size_t size)
{
  RCUTILS_CAN_RETURN_WITH_ERROR_OF(false);

  if (!sequence) {
    return false;
  }
  rosidl_runtime_c__String * data = NULL;
  if (size) {
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    data = (rosidl_runtime_c__String *)allocator.zero_allocate(
      size, sizeof(rosidl_runtime_c__String), allocator.state);
    if (!data) {
      return false;
    }
    // initialize all sequence elements
    for (size_t i = 0; i < size; ++i) {
      if (!rosidl_runtime_c__String__init(&data[i])) {
        /* free currently allocated and return false */
        for (; i-- > 0; ) {
          rosidl_runtime_c__String__fini(&data[i]);
        }
        allocator.deallocate(data, allocator.state);
        return false;
      }
    }
  }
  sequence->data = data;
  sequence->size = size;
  sequence->capacity = size;
  return true;
}

void
rosidl_runtime_c__String__Sequence__fini(
  rosidl_runtime_c__String__Sequence * sequence)
{
  if (!sequence) {
    return;
  }
  if (sequence->data) {
    // ensure that data and capacity values are consistent
    assert(sequence->capacity > 0);
    // finalize all sequence elements
    for (size_t i = 0; i < sequence->capacity; ++i) {
      rosidl_runtime_c__String__fini(&sequence->data[i]);
    }
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    allocator.deallocate(sequence->data, allocator.state);
    sequence->data = NULL;
    sequence->size = 0;
    sequence->capacity = 0;
  } else {
    // ensure that data, size, and capacity values are consistent
    assert(0 == sequence->size);
    assert(0 == sequence->capacity);
  }
}

bool
rosidl_runtime_c__String__Sequence__are_equal(
  const rosidl_runtime_c__String__Sequence * lhs,
  const rosidl_runtime_c__String__Sequence * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  if (lhs->size != rhs->size) {
    return false;
  }
  for (size_t i = 0; i < lhs->size; ++i) {
    if (!rosidl_runtime_c__String__are_equal(
        &(lhs->data[i]), &(rhs->data[i])))
    {
      return false;
    }
  }
  return true;
}

bool
rosidl_runtime_c__String__Sequence__copy(
  const rosidl_runtime_c__String__Sequence * input,
  rosidl_runtime_c__String__Sequence * output)
{
  if (!input || !output) {
    return false;
  }
  if (output->capacity < input->size) {
    const size_t allocation_size =
      input->size * sizeof(rosidl_runtime_c__String);
    rosidl_runtime_c__String * data =
      (rosidl_runtime_c__String *)realloc(output->data, allocation_size);
    if (!data) {
      return false;
    }
    for (size_t i = output->capacity; i < input->size; ++i) {
      if (!rosidl_runtime_c__String__init(&data[i])) {
        /* free currently allocated and return false */
        for (; i-- > output->capacity; ) {
          rosidl_runtime_c__String__fini(&data[i]);
        }
        free(data);
        return false;
      }
    }
    output->data = data;
    output->capacity = input->size;
  }
  output->size = input->size;
  for (size_t i = 0; i < input->size; ++i) {
    if (!rosidl_runtime_c__String__copy(
        &(input->data[i]), &(output->data[i])))
    {
      return false;
    }
  }
  return true;
}

rosidl_runtime_c__String__Sequence *
rosidl_runtime_c__String__Sequence__create(size_t size)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  rosidl_runtime_c__String__Sequence * sequence =
    (rosidl_runtime_c__String__Sequence *)allocator.allocate(
    sizeof(rosidl_runtime_c__String__Sequence), allocator.state);
  if (!sequence) {
    return NULL;
  }
  bool success = rosidl_runtime_c__String__Sequence__init(sequence, size);
  if (!success) {
    allocator.deallocate(sequence, allocator.state);
    return NULL;
  }
  return sequence;
}

void
rosidl_runtime_c__String__Sequence__destroy(
  rosidl_runtime_c__String__Sequence * sequence)
{
  if (sequence) {
    rosidl_runtime_c__String__Sequence__fini(sequence);
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  allocator.deallocate(sequence, allocator.state);
}
