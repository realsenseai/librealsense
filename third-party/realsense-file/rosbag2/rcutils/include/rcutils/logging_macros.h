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

#ifndef RCUTILS__LOGGING_MACROS_H_
#define RCUTILS__LOGGING_MACROS_H_

#include "rcutils/logging.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

// These are used for compiling out logging macros lower than a minimum severity.
#define RCUTILS_LOG_MIN_SEVERITY_DEBUG 0
#define RCUTILS_LOG_MIN_SEVERITY_INFO 1
#define RCUTILS_LOG_MIN_SEVERITY_WARN 2
#define RCUTILS_LOG_MIN_SEVERITY_ERROR 3
#define RCUTILS_LOG_MIN_SEVERITY_FATAL 4
#define RCUTILS_LOG_MIN_SEVERITY_NONE 5

/**
 * \def RCUTILS_LOG_MIN_SEVERITY
 * Define RCUTILS_LOG_MIN_SEVERITY=RCUTILS_LOG_MIN_SEVERITY_[DEBUG|INFO|WARN|ERROR|FATAL]
 * in your build options to compile out anything below that severity.
 * Use RCUTILS_LOG_MIN_SEVERITY_NONE to compile out all macros.
 */
#ifndef RCUTILS_LOG_MIN_SEVERITY
#define RCUTILS_LOG_MIN_SEVERITY RCUTILS_LOG_MIN_SEVERITY_DEBUG
#endif

/**
 * \def RCUTILS_LOG_NAMED
 * Log a message with given severity.
 * \param[in] name The name of the logger
 * \param[in] ... The format string, followed by the variable arguments for the format string
 */
#define RCUTILS_LOG_NAMED(severity, name, ...) \
  do { \
    RCUTILS_LOGGING_AUTOINIT; \
    static rcutils_log_location_t __rcutils_logging_location = {__func__, __FILE__, __LINE__}; \
    if (rcutils_logging_logger_is_enabled_for(name, severity)) { \
      rcutils_log_internal(&__rcutils_logging_location, severity, name, __VA_ARGS__); \
    } \
  } while (0)

/**
 * \def RCUTILS_LOG
 * Log a message with given severity.
 * \param[in] ... The format string, followed by the variable arguments for the format string
 */
#define RCUTILS_LOG(severity, ...) RCUTILS_LOG_NAMED(severity, NULL, __VA_ARGS__)

/**
 * \def RCUTILS_LOG_ONCE_NAMED
 * Log a message with given severity with the following condition:
 * - All log calls except the first one are ignored.
 *
 * \note The condition will only be evaluated if this logging statement is enabled.
 *
 * \param[in] name The name of the logger
 * \param[in] ... The format string, followed by the variable arguments for the format string
 */
#define RCUTILS_LOG_ONCE_NAMED(severity, name, ...) \
  do { \
    RCUTILS_LOGGING_AUTOINIT; \
    static rcutils_log_location_t __rcutils_logging_location = {__func__, __FILE__, __LINE__}; \
    if (rcutils_logging_logger_is_enabled_for(name, severity)) { \
      static int __rcutils_logging_once = 0; \
      if (RCUTILS_UNLIKELY(0 == __rcutils_logging_once)) { \
        __rcutils_logging_once = 1; \
        rcutils_log_internal(&__rcutils_logging_location, severity, name, __VA_ARGS__); \
      } \
    } \
  } while (0)

/**
 * \def RCUTILS_LOG_ONCE
 * Log a message with given severity with the following condition:
 * - All log calls except the first one are ignored.
 *
 * \note The condition will only be evaluated if this logging statement is enabled.
 *
 * \param[in] ... The format string, followed by the variable arguments for the format string
 */
#define RCUTILS_LOG_ONCE(severity, ...) RCUTILS_LOG_ONCE_NAMED(severity, NULL, __VA_ARGS__)

#define RCUTILS_LOG_CONDITION_NAMED(severity, condition, name, ...) \
  do { \
    RCUTILS_LOGGING_AUTOINIT; \
    static rcutils_log_location_t __rcutils_logging_location = {__func__, __FILE__, __LINE__}; \
    if (rcutils_logging_logger_is_enabled_for(name, severity)) { \
      if (condition) { \
        rcutils_log_internal(&__rcutils_logging_location, severity, name, __VA_ARGS__); \
      } \
    } \
  } while (0)

/**
 * \def RCUTILS_LOG_EXPRESSION_NAMED
 * Log a message with given severity with the following condition:
 * - Log calls are ignored when the expression evaluates to false.
 *
 * \note The condition will only be evaluated if this logging statement is enabled.
 *
 * \param[in] expression The expression determining if the message should be logged
 * \param[in] name The name of the logger
 * \param[in] ... The format string, followed by the variable arguments for the format string
 */
#define RCUTILS_LOG_EXPRESSION_NAMED(severity, expression, name, ...) \
  RCUTILS_LOG_CONDITION_NAMED(severity, expression, name, __VA_ARGS__)

/**
 * \def RCUTILS_LOG_EXPRESSION
 * Log a message with given severity with the following condition:
 * - Log calls are ignored when the expression evaluates to false.
 *
 * \note The condition will only be evaluated if this logging statement is enabled.
 *
 * \param[in] expression The expression determining if the message should be logged
 * \param[in] ... The format string, followed by the variable arguments for the format string
 */
