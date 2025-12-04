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

/// \file

#ifndef RCUTILS__TIME_H_
#define RCUTILS__TIME_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#include "rcutils/macros.h"
#include "rcutils/types.h"
#include "rcutils/visibility_control.h"

/// Convenience macro to convert seconds to nanoseconds.
#define RCUTILS_S_TO_NS(seconds) ((seconds) * (1000LL * 1000LL * 1000LL))
/// Convenience macro to convert milliseconds to nanoseconds.
#define RCUTILS_MS_TO_NS(milliseconds) ((milliseconds) * (1000LL * 1000LL))
/// Convenience macro to convert microseconds to nanoseconds.
#define RCUTILS_US_TO_NS(microseconds) ((microseconds) * 1000LL)

/// Convenience macro to convert nanoseconds to seconds.
#define RCUTILS_NS_TO_S(nanoseconds) ((nanoseconds) / (1000LL * 1000LL * 1000LL))
/// Convenience macro to convert nanoseconds to milliseconds.
#define RCUTILS_NS_TO_MS(nanoseconds) ((nanoseconds) / (1000LL * 1000LL))
/// Convenience macro to convert nanoseconds to microseconds.
#define RCUTILS_NS_TO_US(nanoseconds) ((nanoseconds) / 1000LL)
/// Convenience macro for rcutils_steady_time_now(rcutils_time_point_value_t *).
#define RCUTILS_STEADY_TIME rcutils_steady_time_now

/// A single point in time, measured in nanoseconds since the Unix epoch.
typedef int64_t rcutils_time_point_value_t;
/// A duration of time, measured in nanoseconds.
typedef int64_t rcutils_duration_value_t;

/**
 * This function returns the time from a system clock.
 * The closest equivalent would be to std::chrono::system_clock::now();
 *
 * The resolution (e.g. nanoseconds vs microseconds) is not guaranteed.
 *
 * The now argument must point to an allocated rcutils_time_point_value_t object,
 * as the result is copied into this variable.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | Yes
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param[out] now a datafield in which the current time is stored
 * \return #RCUTILS_RET_OK if the current time was successfully obtained, or
 * \return #RCUTILS_RET_INVALID_ARGUMENT if any arguments are invalid, or
 * \return #RCUTILS_RET_ERROR if an unspecified error occur.
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_system_time_now(rcutils_time_point_value_t * now);

/// Retrieve the current time as a rcutils_time_point_value_t object.
/**
 * This function returns the time from a monotonically increasing clock.
 * The closest equivalent would be to std::chrono::steady_clock::now();
 *
 * The resolution (e.g. nanoseconds vs microseconds) is not guaranteed.
 *
 * The now argument must point to an allocated rcutils_time_point_value_t object,
 * as the result is copied into this variable.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | Yes
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param[out] now a struct in which the current time is stored
 * \return #RCUTILS_RET_OK if the current time was successfully obtained, or
 * \return #RCUTILS_RET_INVALID_ARGUMENT if any arguments are invalid, or
 * \return #RCUTILS_RET_ERROR if an unspecified error occur.
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_steady_time_now(rcutils_time_point_value_t * now);

/// Retrieve the current time as a rcutils_time_point_value_t object.
/**
 * This function returns the time from a monotonically increasing slew-free clock.
 *
 * The resolution (e.g. nanoseconds vs microseconds) is not guaranteed.
 *
 * The now argument must point to an allocated rcutils_time_point_value_t object,
 * as the result is copied into this variable.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | Yes
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param[out] now a struct in which the current time is stored
 * \return #RCUTILS_RET_OK if the current time was successfully obtained, or
 * \return #RCUTILS_RET_INVALID_ARGUMENT if any arguments are invalid, or
 * \return #RCUTILS_RET_ERROR if an unspecified error occur.
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_raw_steady_time_now(rcutils_time_point_value_t * now);

/// Return a time point as nanoseconds in a string.
/**
 * The number is always fixed width, with left padding zeros up to the maximum
 * number of digits the time point can represent.
 * Right now that is 19 digits (so 19 characters) for a signed 64-bit integer.
 * Negative values will have a leading `-`, so they will be one character
 * longer than the positive values.
 *
 * The recommended minimum size of the input string is 32 characters, but
 * 21 (` ` or `-` for sign, 19 digits, null terminator) should be sufficiently
 * large for both positive and negative values.
 * If the given string is not large enough, the result will be truncated.
 * If you need a string with variable width, using `snprintf()` directly is
 * recommended.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No [1]
 * Thread-Safe        | Yes
 * Uses Atomics       | No
 * Lock-Free          | Yes
 * <i>[1] if `snprintf()` does not allocate additional memory internally</i>
 *
 * \param[in] time_point the time to be made into a string
 * \param[out] str the output string in which it is stored
 * \param[in] str_size the size of the output string
 * \return #RCUTILS_RET_OK if successful (even if truncated), or
 * \return #RCUTILS_RET_INVALID_ARGUMENT if any arguments are invalid, or
 * \return #RCUTILS_RET_ERROR if an unspecified error occur.
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_time_point_value_as_nanoseconds_string(
  const rcutils_time_point_value_t * time_point,
  char * str,
  size_t str_size);

/// Return a time point as an datetime in local time with milliseconds in a string.
/**
 *
 * If the given string is not large enough, the result will be truncated.
 * If you need a string with variable width, using `snprintf()` directly is
 * recommended.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No [1]
 * Thread-Safe        | Yes
 * Uses Atomics       | No
 * Lock-Free          | Yes
 * <i>[1] if `snprintf()` does not allocate additional memory internally</i>
 *
 * \param[in] time_point the time to be made into a string
 * \param[out] str the output string in which it is stored
 * \param[in] str_size the size of the output string
 * \return #RCUTILS_RET_OK if successful (even if truncated), or
 * \return #RCUTILS_RET_INVALID_ARGUMENT if any arguments are invalid, or
 * \return #RCUTILS_RET_ERROR if an unspecified error occur.
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_time_point_value_as_date_string(
  const rcutils_time_point_value_t * time_point,
  char * str,
  size_t str_size);

/// Return a time point as floating point seconds in a string.
/**
 * The number is always fixed width, with left padding zeros up to the maximum
 * number of digits for the mantissa that the time point can represent and a
 * characteristic (fractional-part) with a fixed width of 9 digits.
 * Right now that means the mantissa is always 10 digits to add up to 19 total
 * for the signed 64-bit time point type.
 * Negative values will have a leading `-`, so they will be one character
 * longer then positive values.
 *
 * The recommended minimum size of the input string is 32 characters, but
 * 22 (` ` or `-` for sign, 19 digits, decimal point, null terminator) should
 * be sufficient for now.
 * If the given string is not large enough, the result will be truncated.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No [1]
 * Thread-Safe        | Yes
 * Uses Atomics       | No
 * Lock-Free          | Yes
 * <i>[1] if `snprintf()` does not allocate additional memory internally</i>
 *
 * \param[in] time_point the time to be made into a string
 * \param[out] str the output string in which it is stored
 * \param[in] str_size the size of the output string
 * \return #RCUTILS_RET_OK if successful (even if truncated), or
 * \return #RCUTILS_RET_INVALID_ARGUMENT if any arguments are invalid, or
 * \return #RCUTILS_RET_ERROR if an unspecified error occur.
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
rcutils_ret_t
rcutils_time_point_value_as_seconds_string(
  const rcutils_time_point_value_t * time_point,
  char * str,
  size_t str_size);

#ifdef __cplusplus
}
#endif

#endif  // RCUTILS__TIME_H_
