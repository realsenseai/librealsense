// Copyright 2017 Open Source Robotics Foundation, Inc.
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

#ifndef RCUTILS__MACROS_H_
#define RCUTILS__MACROS_H_

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef _WIN32
#define RCUTILS_WARN_UNUSED __attribute__((warn_unused_result))
#else
#define RCUTILS_WARN_UNUSED _Check_return_
#endif

// Note: this block was migrated from rmw/macros.h
// This block either sets RCUTILS_THREAD_LOCAL or RCUTILS_THREAD_LOCAL_PTHREAD.
#if defined _WIN32 || defined __CYGWIN__
// Windows or Cygwin
  #define RCUTILS_THREAD_LOCAL __declspec(thread)
#elif defined __APPLE__
// Apple OS's
  #include <TargetConditionals.h>
  #if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE
// iOS Simulator or iOS device
    #include <Availability.h>
    #if defined(__IPHONE_OS_VERSION_MIN_REQUIRED)
      #if __IPHONE_OS_VERSION_MAX_ALLOWED >= 100000
// iOS >= 10, thread local storage was added in iOS 10
        #define RCUTILS_THREAD_LOCAL _Thread_local
      #else
// iOS < 10, no thread local storage, so use pthread instead
        #define RCUTILS_THREAD_LOCAL_PTHREAD 1
        #undef RCUTILS_THREAD_LOCAL
      #endif
    #else
      #error "Unknown iOS version"
    #endif
  #elif TARGET_OS_MAC
// macOS
    #define RCUTILS_THREAD_LOCAL _Thread_local
  #else
    #error "Unknown Apple platform"
  #endif
#else
// Some other non-Windows, non-cygwin, non-apple OS
  #define RCUTILS_THREAD_LOCAL _Thread_local
#endif

#define RCUTILS_STRINGIFY_IMPL(x) #x
#define RCUTILS_STRINGIFY(x) RCUTILS_STRINGIFY_IMPL(x)
#define RCUTILS_UNUSED(x) (void)(x)

#define RCUTILS_JOIN_IMPL(arg1, arg2) arg1 ## arg2
#define RCUTILS_JOIN(arg1, arg2) RCUTILS_JOIN_IMPL(arg1, arg2)

#if defined _WIN32 || defined __CYGWIN__
/// Macro to annotate printf-like functions on Linux. Disabled on Windows.
#define RCUTILS_ATTRIBUTE_PRINTF_FORMAT(format_string_index, first_to_check_index)
#else
/// Macro to annotate printf-like functions which are relying on a format string and a variable
/// number of arguments.
/**
 * This enables GCC to emit warnings if dangerous patterns are detected.
 * See GCC documentation for details:
 * https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html
 *
 * For the following function:
 * int snprintf(char *str, size_t size, const char *format, ...);
 *              ^^^^^^^^^  ^^^^^^^^^^^  ^^^^^^^^^^^^^^^^^^  ^^^
 *              ARG 1      ARG 2        ARG 3               ARG 4
 *                                      format string       first optional argument
 *
 * format_string_index value would be 3, first_to_check_index value would be 4.
 *
 * IMPORTANT: the first argument has an index of ONE (not zero!).
 *
 * \param[in] format_string_index index of the format string passed to the function
 * \param[in] first_to_check_index index of the first "optional argument"
 */
#define RCUTILS_ATTRIBUTE_PRINTF_FORMAT(format_string_index, first_to_check_index) \
  __attribute__ ((format(printf, format_string_index, first_to_check_index)))
#endif  // !defined _WIN32 || defined __CYGWIN__

/// Macro to declare deprecation in the platform appropriate manner.
#ifndef _WIN32
# define RCUTILS_DEPRECATED __attribute__((deprecated))
#else
# define RCUTILS_DEPRECATED __declspec(deprecated)
#endif

/// Macro to declare deprecation in the platform appropriate manner with a message.
#ifndef _WIN32
# define RCUTILS_DEPRECATED_WITH_MSG(msg) __attribute__((deprecated(msg)))
#else
# define RCUTILS_DEPRECATED_WITH_MSG(msg) __declspec(deprecated(msg))
#endif

// Provide the compiler with branch prediction information
#ifndef _WIN32
/**
 * \def RCUTILS_LIKELY
 * Instruct the compiler to optimize for the case where the argument equals 1.
 */