#define RCUTILS_LOG_EXPRESSION(severity, expression, ...) \
  RCUTILS_LOG_CONDITION_NAMED(severity, expression, NULL, __VA_ARGS__)

/**
 * \def RCUTILS_LOG_FUNCTION_NAMED
 * Log a message with given severity with the following condition:
 * - Log calls are ignored when the function returns false.
 *
 * \note The condition will only be evaluated if this logging statement is enabled.
 *
 * \param[in] function The return value of the function determines if the message should be logged
 * \param[in] name The name of the logger
 * \param[in] ... The format string, followed by the variable arguments for the format string
 */
#define RCUTILS_LOG_FUNCTION_NAMED(severity, function, name, ...) \
  RCUTILS_LOG_CONDITION_NAMED(severity, (*function)(), name, __VA_ARGS__)

/**
 * \def RCUTILS_LOG_FUNCTION
 * Log a message with given severity with the following condition:
 * - Log calls are ignored when the function returns false.
 *
 * \note The condition will only be evaluated if this logging statement is enabled.
 *
 * \param[in] function The return value of the function determines if the message should be logged
 * \param[in] ... The format string, followed by the variable arguments for the format string
 */
#define RCUTILS_LOG_FUNCTION(severity, function, ...) \
  RCUTILS_LOG_CONDITION_NAMED(severity, (*function)(), NULL, __VA_ARGS__)

/**
 * \def RCUTILS_LOG_SKIPFIRST_NAMED
 * Log a message with given severity with the following condition:
 * - The first log call is ignored but all subsequent calls are processed.
 *
 * \note The condition will only be evaluated if this logging statement is enabled.
 *
 * \param[in] name The name of the logger
 * \param[in] ... The format string, followed by the variable arguments for the format string
 */
#define RCUTILS_LOG_SKIPFIRST_NAMED(severity, name, ...) \
  do { \
    RCUTILS_LOGGING_AUTOINIT; \
    static rcutils_log_location_t __rcutils_logging_location = {__func__, __FILE__, __LINE__}; \
    if (rcutils_logging_logger_is_enabled_for(name, severity)) { \
      static bool __rcutils_logging_first = true; \
      if (RCUTILS_UNLIKELY(true == __rcutils_logging_first)) { \
        __rcutils_logging_first = false; \
      } else { \
        rcutils_log_internal(&__rcutils_logging_location, severity, name, __VA_ARGS__); \
      } \
    } \
  } while (0)

/**
 * \def RCUTILS_LOG_SKIPFIRST
 * Log a message with given severity with the following condition:
 * - The first log call is ignored but all subsequent calls are processed.
 *
 * \note The condition will only be evaluated if this logging statement is enabled.
 *
 * \param[in] ... The format string, followed by the variable arguments for the format string
 */
#define RCUTILS_LOG_SKIPFIRST(severity, ...) \
  RCUTILS_LOG_SKIPFIRST_NAMED(severity, NULL, __VA_ARGS__)

/**
 * \def RCUTILS_LOG_THROTTLE_NAMED
 * Log a message with given severity with the following condition:
 * - Log calls are ignored if the last logged message is not longer ago than the specified duration.
 *
 * \note The condition will only be evaluated if this logging statement is enabled.
 *
 * \param[in] get_time_point Function that returns rcutils_ret_t and expects a rcutils_time_point_value_t pointer.
 * \param[in] duration The duration of the throttle interval as an integral value in milliseconds.
 * \param[in] name The name of the logger
 * \param[in] ... The format string, followed by the variable arguments for the format string
 */
#define RCUTILS_LOG_THROTTLE_NAMED(severity, get_time_point, duration, name, ...) \
  do { \
    RCUTILS_LOGGING_AUTOINIT; \
    static rcutils_log_location_t __rcutils_logging_location = {__func__, __FILE__, __LINE__}; \
    if (rcutils_logging_logger_is_enabled_for(name, severity)) { \
      static rcutils_duration_value_t __rcutils_logging_duration = RCUTILS_MS_TO_NS(duration); \
      static rcutils_time_point_value_t __rcutils_logging_last_logged = 0; \
      rcutils_time_point_value_t __rcutils_logging_now = 0; \
      bool __rcutils_logging_condition = true; \
      if (get_time_point(&__rcutils_logging_now) != RCUTILS_RET_OK) { \
        rcutils_log( \
            &__rcutils_logging_location, RCUTILS_LOG_SEVERITY_ERROR, "", \
            "%s() at %s:%d getting current steady time failed\n", \
            __func__, __FILE__, __LINE__); \
      } else { \
        __rcutils_logging_condition = __rcutils_logging_now >= \
          __rcutils_logging_last_logged + __rcutils_logging_duration; \
      } \
      if (RCUTILS_LIKELY(__rcutils_logging_condition)) { \
        __rcutils_logging_last_logged = __rcutils_logging_now; \
        rcutils_log_internal(&__rcutils_logging_location, severity, name, __VA_ARGS__); \
      } \
    } \
  } while (0)

/**
 * \def RCUTILS_LOG_THROTTLE
 * Log a message with given severity with the following condition:
 * - Log calls are ignored if the last logged message is not longer ago than the specified duration.
 *
 * \note The condition will only be evaluated if this logging statement is enabled.
 *
 * \param[in] get_time_point Function that returns rcutils_ret_t and expects a rcutils_time_point_value_t pointer.
 * \param[in] duration The duration of the throttle interval as an integral value in milliseconds.
 * \param[in] ... The format string, followed by the variable arguments for the format string
 */
