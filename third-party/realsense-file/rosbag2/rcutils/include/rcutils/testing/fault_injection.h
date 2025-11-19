// Copyright 2020 Open Source Robotics Foundation, Inc.
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

#ifndef RCUTILS__TESTING__FAULT_INJECTION_H_
#define RCUTILS__TESTING__FAULT_INJECTION_H_
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#include "rcutils/macros.h"
#include "rcutils/visibility_control.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define RCUTILS_FAULT_INJECTION_NEVER_FAIL -1

#define RCUTILS_FAULT_INJECTION_FAIL_NOW 0

RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
bool
rcutils_fault_injection_is_test_complete(void);

/**
 * \brief Atomically set the fault injection counter.
 *
 * This is typically not the preferred method of interacting directly with the fault injection
 * logic, instead use `RCUTILS_FAULT_INJECTION_TEST` instead.
 *
 * This function may also be used for pausing code inside of a `RCUTILS_FAULT_INJECTION_TEST` with
 * something like the following:
 *
 * RCUTILS_FAULT_INJECTION_TEST({
 *     ...  // code to run with fault injection
 *     int64_t count = rcutils_fault_injection_get_count();
 *     rcutils_fault_injection_set_count(RCUTILS_FAULT_INJECTION_NEVER_FAIL);
 *     ...  // code to run without fault injection
 *     rcutils_fault_injection_set_count(count);
 *     ...  // code to run with fault injection
 * });
 *
 * \param count The count to set the fault injection counter to. If count is negative, then fault
 * injection errors will be disabled. The counter is globally initialized to
 * RCUTILS_FAULT_INJECTION_NEVER_FAIL.
 */
RCUTILS_PUBLIC
void
rcutils_fault_injection_set_count(int_least64_t count);

/**
 * \brief Atomically get the fault injection counter value
 *
 * This function is typically not used directly but instead indirectly inside an
 * `RCUTILS_FAULT_INJECTION_TEST`
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
int_least64_t
rcutils_fault_injection_get_count(void);

/**
 * \brief Implementation of fault injection decrementer
 *
 * This is included inside of macros, so it needs to be exported as a public function, but it
 * should not be used directly.
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
int_least64_t
_rcutils_fault_injection_maybe_fail(void);

/**
 * \def RCUTILS_FAULT_INJECTION_MAYBE_RETURN_ERROR
 * \brief This macro checks and decrements a static global variable atomic counter and returns
 * `return_value_on_error` if 0.
 *
 * This macro is not a function itself, so when this macro returns it will cause the calling
 * function to return with the return value.
 *
 * Set the counter with `RCUTILS_FAULT_INJECTION_SET_COUNT`. If the count is less than 0, then
 * `RCUTILS_FAULT_INJECTION_MAYBE_RETURN_ERROR` will not cause an early return.
 *
 * This macro is thread-safe, and ensures that at most one invocation results in a failure for each
 * time the fault injection counter is set with `RCUTILS_FAULT_INJECTION_SET_COUNT`
 *
 * \param return_value_on_error the value to return in the case of fault injected failure.
 */
