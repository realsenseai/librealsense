/* -*- Mode: C; indent-tabs-mode:nil -*- */
/*
 * Unit tests for libusb_set_option
 * Copyright © 2023 Nathan Hjelm <hjelmn@cs.unm.edu>
 * Copyright © 2023 Google, LLC. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "libusbi.h"
#include "libusb_testlib.h"

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <winbase.h>

static int unsetenv(const char *env) {
    return _putenv_s(env, "");
}

static int setenv(const char *env, const char *value, int overwrite) {
    if(getenv(env) && !overwrite)
        return 0;
    return _putenv_s(env, value);
}
#endif

#define LIBUSB_TEST_CLEAN_EXIT(code) \
    do {                             \
        if(test_ctx != NULL) {       \
            libusb_exit(test_ctx);   \
        }                            \
        unsetenv("LIBUSB_DEBUG");    \
        return (code);               \
    } while(0)

/**
 * Fail the test if the expression does not evaluate to LIBUSB_SUCCESS.
 */
#define LIBUSB_TEST_RETURN_ON_ERROR(expr)                                                \
    do {                                                                                 \
        int _result = (expr);                                                            \
        if(LIBUSB_SUCCESS != _result) {                                                  \
            libusb_testlib_logf("Not success (%s) at %s:%d", #expr, __FILE__, __LINE__); \
            LIBUSB_TEST_CLEAN_EXIT(TEST_STATUS_FAILURE);                                 \
        }                                                                                \
    } while(0)

/**
 * Use relational operatator to compare two values and fail the test if the
 * comparison is false. Intended to compare integer or pointer types.
 *
 * Example: LIBUSB_EXPECT(==, 0, 1) -> fail, LIBUSB_EXPECT(==, 0, 0) -> ok.
 */
#define LIBUSB_EXPECT(operator, lhs, rhs)                                                                                                \
    do {                                                                                                                                 \
        int64_t _lhs = (lhs), _rhs = (rhs);                                                                                              \
        if(!(_lhs operator _rhs)) {                                                                                                      \
            libusb_testlib_logf("Expected %s (%" PRId64 ") " #operator" %s (%" PRId64 ") at %s:%d", #lhs, (int64_t)(intptr_t)_lhs, #rhs, \
                                (int64_t)(intptr_t)_rhs, __FILE__, __LINE__);                                                            \
            LIBUSB_TEST_CLEAN_EXIT(TEST_STATUS_FAILURE);                                                                                 \
        }                                                                                                                                \
    } while(0)

static libusb_testlib_result test_set_log_level_basic(void) {
#if defined(ENABLE_LOGGING) && !defined(ENABLE_DEBUG_LOGGING)
    libusb_context *test_ctx = NULL;

    /* unset LIBUSB_DEBUG if it is set */
    unsetenv("LIBUSB_DEBUG");

    /* test basic functionality */
    LIBUSB_TEST_RETURN_ON_ERROR(libusb_init_context(&test_ctx, /*options=*/NULL,
                                                    /*num_options=*/0));
    LIBUSB_TEST_RETURN_ON_ERROR(libusb_set_option(test_ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_ERROR));
    LIBUSB_EXPECT(==, test_ctx->debug, LIBUSB_LOG_LEVEL_ERROR);
    LIBUSB_TEST_RETURN_ON_ERROR(libusb_set_option(test_ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_NONE));
    LIBUSB_EXPECT(==, test_ctx->debug, LIBUSB_LOG_LEVEL_NONE);

    LIBUSB_TEST_CLEAN_EXIT(TEST_STATUS_SUCCESS);
#else
    return TEST_STATUS_SKIP;
#endif
}

static libusb_testlib_result test_set_log_level_default(void) {
#if defined(ENABLE_LOGGING) && !defined(ENABLE_DEBUG_LOGGING)
    libusb_context *test_ctx = NULL;

    /* set the default debug level */
    LIBUSB_TEST_RETURN_ON_ERROR(libusb_set_option(NULL, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_ERROR));

    LIBUSB_TEST_RETURN_ON_ERROR(libusb_init_context(&test_ctx, /*options=*/NULL,
                                                    /*num_options=*/0));
    /* check that debug level came from the default */
    LIBUSB_EXPECT(==, test_ctx->debug, LIBUSB_LOG_LEVEL_ERROR);

    /* try to override the old log level. since this was set from the default it
     * should be possible to change it */
    LIBUSB_TEST_RETURN_ON_ERROR(libusb_set_option(test_ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_NONE));
    LIBUSB_EXPECT(==, test_ctx->debug, LIBUSB_LOG_LEVEL_NONE);

    LIBUSB_TEST_CLEAN_EXIT(TEST_STATUS_SUCCESS);
#else
    return TEST_STATUS_SKIP;
#endif
}

static libusb_testlib_result test_set_log_level_env(void) {
#if defined(ENABLE_LOGGING)
    libusb_context *test_ctx = NULL;

    /* check that libusb_set_option does not change the log level when it was set
     * from the environment. */
    setenv("LIBUSB_DEBUG", "4", /*overwrite=*/0);
    LIBUSB_TEST_RETURN_ON_ERROR(libusb_init_context(&test_ctx, /*options=*/NULL,
                                                    /*num_options=*/0));
    LIBUSB_EXPECT(==, test_ctx->debug, 4);

    LIBUSB_TEST_RETURN_ON_ERROR(libusb_set_option(test_ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_ERROR));
    /* environment variable should always override LIBUSB_OPTION_LOG_LEVEL if set */
    LIBUSB_EXPECT(==, test_ctx->debug, 4);

    LIBUSB_TEST_CLEAN_EXIT(TEST_STATUS_SUCCESS);
#else
    return TEST_STATUS_SKIP;
#endif
}

static libusb_testlib_result test_no_discovery(void) {
#if defined(__linux__)
    libusb_context *test_ctx;
    LIBUSB_TEST_RETURN_ON_ERROR(libusb_init_context(&test_ctx, /*options=*/NULL,
                                                    /*num_options=*/0));
    libusb_device **device_list = NULL;
    ssize_t         num_devices = libusb_get_device_list(test_ctx, &device_list);
    libusb_free_device_list(device_list, /*unref_devices=*/1);
    libusb_exit(test_ctx);

    LIBUSB_EXPECT(>, num_devices, 0);

    LIBUSB_TEST_RETURN_ON_ERROR(libusb_set_option(NULL, LIBUSB_OPTION_NO_DEVICE_DISCOVERY));
    LIBUSB_TEST_RETURN_ON_ERROR(libusb_init_context(&test_ctx, /*options=*/NULL,
                                                    /*num_options=*/0));
    device_list = NULL;
    num_devices = libusb_get_device_list(test_ctx, &device_list);
    libusb_free_device_list(device_list, /*unref_devices=*/1);

    LIBUSB_EXPECT(==, num_devices, 0);
    LIBUSB_TEST_CLEAN_EXIT(TEST_STATUS_SUCCESS);
#else
    return TEST_STATUS_SKIP;
#endif
}

static const libusb_testlib_test tests[] = { { "test_set_log_level_basic", &test_set_log_level_basic },
                                             { "test_set_log_level_env", &test_set_log_level_env },
                                             { "test_no_discovery", &test_no_discovery },
                                             /* since default options can't be unset, run this one last */
                                             { "test_set_log_level_default", &test_set_log_level_default },
                                             LIBUSB_NULL_TEST };

int main(int argc, char *argv[]) {
    return libusb_testlib_run_tests(argc, argv, tests);
}