#define RCUTILS_LOG_THROTTLE(severity, get_time_point, duration, ...) \
  RCUTILS_LOG_THROTTLE_NAMED(severity, get_time_point, duration, NULL, __VA_ARGS__)

/**
 * \def RCUTILS_LOG_SKIPFIRST_THROTTLE_NAMED
 * Log a message with given severity with the following conditions:
 * - The first log call is ignored but all subsequent calls are processed.
 * - Log calls are being ignored if the last logged message is not longer ago than the specified duration.
 *
 * \note The conditions will only be evaluated if this logging statement is enabled.
 *
 * \param[in] get_time_point Function that returns rcutils_ret_t and expects a rcutils_time_point_value_t pointer.
 * \param[in] duration The duration of the throttle interval as an integral value in milliseconds.
 * \param[in] name The name of the logger
 * \param[in] ... The format string, followed by the variable arguments for the format string
 */
#define RCUTILS_LOG_SKIPFIRST_THROTTLE_NAMED(severity, get_time_point, duration, name, ...) \
  do { \
    RCUTILS_LOGGING_AUTOINIT; \
    static rcutils_log_location_t __rcutils_logging_location = {__func__, __FILE__, __LINE__}; \
    if (rcutils_logging_logger_is_enabled_for(name, severity)) { \
      static rcutils_duration_value_t __rcutils_logging_duration = RCUTILS_MS_TO_NS(duration); \
      static rcutils_time_point_value_t __rcutils_logging_last_logged = 0; \
      rcutils_time_point_value_t __rcutils_logging_now = 0; \
      bool __rcutils_logging_condition = true; \
      if (get_time_point(&__rcutils_logging_now) != RCUTILS_RET_OK) { \
        rcutils_log( \
            &__rcutils_logging_location, RCUTILS_LOG_SEVERITY_ERROR, "", \
            "%s() at %s:%d getting current steady time failed\n", \
            __func__, __FILE__, __LINE__); \
      } else { \
        __rcutils_logging_condition = __rcutils_logging_now >= \
          __rcutils_logging_last_logged + __rcutils_logging_duration; \
      } \
      if (RCUTILS_LIKELY(__rcutils_logging_condition)) { \
        __rcutils_logging_last_logged = __rcutils_logging_now; \
        static bool __rcutils_logging_first = true; \
        if (RCUTILS_UNLIKELY(true == __rcutils_logging_first)) { \
          __rcutils_logging_first = false; \
        } else { \
          rcutils_log_internal(&__rcutils_logging_location, severity, name, __VA_ARGS__); \
        } \
      } \
    } \
  } while (0)

/**
 * \def RCUTILS_LOG_SKIPFIRST_THROTTLE
 * Log a message with given severity with the following conditions:
 * - The first log call is ignored but all subsequent calls are processed.
 * - Log calls are ignored if the last logged message is not longer ago than the specified duration.
 *
 * \note The conditions will only be evaluated if this logging statement is enabled.
 *
 * \param[in] get_time_point Function that returns rcutils_ret_t and expects a rcutils_time_point_value_t pointer.
 * \param[in] duration The duration of the throttle interval as an integral value in milliseconds.
 * \param[in] ... The format string, followed by the variable arguments for the format string
 */
#define RCUTILS_LOG_SKIPFIRST_THROTTLE(severity, get_time_point, duration, ...) \
  RCUTILS_LOG_SKIPFIRST_THROTTLE_NAMED(severity, get_time_point, duration, NULL, __VA_ARGS__)