#define RCUTILS_FAULT_INJECTION_MAYBE_RETURN_ERROR(return_value_on_error) \
  if (RCUTILS_FAULT_INJECTION_FAIL_NOW == _rcutils_fault_injection_maybe_fail()) { \
    printf( \
      "%s:%d Injecting fault and returning " #return_value_on_error "\n", __FILE__, __LINE__); \
    return return_value_on_error; \
  }

/**
 * \def RCUTILS_FAULT_INJECTION_MAYBE_FAIL
 * \brief This macro checks and decrements a static global variable atomic counter and executes
 * `failure_code` if the counter is 0 inside a scoped block (any variables declared in
 * failure_code) will not be avaliable outside of this scoped block.
 *
 * This macro is not a function itself, so it will cause the calling function to execute the code
 * from within an if loop.
 *
 * Set the counter with `RCUTILS_FAULT_INJECTION_SET_COUNT`. If the count is less than 0, then
 * `RCUTILS_FAULT_INJECTION_MAYBE_FAIL` will not execute the failure code.
 *
 * This macro is thread-safe, and ensures that at most one invocation results in a failure for each
 * time the fault injection counter is set with `RCUTILS_FAULT_INJECTION_SET_COUNT`
 *
 * \param failure_code the code to execute in the case of fault injected failure.
 */
#define RCUTILS_FAULT_INJECTION_MAYBE_FAIL(failure_code) \
  if (RCUTILS_FAULT_INJECTION_FAIL_NOW == _rcutils_fault_injection_maybe_fail()) { \
    printf( \
      "%s:%d Injecting fault and executing " #failure_code "\n", __FILE__, __LINE__); \
    failure_code; \
  }

/**
 * \def RCUTILS_FAULT_INJECTION_TEST
 *
 * The fault injection macro for use with unit tests to check that `code` can tolerate injected
 * failures at all points along the execution path where the indicating macros
 * `RCUTILS_CAN_RETURN_WITH_ERROR_OF` and `RCUTILS_CAN_FAIL_WITH` are located.
 *
 * This macro is intended to be used within a gtest function macro like 'TEST', 'TEST_F', etc.
 *
 * `code` is executed within a do-while loop and therefore any variables declared within are in
 * their own scope block.
 *
 * Here's a simple example:
 *  RCUTILS_FAULT_INJECTION_TEST(
 *    rcl_ret_t ret = rcl_init(argc, argv, options, context);
 *    if (RCL_RET_OK == ret)
 *    {
 *        ret = rcl_shutdown(context);
 *    }
 * });
 *
 * In this example, you will need have conditional execution based on the return value of
 * `rcl_init`. If it failed, then it wouldn't make sense to call rcl_shutdown. In your own test,
 * there might be similar logic that requires conditional checks. The goal of writing this test
 * is less about checking the behavior is consistent, but instead that failures do not cause
 * program crashes, memory errors, or unnecessary memory leaks.
 */
#define RCUTILS_FAULT_INJECTION_TEST(code) \
  do { \
    int fault_injection_count = 0; \
    do { \
      rcutils_fault_injection_set_count(fault_injection_count++); \
      code; \
    } while (!rcutils_fault_injection_is_test_complete()); \
    rcutils_fault_injection_set_count(RCUTILS_FAULT_INJECTION_NEVER_FAIL); \
  } while (0)

/**
 * \def RCUTILS_NO_FAULT_INJECTION
 *
 * A convenience macro built around rcutils_fault_injection_set_count() to pause fault
 * injection during `code` execution.
 * This macro is intended to be used within RCUTILS_FAULT_INJECTION_TEST() blocks.
 *
 * `code` is executed within a do-while loop and therefore any variables declared within are in
 * their own scope block.
 *
 * Here's a simple example:
 *  RCUTILS_FAULT_INJECTION_TEST({
 *    rcl_ret_t ret = rcl_init(argc, argv, options, context);
 *    if (RCL_RET_OK == ret)
 *    {
 *      RCUTILS_NO_FAULT_INJECTION({
 *        ret = rcl_shutdown(context);
 *      });
 *    }
 * });
 *
 * In this example, on successful rcl_init(), rcl_shutdown() is called while ensuring that
 * it will not fail due to fault injection.
 */
#define RCUTILS_NO_FAULT_INJECTION(code) \
  do { \
    int64_t no_fault_injection_count = rcutils_fault_injection_get_count(); \
    rcutils_fault_injection_set_count(RCUTILS_FAULT_INJECTION_NEVER_FAIL); \
    code; \
    rcutils_fault_injection_set_count(no_fault_injection_count); \
  } while (0)

#ifdef __cplusplus
}
#endif

#endif  // RCUTILS__TESTING__FAULT_INJECTION_H_