# define RCUTILS_LIKELY(x) __builtin_expect((x), 1)
/**
 * \def RCUTILS_UNLIKELY
 * Instruct the compiler to optimize for the case where the argument equals 0.
 */
# define RCUTILS_UNLIKELY(x) __builtin_expect((x), 0)
#else
/**
 * \def RCUTILS_LIKELY
 * No op since Windows doesn't support providing branch prediction information.
 */
# define RCUTILS_LIKELY(x) (x)
/**
 * \def RCUTILS_UNLIKELY
 * No op since Windows doesn't support providing branch prediction information.
 */
# define RCUTILS_UNLIKELY(x) (x)
#endif  // _WIN32

#if defined RCUTILS_ENABLE_FAULT_INJECTION
#include "rcutils/testing/fault_injection.h"

/**
 * \def RCUTILS_CAN_RETURN_WITH_ERROR_OF
 * Indicating macro that the function intends to return possible error value.
 *
 * Put this macro as the first line in the function. For example:
 *
 * int rcutils_function_that_can_fail() {
 *   RCUTILS_CAN_RETURN_WITH_ERROR_OF(RCUTILS_RET_INVALID_ARGUMENT);
 *   ...  // rest of function
 * }
 *
 * For now, this macro just simply calls `RCUTILS_FAULT_INJECTION_MAYBE_RETURN_ERROR` if fault
 * injection is enabled. However, for source code, the macro annotation
 * `RCUTILS_CAN_RETURN_WITH_ERROR_OF` helps clarify that a function may return a value signifying
 * an error and what those are.
 *
 * In general, you should only include a return value that originates in the function you're
 * annotating instead of one that is merely passed on from a called function already annotated with
 *`RCUTILS_CAN_RETURN_WITH_ERROR_OF`. If you are passing on return values from a called function,
 * but that function is not annotated with `RCUTILS_CAN_RETURN_WITH_ERROR_OF`, then you might
 * consider annotating that function first. If for some reason that is not desired or possible,
 * then annotate your function as if the return values you are passing on originated from your
 * function.
 *
 * If the function can return multiple return values indicating separate failure types, each one
 * should go on a separate line.
 *
 * If in your function, there are expected effects on output parameters that occur during
 * the failure case, then it will introduce a discrepancy between fault injection testing and
 * production operation. This is because the fault injection will cause the function to return
 * where this macro is used, not at the location the error values are typically returned. To help
 * protect against this scenario you may consider adding unit tests that check your function does
 * not modify output parameters when it actually returns a failing error code if it's possible for
 * your code.
 *
 * If your function is void, this macro can be used without parameters. However, for the above
 * reasoning, there should be no side effects on output parameters for all possible early returns.
 *
 * \param error_return_value the value returned as a result of an error. It does not need to be
 * a rcutils_ret_t type. It could also be NULL, -1, a string error message, etc
 */
# define RCUTILS_CAN_RETURN_WITH_ERROR_OF(error_return_value) \
  RCUTILS_FAULT_INJECTION_MAYBE_RETURN_ERROR(error_return_value);

/**
 * \def RCUTILS_CAN_FAIL_WITH
 * Indicating macro similar to RCUTILS_CAN_RETURN_WITH_ERROR_OF but for use with more complicated
 * statements.
 *
 * The `failure_code` will be executed inside a scoped if block, so any variables declared within
 * will not be available outside of the macro.
 *
 * One example where you might need this version, is if a side-effect may occur within a function.
 * For example, in snprintf, rcutils_snprintf needs to set both errno and return -1 on failure.
 * This macro is used to capture both effects.
 *
 * \param failure_code Code that is representative of the failure case in this function.
 */
# define RCUTILS_CAN_FAIL_WITH(failure_code) \
  RCUTILS_FAULT_INJECTION_MAYBE_FAIL(failure_code);

#else
# define RCUTILS_CAN_RETURN_WITH_ERROR_OF(error_return_value)
# define RCUTILS_CAN_FAIL_WITH(failure_code)
#endif  // defined RCUTILS_ENABLE_FAULT_INJECTION

#ifdef __cplusplus
}
#endif

#endif  // RCUTILS__MACROS_H_