#if (RCUTILS_LOG_MIN_SEVERITY > RCUTILS_LOG_MIN_SEVERITY_DEBUG)
// empty logging macros for severity DEBUG when disabled at compile time
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_DEBUG(format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_DEBUG_NAMED(name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_DEBUG_ONCE(format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_DEBUG_ONCE_NAMED(name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_DEBUG_EXPRESSION(expression, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_DEBUG_EXPRESSION_NAMED(expression, name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_DEBUG_FUNCTION(function, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_DEBUG_FUNCTION_NAMED(function, name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_DEBUG_SKIPFIRST(format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_DEBUG_SKIPFIRST_NAMED(name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_DEBUG_THROTTLE(get_time_point, duration, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_DEBUG_SKIPFIRST_THROTTLE(get_time_point, duration, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_DEBUG_THROTTLE_NAMED(get_time_point, duration, name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_DEBUG_SKIPFIRST_THROTTLE_NAMED(get_time_point, duration, name, format, ...)
#else
/**
 * \def RCUTILS_LOG_DEBUG
 * \copydoc RCUTILS_LOG
 */
# define RCUTILS_LOG_DEBUG(...) RCUTILS_LOG(RCUTILS_LOG_SEVERITY_DEBUG, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_DEBUG_NAMED
 * \copydoc RCUTILS_LOG_NAMED
 */
# define RCUTILS_LOG_DEBUG_NAMED(name, ...) \
  RCUTILS_LOG_NAMED(RCUTILS_LOG_SEVERITY_DEBUG, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_DEBUG_ONCE
 * \copydoc RCUTILS_LOG_ONCE
 */
# define RCUTILS_LOG_DEBUG_ONCE(...) RCUTILS_LOG_ONCE(RCUTILS_LOG_SEVERITY_DEBUG, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_DEBUG_ONCE_NAMED
 * \copydoc RCUTILS_LOG_ONCE_NAMED
 */
# define RCUTILS_LOG_DEBUG_ONCE_NAMED(name, ...) \
  RCUTILS_LOG_ONCE_NAMED(RCUTILS_LOG_SEVERITY_DEBUG, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_DEBUG_EXPRESSION
 * \copydoc RCUTILS_LOG_EXPRESSION
 */
# define RCUTILS_LOG_DEBUG_EXPRESSION(expression, ...) \
  RCUTILS_LOG_EXPRESSION(RCUTILS_LOG_SEVERITY_DEBUG, expression, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_DEBUG_EXPRESSION_NAMED
 * \copydoc RCUTILS_LOG_EXPRESSION_NAMED
 */
# define RCUTILS_LOG_DEBUG_EXPRESSION_NAMED(expression, name, ...) \
  RCUTILS_LOG_EXPRESSION_NAMED(RCUTILS_LOG_SEVERITY_DEBUG, expression, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_DEBUG_FUNCTION
 * \copydoc RCUTILS_LOG_FUNCTION
 */
# define RCUTILS_LOG_DEBUG_FUNCTION(function, ...) \
  RCUTILS_LOG_FUNCTION(RCUTILS_LOG_SEVERITY_DEBUG, function, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_DEBUG_FUNCTION_NAMED
 * \copydoc RCUTILS_LOG_FUNCTION_NAMED
 */
# define RCUTILS_LOG_DEBUG_FUNCTION_NAMED(function, name, ...) \
  RCUTILS_LOG_FUNCTION_NAMED(RCUTILS_LOG_SEVERITY_DEBUG, function, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_DEBUG_SKIPFIRST
 * \copydoc RCUTILS_LOG_SKIPFIRST
 */
# define RCUTILS_LOG_DEBUG_SKIPFIRST(...) \
  RCUTILS_LOG_SKIPFIRST(RCUTILS_LOG_SEVERITY_DEBUG, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_DEBUG_SKIPFIRST_NAMED
 * \copydoc RCUTILS_LOG_SKIPFIRST_NAMED
 */
# define RCUTILS_LOG_DEBUG_SKIPFIRST_NAMED(name, ...) \
  RCUTILS_LOG_SKIPFIRST_NAMED(RCUTILS_LOG_SEVERITY_DEBUG, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_DEBUG_THROTTLE
 * \copydoc RCUTILS_LOG_THROTTLE
 */
# define RCUTILS_LOG_DEBUG_THROTTLE(get_time_point, duration, ...) \
  RCUTILS_LOG_THROTTLE(RCUTILS_LOG_SEVERITY_DEBUG, get_time_point, duration, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_DEBUG_THROTTLE_NAMED
 * \copydoc RCUTILS_LOG_THROTTLE_NAMED
 */
# define RCUTILS_LOG_DEBUG_THROTTLE_NAMED(get_time_point, duration, name, ...) \
  RCUTILS_LOG_THROTTLE_NAMED(RCUTILS_LOG_SEVERITY_DEBUG, get_time_point, duration, name, \
    __VA_ARGS__)
/**
 * \def RCUTILS_LOG_DEBUG_SKIPFIRST_THROTTLE
 * \copydoc RCUTILS_LOG_SKIPFIRST_THROTTLE
 */
# define RCUTILS_LOG_DEBUG_SKIPFIRST_THROTTLE(get_time_point, duration, ...) \
  RCUTILS_LOG_SKIPFIRST_THROTTLE(RCUTILS_LOG_SEVERITY_DEBUG, get_time_point, duration, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_DEBUG_SKIPFIRST_THROTTLE_NAMED
 * \copydoc RCUTILS_LOG_SKIPFIRST_THROTTLE_NAMED
 */
# define RCUTILS_LOG_DEBUG_SKIPFIRST_THROTTLE_NAMED(get_time_point, duration, name, ...) \
  RCUTILS_LOG_SKIPFIRST_THROTTLE_NAMED(RCUTILS_LOG_SEVERITY_DEBUG, get_time_point, duration, name, \
    __VA_ARGS__)
#endif

#if (RCUTILS_LOG_MIN_SEVERITY > RCUTILS_LOG_MIN_SEVERITY_INFO)
// empty logging macros for severity INFO when disabled at compile time
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_INFO(format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_INFO_NAMED(name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_INFO_ONCE(format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_INFO_ONCE_NAMED(name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_INFO_EXPRESSION(expression, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_INFO_EXPRESSION_NAMED(expression, name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_INFO_FUNCTION(function, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_INFO_FUNCTION_NAMED(function, name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_INFO_SKIPFIRST(format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_INFO_SKIPFIRST_NAMED(name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_INFO_THROTTLE(get_time_point, duration, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_INFO_SKIPFIRST_THROTTLE(get_time_point, duration, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_INFO_THROTTLE_NAMED(get_time_point, duration, name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_INFO_SKIPFIRST_THROTTLE_NAMED(get_time_point, duration, name, format, ...)
#else
/**
 * \def RCUTILS_LOG_INFO
 * \copydoc RCUTILS_LOG
 */
# define RCUTILS_LOG_INFO(...) RCUTILS_LOG(RCUTILS_LOG_SEVERITY_INFO, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_INFO_NAMED
 * \copydoc RCUTILS_LOG_NAMED
 */
# define RCUTILS_LOG_INFO_NAMED(name, ...) \
  RCUTILS_LOG_NAMED(RCUTILS_LOG_SEVERITY_INFO, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_INFO_ONCE
 * \copydoc RCUTILS_LOG_ONCE
 */
# define RCUTILS_LOG_INFO_ONCE(...) RCUTILS_LOG_ONCE(RCUTILS_LOG_SEVERITY_INFO, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_INFO_ONCE_NAMED
 * \copydoc RCUTILS_LOG_ONCE_NAMED
 */
# define RCUTILS_LOG_INFO_ONCE_NAMED(name, ...) \
  RCUTILS_LOG_ONCE_NAMED(RCUTILS_LOG_SEVERITY_INFO, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_INFO_EXPRESSION
 * \copydoc RCUTILS_LOG_EXPRESSION
 */
# define RCUTILS_LOG_INFO_EXPRESSION(expression, ...) \
  RCUTILS_LOG_EXPRESSION(RCUTILS_LOG_SEVERITY_INFO, expression, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_INFO_EXPRESSION_NAMED
 * \copydoc RCUTILS_LOG_EXPRESSION_NAMED
 */
# define RCUTILS_LOG_INFO_EXPRESSION_NAMED(expression, name, ...) \
  RCUTILS_LOG_EXPRESSION_NAMED(RCUTILS_LOG_SEVERITY_INFO, expression, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_INFO_FUNCTION
 * \copydoc RCUTILS_LOG_FUNCTION
 */
# define RCUTILS_LOG_INFO_FUNCTION(function, ...) \
  RCUTILS_LOG_FUNCTION(RCUTILS_LOG_SEVERITY_INFO, function, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_INFO_FUNCTION_NAMED
 * \copydoc RCUTILS_LOG_FUNCTION_NAMED
 */
# define RCUTILS_LOG_INFO_FUNCTION_NAMED(function, name, ...) \
  RCUTILS_LOG_FUNCTION_NAMED(RCUTILS_LOG_SEVERITY_INFO, function, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_INFO_SKIPFIRST
 * \copydoc RCUTILS_LOG_SKIPFIRST
 */
# define RCUTILS_LOG_INFO_SKIPFIRST(...) \
  RCUTILS_LOG_SKIPFIRST(RCUTILS_LOG_SEVERITY_INFO, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_INFO_SKIPFIRST_NAMED
 * \copydoc RCUTILS_LOG_SKIPFIRST_NAMED
 */
# define RCUTILS_LOG_INFO_SKIPFIRST_NAMED(name, ...) \
  RCUTILS_LOG_SKIPFIRST_NAMED(RCUTILS_LOG_SEVERITY_INFO, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_INFO_THROTTLE
 * \copydoc RCUTILS_LOG_THROTTLE
 */
# define RCUTILS_LOG_INFO_THROTTLE(get_time_point, duration, ...) \
  RCUTILS_LOG_THROTTLE(RCUTILS_LOG_SEVERITY_INFO, get_time_point, duration, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_INFO_THROTTLE_NAMED
 * \copydoc RCUTILS_LOG_THROTTLE_NAMED
 */
# define RCUTILS_LOG_INFO_THROTTLE_NAMED(get_time_point, duration, name, ...) \
  RCUTILS_LOG_THROTTLE_NAMED(RCUTILS_LOG_SEVERITY_INFO, get_time_point, duration, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_INFO_SKIPFIRST_THROTTLE
 * \copydoc RCUTILS_LOG_SKIPFIRST_THROTTLE
 */
# define RCUTILS_LOG_INFO_SKIPFIRST_THROTTLE(get_time_point, duration, ...) \
  RCUTILS_LOG_SKIPFIRST_THROTTLE(RCUTILS_LOG_SEVERITY_INFO, get_time_point, duration, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_INFO_SKIPFIRST_THROTTLE_NAMED
 * \copydoc RCUTILS_LOG_SKIPFIRST_THROTTLE_NAMED
 */
# define RCUTILS_LOG_INFO_SKIPFIRST_THROTTLE_NAMED(get_time_point, duration, name, ...) \
  RCUTILS_LOG_SKIPFIRST_THROTTLE_NAMED(RCUTILS_LOG_SEVERITY_INFO, get_time_point, duration, name, \
    __VA_ARGS__)
#endif

#if (RCUTILS_LOG_MIN_SEVERITY > RCUTILS_LOG_MIN_SEVERITY_WARN)
// empty logging macros for severity WARN when disabled at compile time
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_WARN(format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_WARN_NAMED(name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_WARN_ONCE(format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_WARN_ONCE_NAMED(name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_WARN_EXPRESSION(expression, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_WARN_EXPRESSION_NAMED(expression, name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_WARN_FUNCTION(function, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_WARN_FUNCTION_NAMED(function, name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_WARN_SKIPFIRST(format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_WARN_SKIPFIRST_NAMED(name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_WARN_THROTTLE(get_time_point, duration, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_WARN_SKIPFIRST_THROTTLE(get_time_point, duration, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_WARN_THROTTLE_NAMED(get_time_point, duration, name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_WARN_SKIPFIRST_THROTTLE_NAMED(get_time_point, duration, name, format, ...)
#else
/**
 * \def RCUTILS_LOG_WARN
 * \copydoc RCUTILS_LOG
 */
# define RCUTILS_LOG_WARN(...) RCUTILS_LOG(RCUTILS_LOG_SEVERITY_WARN, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_WARN_NAMED
 * \copydoc RCUTILS_LOG_NAMED
 */
# define RCUTILS_LOG_WARN_NAMED(name, ...) \
  RCUTILS_LOG_NAMED(RCUTILS_LOG_SEVERITY_WARN, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_WARN_ONCE
 * \copydoc RCUTILS_LOG_ONCE
 */
# define RCUTILS_LOG_WARN_ONCE(...) RCUTILS_LOG_ONCE(RCUTILS_LOG_SEVERITY_WARN, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_WARN_ONCE_NAMED
 * \copydoc RCUTILS_LOG_ONCE_NAMED
 */
# define RCUTILS_LOG_WARN_ONCE_NAMED(name, ...) \
  RCUTILS_LOG_ONCE_NAMED(RCUTILS_LOG_SEVERITY_WARN, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_WARN_EXPRESSION
 * \copydoc RCUTILS_LOG_EXPRESSION
 */
# define RCUTILS_LOG_WARN_EXPRESSION(expression, ...) \
  RCUTILS_LOG_EXPRESSION(RCUTILS_LOG_SEVERITY_WARN, expression, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_WARN_EXPRESSION_NAMED
 * \copydoc RCUTILS_LOG_EXPRESSION_NAMED
 */
# define RCUTILS_LOG_WARN_EXPRESSION_NAMED(expression, name, ...) \
  RCUTILS_LOG_EXPRESSION_NAMED(RCUTILS_LOG_SEVERITY_WARN, expression, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_WARN_FUNCTION
 * \copydoc RCUTILS_LOG_FUNCTION
 */
# define RCUTILS_LOG_WARN_FUNCTION(function, ...) \
  RCUTILS_LOG_FUNCTION(RCUTILS_LOG_SEVERITY_WARN, function, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_WARN_FUNCTION_NAMED
 * \copydoc RCUTILS_LOG_FUNCTION_NAMED
 */
# define RCUTILS_LOG_WARN_FUNCTION_NAMED(function, name, ...) \
  RCUTILS_LOG_FUNCTION_NAMED(RCUTILS_LOG_SEVERITY_WARN, function, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_WARN_SKIPFIRST
 * \copydoc RCUTILS_LOG_SKIPFIRST
 */
# define RCUTILS_LOG_WARN_SKIPFIRST(...) \
  RCUTILS_LOG_SKIPFIRST(RCUTILS_LOG_SEVERITY_WARN, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_WARN_SKIPFIRST_NAMED
 * \copydoc RCUTILS_LOG_SKIPFIRST_NAMED
 */
# define RCUTILS_LOG_WARN_SKIPFIRST_NAMED(name, ...) \
  RCUTILS_LOG_SKIPFIRST_NAMED(RCUTILS_LOG_SEVERITY_WARN, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_WARN_THROTTLE
 * \copydoc RCUTILS_LOG_THROTTLE
 */
# define RCUTILS_LOG_WARN_THROTTLE(get_time_point, duration, ...) \
  RCUTILS_LOG_THROTTLE(RCUTILS_LOG_SEVERITY_WARN, get_time_point, duration, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_WARN_THROTTLE_NAMED
 * \copydoc RCUTILS_LOG_THROTTLE_NAMED
 */
# define RCUTILS_LOG_WARN_THROTTLE_NAMED(get_time_point, duration, name, ...) \
  RCUTILS_LOG_THROTTLE_NAMED(RCUTILS_LOG_SEVERITY_WARN, get_time_point, duration, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_WARN_SKIPFIRST_THROTTLE
 * \copydoc RCUTILS_LOG_SKIPFIRST_THROTTLE
 */
# define RCUTILS_LOG_WARN_SKIPFIRST_THROTTLE(get_time_point, duration, ...) \
  RCUTILS_LOG_SKIPFIRST_THROTTLE(RCUTILS_LOG_SEVERITY_WARN, get_time_point, duration, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_WARN_SKIPFIRST_THROTTLE_NAMED
 * \copydoc RCUTILS_LOG_SKIPFIRST_THROTTLE_NAMED
 */
# define RCUTILS_LOG_WARN_SKIPFIRST_THROTTLE_NAMED(get_time_point, duration, name, ...) \
  RCUTILS_LOG_SKIPFIRST_THROTTLE_NAMED(RCUTILS_LOG_SEVERITY_WARN, get_time_point, duration, name, \
    __VA_ARGS__)
#endif

#if (RCUTILS_LOG_MIN_SEVERITY > RCUTILS_LOG_MIN_SEVERITY_ERROR)
// empty logging macros for severity ERROR when disabled at compile time
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_ERROR(format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_ERROR_NAMED(name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_ERROR_ONCE(format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_ERROR_ONCE_NAMED(name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_ERROR_EXPRESSION(expression, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_ERROR_EXPRESSION_NAMED(expression, name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_ERROR_FUNCTION(function, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_ERROR_FUNCTION_NAMED(function, name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_ERROR_SKIPFIRST(format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_ERROR_SKIPFIRST_NAMED(name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_ERROR_THROTTLE(get_time_point, duration, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_ERROR_SKIPFIRST_THROTTLE(get_time_point, duration, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_ERROR_THROTTLE_NAMED(get_time_point, duration, name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_ERROR_SKIPFIRST_THROTTLE_NAMED(get_time_point, duration, name, format, ...)
#else
/**
 * \def RCUTILS_LOG_ERROR
 * \copydoc RCUTILS_LOG
 */
# define RCUTILS_LOG_ERROR(...) RCUTILS_LOG(RCUTILS_LOG_SEVERITY_ERROR, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_ERROR_NAMED
 * \copydoc RCUTILS_LOG_NAMED
 */
# define RCUTILS_LOG_ERROR_NAMED(name, ...) \
  RCUTILS_LOG_NAMED(RCUTILS_LOG_SEVERITY_ERROR, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_ERROR_ONCE
 * \copydoc RCUTILS_LOG_ONCE
 */
# define RCUTILS_LOG_ERROR_ONCE(...) RCUTILS_LOG_ONCE(RCUTILS_LOG_SEVERITY_ERROR, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_ERROR_ONCE_NAMED
 * \copydoc RCUTILS_LOG_ONCE_NAMED
 */
# define RCUTILS_LOG_ERROR_ONCE_NAMED(name, ...) \
  RCUTILS_LOG_ONCE_NAMED(RCUTILS_LOG_SEVERITY_ERROR, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_ERROR_EXPRESSION
 * \copydoc RCUTILS_LOG_EXPRESSION
 */
# define RCUTILS_LOG_ERROR_EXPRESSION(expression, ...) \
  RCUTILS_LOG_EXPRESSION(RCUTILS_LOG_SEVERITY_ERROR, expression, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_ERROR_EXPRESSION_NAMED
 * \copydoc RCUTILS_LOG_EXPRESSION_NAMED
 */
# define RCUTILS_LOG_ERROR_EXPRESSION_NAMED(expression, name, ...) \
  RCUTILS_LOG_EXPRESSION_NAMED(RCUTILS_LOG_SEVERITY_ERROR, expression, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_ERROR_FUNCTION
 * \copydoc RCUTILS_LOG_FUNCTION
 */
# define RCUTILS_LOG_ERROR_FUNCTION(function, ...) \
  RCUTILS_LOG_FUNCTION(RCUTILS_LOG_SEVERITY_ERROR, function, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_ERROR_FUNCTION_NAMED
 * \copydoc RCUTILS_LOG_FUNCTION_NAMED
 */
# define RCUTILS_LOG_ERROR_FUNCTION_NAMED(function, name, ...) \
  RCUTILS_LOG_FUNCTION_NAMED(RCUTILS_LOG_SEVERITY_ERROR, function, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_ERROR_SKIPFIRST
 * \copydoc RCUTILS_LOG_SKIPFIRST
 */
# define RCUTILS_LOG_ERROR_SKIPFIRST(...) \
  RCUTILS_LOG_SKIPFIRST(RCUTILS_LOG_SEVERITY_ERROR, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_ERROR_SKIPFIRST_NAMED
 * \copydoc RCUTILS_LOG_SKIPFIRST_NAMED
 */
# define RCUTILS_LOG_ERROR_SKIPFIRST_NAMED(name, ...) \
  RCUTILS_LOG_SKIPFIRST_NAMED(RCUTILS_LOG_SEVERITY_ERROR, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_ERROR_THROTTLE
 * \copydoc RCUTILS_LOG_THROTTLE
 */
# define RCUTILS_LOG_ERROR_THROTTLE(get_time_point, duration, ...) \
  RCUTILS_LOG_THROTTLE(RCUTILS_LOG_SEVERITY_ERROR, get_time_point, duration, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_ERROR_THROTTLE_NAMED
 * \copydoc RCUTILS_LOG_THROTTLE_NAMED
 */
# define RCUTILS_LOG_ERROR_THROTTLE_NAMED(get_time_point, duration, name, ...) \
  RCUTILS_LOG_THROTTLE_NAMED(RCUTILS_LOG_SEVERITY_ERROR, get_time_point, duration, name, \
    __VA_ARGS__)
/**
 * \def RCUTILS_LOG_ERROR_SKIPFIRST_THROTTLE
 * \copydoc RCUTILS_LOG_SKIPFIRST_THROTTLE
 */
# define RCUTILS_LOG_ERROR_SKIPFIRST_THROTTLE(get_time_point, duration, ...) \
  RCUTILS_LOG_SKIPFIRST_THROTTLE(RCUTILS_LOG_SEVERITY_ERROR, get_time_point, duration, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_ERROR_SKIPFIRST_THROTTLE_NAMED
 * \copydoc RCUTILS_LOG_SKIPFIRST_THROTTLE_NAMED
 */
# define RCUTILS_LOG_ERROR_SKIPFIRST_THROTTLE_NAMED(get_time_point, duration, name, ...) \
  RCUTILS_LOG_SKIPFIRST_THROTTLE_NAMED(RCUTILS_LOG_SEVERITY_ERROR, get_time_point, duration, name, \
    __VA_ARGS__)
#endif

#if (RCUTILS_LOG_MIN_SEVERITY > RCUTILS_LOG_MIN_SEVERITY_FATAL)
// empty logging macros for severity FATAL when disabled at compile time
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_FATAL(format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_FATAL_NAMED(name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_FATAL_ONCE(format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_FATAL_ONCE_NAMED(name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_FATAL_EXPRESSION(expression, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_FATAL_EXPRESSION_NAMED(expression, name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_FATAL_FUNCTION(function, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_FATAL_FUNCTION_NAMED(function, name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_FATAL_SKIPFIRST(format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_FATAL_SKIPFIRST_NAMED(name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_FATAL_THROTTLE(get_time_point, duration, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_FATAL_SKIPFIRST_THROTTLE(get_time_point, duration, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_FATAL_THROTTLE_NAMED(get_time_point, duration, name, format, ...)
/// Empty logging macro due to the preprocessor definition of RCUTILS_LOG_MIN_SEVERITY.
# define RCUTILS_LOG_FATAL_SKIPFIRST_THROTTLE_NAMED(get_time_point, duration, name, format, ...)
#else
/**
 * \def RCUTILS_LOG_FATAL
 * \copydoc RCUTILS_LOG
 */
# define RCUTILS_LOG_FATAL(...) RCUTILS_LOG(RCUTILS_LOG_SEVERITY_FATAL, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_FATAL_NAMED
 * \copydoc RCUTILS_LOG_NAMED
 */
# define RCUTILS_LOG_FATAL_NAMED(name, ...) \
  RCUTILS_LOG_NAMED(RCUTILS_LOG_SEVERITY_FATAL, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_FATAL_ONCE
 * \copydoc RCUTILS_LOG_ONCE
 */
# define RCUTILS_LOG_FATAL_ONCE(...) RCUTILS_LOG_ONCE(RCUTILS_LOG_SEVERITY_FATAL, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_FATAL_ONCE_NAMED
 * \copydoc RCUTILS_LOG_ONCE_NAMED
 */
# define RCUTILS_LOG_FATAL_ONCE_NAMED(name, ...) \
  RCUTILS_LOG_ONCE_NAMED(RCUTILS_LOG_SEVERITY_FATAL, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_FATAL_EXPRESSION
 * \copydoc RCUTILS_LOG_EXPRESSION
 */
# define RCUTILS_LOG_FATAL_EXPRESSION(expression, ...) \
  RCUTILS_LOG_EXPRESSION(RCUTILS_LOG_SEVERITY_FATAL, expression, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_FATAL_EXPRESSION_NAMED
 * \copydoc RCUTILS_LOG_EXPRESSION_NAMED
 */
# define RCUTILS_LOG_FATAL_EXPRESSION_NAMED(expression, name, ...) \
  RCUTILS_LOG_EXPRESSION_NAMED(RCUTILS_LOG_SEVERITY_FATAL, expression, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_FATAL_FUNCTION
 * \copydoc RCUTILS_LOG_FUNCTION
 */
# define RCUTILS_LOG_FATAL_FUNCTION(function, ...) \
  RCUTILS_LOG_FUNCTION(RCUTILS_LOG_SEVERITY_FATAL, function, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_FATAL_FUNCTION_NAMED
 * \copydoc RCUTILS_LOG_FUNCTION_NAMED
 */
# define RCUTILS_LOG_FATAL_FUNCTION_NAMED(function, name, ...) \
  RCUTILS_LOG_FUNCTION_NAMED(RCUTILS_LOG_SEVERITY_FATAL, function, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_FATAL_SKIPFIRST
 * \copydoc RCUTILS_LOG_SKIPFIRST
 */
# define RCUTILS_LOG_FATAL_SKIPFIRST(...) \
  RCUTILS_LOG_SKIPFIRST(RCUTILS_LOG_SEVERITY_FATAL, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_FATAL_SKIPFIRST_NAMED
 * \copydoc RCUTILS_LOG_SKIPFIRST_NAMED
 */
# define RCUTILS_LOG_FATAL_SKIPFIRST_NAMED(name, ...) \
  RCUTILS_LOG_SKIPFIRST_NAMED(RCUTILS_LOG_SEVERITY_FATAL, name, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_FATAL_THROTTLE
 * \copydoc RCUTILS_LOG_THROTTLE
 */
# define RCUTILS_LOG_FATAL_THROTTLE(get_time_point, duration, ...) \
  RCUTILS_LOG_THROTTLE(RCUTILS_LOG_SEVERITY_FATAL, get_time_point, duration, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_FATAL_THROTTLE_NAMED
 * \copydoc RCUTILS_LOG_THROTTLE_NAMED
 */
# define RCUTILS_LOG_FATAL_THROTTLE_NAMED(get_time_point, duration, name, ...) \
  RCUTILS_LOG_THROTTLE_NAMED(RCUTILS_LOG_SEVERITY_FATAL, get_time_point, duration, name, \
    __VA_ARGS__)
/**
 * \def RCUTILS_LOG_FATAL_SKIPFIRST_THROTTLE
 * \copydoc RCUTILS_LOG_SKIPFIRST_THROTTLE
 */
# define RCUTILS_LOG_FATAL_SKIPFIRST_THROTTLE(get_time_point, duration, ...) \
  RCUTILS_LOG_SKIPFIRST_THROTTLE(RCUTILS_LOG_SEVERITY_FATAL, get_time_point, duration, __VA_ARGS__)
/**
 * \def RCUTILS_LOG_FATAL_SKIPFIRST_THROTTLE_NAMED
 * \copydoc RCUTILS_LOG_SKIPFIRST_THROTTLE_NAMED
 */
# define RCUTILS_LOG_FATAL_SKIPFIRST_THROTTLE_NAMED(get_time_point, duration, name, ...) \
  RCUTILS_LOG_SKIPFIRST_THROTTLE_NAMED(RCUTILS_LOG_SEVERITY_FATAL, get_time_point, duration, name, \
    __VA_ARGS__)
#endif

#ifdef __cplusplus
}
#endif

#endif  // RCUTILS__LOGGING_MACROS_H_
