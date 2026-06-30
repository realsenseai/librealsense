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

// Note: migrated from rmw/error_handling.h in 2017-04

#ifndef ERROR_HANDLING_HELPERS_H_
#define ERROR_HANDLING_HELPERS_H_

// When this define evaluates to true (default), then messages will printed to
// stderr when an error is encountered while setting the error state.
// For example, when memory cannot be allocated or a previous error state is
// being overwritten.
#ifndef RCUTILS_REPORT_ERROR_HANDLING_ERRORS
#define RCUTILS_REPORT_ERROR_HANDLING_ERRORS 1
#endif

// When this define evaluates to true (default), then a warning will be written
// to stderr using RCUTILS_SAFE_FWRITE_TO_STDERR.
#ifndef RCUTILS_WARN_ON_TRUNCATION
#define RCUTILS_WARN_ON_TRUNCATION 1
#endif

#ifndef __STDC_WANT_LIB_EXT1__
#define __STDC_WANT_LIB_EXT1__ 1  // indicate we would like memmove_s if available
#endif
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <rcutils/error_handling.h>

#ifdef __cplusplus
extern "C"
{
#endif

// do not use externally, internal function which is only to be used by error_handling.c
static
size_t
__rcutils_copy_string(char * dst, size_t dst_size, const char * src)
{
  assert(dst != NULL);
  assert(dst_size > 0);
  assert(src != NULL);
  // doesn't matter how long src actually is if it is longer than dst, so limit to dst + 1
  size_t src_length = strnlen(src, dst_size);
  size_t size_to_copy = src_length;
  // the destination must be one byte bigger to store the NULL terminating character
  if (src_length >= dst_size) {
    size_to_copy = dst_size - 1;
#if RCUTILS_REPORT_ERROR_HANDLING_ERRORS && RCUTILS_WARN_ON_TRUNCATION
    // truncation will be required
    RCUTILS_SAFE_FWRITE_TO_STDERR(
      "[rcutils|error_handling.c:" RCUTILS_STRINGIFY(__LINE__)
      "] an error string (message, file name, or formatted message) will be truncated\n");
#endif
  }

#ifdef __STDC_LIB_EXT1__
  errno_t ret = memmove_s(
    dst, dst_size,
    src, size_to_copy);  // this will always ensure truncation
  if (0 != ret) {
# if RCUTILS_REPORT_ERROR_HANDLING_ERRORS
    RCUTILS_SAFE_FWRITE_TO_STDERR(
      "[rcutils|error_handling.c:" RCUTILS_STRINGIFY(__LINE__)
      "] memmove_s failed, the error string may be malformed\n");
# endif
  }
#else
  (void)memmove(dst, src, size_to_copy);
#endif
  dst[size_to_copy] = '\0';
  return size_to_copy;
}

// do not use externally, internal function which is only to be used by error_handling.c
static
void
__rcutils_reverse_str(char * string_in, size_t string_len)
{
  assert(string_in != NULL);
  if (0 == string_len) {
    return;
  }
  size_t i = 0;
  size_t j = string_len - 1;
  for (; i < j; i++, j--) {
    char c = string_in[i];
    string_in[i] = string_in[j];
    string_in[j] = c;
  }
}

// do not use externally, internal function which is only to be used by error_handling.c
static
void
__rcutils_convert_uint64_t_into_c_str(uint64_t number, char * buffer, size_t buffer_size)
{
  assert(buffer != NULL);
  assert(buffer_size >= 21);
  (void)buffer_size;  // prevent warning in release builds where there is no assert(...)
  size_t i = 0;

  // if number is 0, short circuit
  if (number == 0) {
    buffer[0] = '0';
    buffer[1] = '\0';
    return;
  }

  // add the modulo 10 to the string and then integer divide by 10 until 0
  while (number != 0) {
    buffer[i++] = (char)(number % 10 + '0');
    number = number / 10;
  }

  // null terminate
  buffer[i] = '\0';

  // reverse the string in place
  __rcutils_reverse_str(buffer, strnlen(buffer, 21));
}

// do not use externally, internal function which is only to be used by error_handling.c
static
void
__rcutils_format_error_string(
  rcutils_error_string_t * error_string,
  const rcutils_error_state_t * error_state)
{
  assert(error_string != NULL);
  assert(error_state != NULL);
  static const char format_1[] = ", at ";
  static const char format_2[] = ":";
  char line_number_buffer[21];
  static_assert(
    sizeof(error_string->str) == (
      sizeof(error_state->message) +
      sizeof(format_1) - 1 /* minus the null-term */ +
      sizeof(error_state->file) +
      sizeof(format_2) - 1 /* minus the null-term */ +
      sizeof(line_number_buffer) - 1 /* minus the null-term */ +
      1  // null terminator
    ), "math error in static string formatting");
  char * offset = error_string->str;
  size_t bytes_left = sizeof(error_string->str);
  size_t written = __rcutils_copy_string(offset, bytes_left, error_state->message);
  offset += written;
  bytes_left -= written;
  written = __rcutils_copy_string(offset, bytes_left, format_1);
  offset += written;
  bytes_left -= written;
  written = __rcutils_copy_string(offset, bytes_left, error_state->file);
  offset += written;
  bytes_left -= written;
  written = __rcutils_copy_string(offset, bytes_left, format_2);
  offset += written;
  bytes_left -= written;
  __rcutils_convert_uint64_t_into_c_str(
    error_state->line_number, line_number_buffer, sizeof(line_number_buffer));
  written = __rcutils_copy_string(offset, bytes_left, line_number_buffer);
  offset += written;
  offset[0] = '\0';
}

#ifdef __cplusplus
}
#endif

#endif  // ERROR_HANDLING_HELPERS_H_
