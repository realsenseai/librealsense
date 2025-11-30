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

// Note: migrated from rmw/error_handling.c in 2017-04

#ifdef __cplusplus
extern "C"
{
#endif

#include <rcutils/error_handling.h>

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rcutils/allocator.h>
#include <rcutils/macros.h>
#include <rcutils/strdup.h>

// RCUTILS_REPORT_ERROR_HANDLING_ERRORS and RCUTILS_WARN_ON_TRUNCATION are set in the header below
#include "./error_handling_helpers.h"

// g_ is to global variable, as gtls_ is to global thread-local storage variable
RCUTILS_THREAD_LOCAL bool gtls_rcutils_thread_local_initialized = false;
RCUTILS_THREAD_LOCAL rcutils_error_state_t gtls_rcutils_error_state;
RCUTILS_THREAD_LOCAL bool gtls_rcutils_error_string_is_formatted = false;
RCUTILS_THREAD_LOCAL rcutils_error_string_t gtls_rcutils_error_string;
RCUTILS_THREAD_LOCAL bool gtls_rcutils_error_is_set = false;

rcutils_ret_t
rcutils_initialize_error_handling_thread_local_storage(rcutils_allocator_t allocator)
{
  if (gtls_rcutils_thread_local_initialized) {
    return RCUTILS_RET_OK;
  }

  // check if the given allocator is valid
  if (!rcutils_allocator_is_valid(&allocator)) {
#if RCUTILS_REPORT_ERROR_HANDLING_ERRORS
    RCUTILS_SAFE_FWRITE_TO_STDERR(
      "[rcutils|error_handling.c:" RCUTILS_STRINGIFY(__LINE__)
      "] rcutils_initialize_error_handling_thread_local_storage() given invalid allocator\n");
#endif
    return RCUTILS_RET_INVALID_ARGUMENT;
  }
  // right now the allocator is not used for anything
  // but other future implementations may need to use it
  // e.g. pthread which could only provide thread-local pointers would need to
  // allocate memory to which those pointers would point

  // forcing the values back to their initial state should force the thread-local storage
  // to initialize and do any required memory allocation
  gtls_rcutils_thread_local_initialized = true;
  rcutils_reset_error();
  RCUTILS_SET_ERROR_MSG("no error - initializing thread-local storage");
  rcutils_error_string_t throw_away = rcutils_get_error_string();
  (void)throw_away;
  rcutils_reset_error();

  // at this point the thread-local allocator, error state, and error string are all initialized
  return RCUTILS_RET_OK;
}

static
bool
__same_string(const char * str1, const char * str2, size_t count)
{
  assert(NULL != str1);
  assert(NULL != str2);
  return str1 == str2 || 0 == strncmp(str1, str2, count);
}

static
void
__format_overwriting_error_state_message(
  char * buffer,
  size_t buffer_size,
  const rcutils_error_state_t * new_error_state)
{
  assert(NULL != buffer);
  assert(0 != buffer_size);
  assert(SIZE_MAX > buffer_size);
  assert(NULL != new_error_state);

  int64_t bytes_left = (int64_t)buffer_size;
  do {
    char * offset = buffer;
    size_t written = 0;

    // write the first static part of the error message
    written = __rcutils_copy_string(
      offset, (size_t)bytes_left,
      "\n"
      ">>> [rcutils|error_handling.c:" RCUTILS_STRINGIFY(__LINE__) "] rcutils_set_error_state()\n"
      "This error state is being overwritten:\n"
      "\n"
      "  '");
    offset += written;
    bytes_left -= (int64_t)written;
    if (0 >= bytes_left) {break;}

    // write the old error string
    rcutils_error_string_t old_error_string = rcutils_get_error_string();
    written = __rcutils_copy_string(offset, sizeof(old_error_string.str), old_error_string.str);
    offset += written;
    bytes_left -= (int64_t)written;
    if (0 >= bytes_left) {break;}

    // write the middle part of the state error message
    written = __rcutils_copy_string(
      offset, (size_t)bytes_left,
      "'\n"
      "\n"
      "with this new error message:\n"
      "\n"
      "  '");
    offset += written;
    bytes_left -= (int64_t)written;
    if (0 >= bytes_left) {break;}

    // format error string for new error state and write it in
    rcutils_error_string_t new_error_string = {
      .str = "\0"
    };
    __rcutils_format_error_string(&new_error_string, new_error_state);
    written = __rcutils_copy_string(offset, sizeof(new_error_string.str), new_error_string.str);
    offset += written;
    bytes_left -= (int64_t)written;
    if (0 >= bytes_left) {break;}

    // write the last part of the state error message
    written = __rcutils_copy_string(
      offset, (size_t)bytes_left,
      "'\n"
      "\n"
      "rcutils_reset_error() should be called after error handling to avoid this.\n"
      "<<<\n");
    bytes_left -= (int64_t)written;
  } while (0);

#if RCUTILS_REPORT_ERROR_HANDLING_ERRORS
  // note that due to the way truncating is done above, and the size of the
  // output buffer used with this function in rcutils_set_error_state() below,
  // this should never evaluate to true, but it's here to be defensive and try
  // to catch programming mistakes in this file earlier.
  if (0 >= bytes_left) {
    RCUTILS_SAFE_FWRITE_TO_STDERR(
      "[rcutils|error_handling.c:" RCUTILS_STRINGIFY(__LINE__)
      "] rcutils_set_error_state() following error message was too long and will be truncated\n");
  }
#else
  (void)bytes_left;  // avoid scope could be reduced warning if in this case
#endif
}

void
rcutils_set_error_state(
  const char * error_string,
  const char * file,
  size_t line_number)
{
  rcutils_error_state_t error_state;

  if (NULL == error_string) {
#if RCUTILS_REPORT_ERROR_HANDLING_ERRORS
    RCUTILS_SAFE_FWRITE_TO_STDERR(
      "[rcutils|error_handling.c:" RCUTILS_STRINGIFY(__LINE__)
      "] rcutils_set_error_state() given null pointer for error_string, error was not set\n");
#endif
    return;
  }

  if (NULL == file) {
#if RCUTILS_REPORT_ERROR_HANDLING_ERRORS
    RCUTILS_SAFE_FWRITE_TO_STDERR(
      "[rcutils|error_handling.c:" RCUTILS_STRINGIFY(__LINE__)
      "] rcutils_set_error_state() given null pointer for file string, error was not set\n");
#endif
    return;
  }

  __rcutils_copy_string(error_state.message, sizeof(error_state.message), error_string);
  __rcutils_copy_string(error_state.file, sizeof(error_state.file), file);
  error_state.line_number = line_number;
#if RCUTILS_REPORT_ERROR_HANDLING_ERRORS
  // Only warn of overwritting if the new error is different from the old ones.
  size_t characters_to_compare = strnlen(error_string, RCUTILS_ERROR_MESSAGE_MAX_LENGTH);
  // assumption is that message length is <= max error string length
  static_assert(
    sizeof(gtls_rcutils_error_state.message) <= sizeof(gtls_rcutils_error_string.str),
    "expected error state's max message length to be less than or equal to error string max");
  if (
    gtls_rcutils_error_is_set &&
    !__same_string(error_string, gtls_rcutils_error_string.str, characters_to_compare) &&
    !__same_string(error_string, gtls_rcutils_error_state.message, characters_to_compare))
  {
    char output_buffer[4096];
    __format_overwriting_error_state_message(output_buffer, sizeof(output_buffer), &error_state);
    RCUTILS_SAFE_FWRITE_TO_STDERR(output_buffer);
  }
#endif
  gtls_rcutils_error_state = error_state;
  gtls_rcutils_error_string_is_formatted = false;
  gtls_rcutils_error_string = (const rcutils_error_string_t) {
    .str = "\0"
  };
  gtls_rcutils_error_is_set = true;
}

bool
rcutils_error_is_set(void)
{
  return gtls_rcutils_error_is_set;
}

const rcutils_error_state_t *
rcutils_get_error_state(void)
{
  return &gtls_rcutils_error_state;
}

rcutils_error_string_t
rcutils_get_error_string(void)
{
  if (!gtls_rcutils_error_is_set) {
    return (rcutils_error_string_t) {"error not set"};  // NOLINT(readability/braces)
  }
  if (!gtls_rcutils_error_string_is_formatted) {
    __rcutils_format_error_string(&gtls_rcutils_error_string, &gtls_rcutils_error_state);
    gtls_rcutils_error_string_is_formatted = true;
  }
  return gtls_rcutils_error_string;
}

void
rcutils_reset_error(void)
{
  gtls_rcutils_error_state = (const rcutils_error_state_t) {
    .message = {0}, .file = {0}, .line_number = 0
  };  // NOLINT(readability/braces)
  gtls_rcutils_error_string_is_formatted = false;
  gtls_rcutils_error_string = (const rcutils_error_string_t) {
    .str = "\0"
  };
  gtls_rcutils_error_is_set = false;
}

#ifdef __cplusplus
}
#endif
