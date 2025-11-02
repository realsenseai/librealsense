/*
 * libusb test library helper functions
 * Copyright © 2012 Toby Gray <toby.gray@realvnc.com>
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

#include <libusb/config.h>

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "libusb_testlib.h"

#if defined(PLATFORM_POSIX)
#define NULL_PATH "/dev/null"
#elif defined(PLATFORM_WINDOWS)
#define NULL_PATH "nul"
#endif

/**
 * Converts a test result code into a human readable string.
 */
static const char *test_result_to_str(libusb_testlib_result result) {
    switch(result) {
    case TEST_STATUS_SUCCESS:
        return "Success";
    case TEST_STATUS_FAILURE:
        return "Failure";
    case TEST_STATUS_ERROR:
        return "Error";
    case TEST_STATUS_SKIP:
        return "Skip";
    default:
        return "Unknown";
    }
}

static void print_usage(const char *progname) {
    printf("Usage: %s [-l] [-v] [<test_name> ...]\n", progname);
    printf("   -l   List available tests\n");
    printf("   -v   Don't redirect STDERR before running tests\n");
    printf("   -h   Display this help and exit\n");
}

void libusb_testlib_logf(const char *fmt, ...) {
    va_list va;

    va_start(va, fmt);
    vfprintf(stdout, fmt, va);
    va_end(va);
    fputc('\n', stdout);
    fflush(stdout);
}

int libusb_testlib_run_tests(int argc, char *argv[], const libusb_testlib_test *tests) {
    int run_count   = 0;
    int idx         = 0;
    int pass_count  = 0;
    int fail_count  = 0;
    int error_count = 0;
    int skip_count  = 0;

    /* Setup default mode of operation */
    char **test_names = NULL;
    int    test_count = 0;
    bool   list_tests = false;
    bool   verbose    = false;

    /* Parse command line options */
    if(argc >= 2) {
        for(int j = 1; j < argc; j++) {
            const char *argstr = argv[j];
            size_t      arglen = strlen(argstr);

            if(argstr[0] == '-' || argstr[0] == '/') {
                if(arglen == 2) {
                    switch(argstr[1]) {
                    case 'l':
                        list_tests = true;
                        continue;
                    case 'v':
                        verbose = true;
                        continue;
                    case 'h':
                        print_usage(argv[0]);
                        return 0;
                    }
                }

                fprintf(stderr, "Unknown option: '%s'\n", argstr);
                print_usage(argv[0]);
                return 1;
            }
            else {
                /* End of command line options, remaining must be list of tests to run */
                test_names = argv + j;
                test_count = argc - j;
                break;
            }
        }
    }

    /* Validate command line options */
    if(test_names && list_tests) {
        fprintf(stderr, "List of tests requested but test list provided\n");
        print_usage(argv[0]);
        return 1;
    }

    /* Setup test log output */
    if(!verbose) {
        if(!freopen(NULL_PATH, "w", stderr)) {
            printf("Failed to open null handle: %d\n", errno);
            return 1;
        }
    }

    /* Act on any options not related to running tests */
    if(list_tests) {
        while(tests[idx].function)
            libusb_testlib_logf("%s", tests[idx++].name);
        return 0;
    }

    /* Run any requested tests */
    while(tests[idx].function) {
        const libusb_testlib_test *test = &tests[idx++];
        libusb_testlib_result      test_result;

        if(test_count > 0) {
            /* Filtering tests to run, check if this is one of them */
            int i;

            for(i = 0; i < test_count; i++) {
                if(!strcmp(test_names[i], test->name))
                    /* Matches a requested test name */
                    break;
            }
            if(i == test_count) {
                /* Failed to find a test match, so do the next loop iteration */
                continue;
            }
        }
        libusb_testlib_logf("Starting test run: %s...", test->name);
        test_result = test->function();
        libusb_testlib_logf("%s (%d)", test_result_to_str(test_result), test_result);
        switch(test_result) {
        case TEST_STATUS_SUCCESS:
            pass_count++;
            break;
        case TEST_STATUS_FAILURE:
            fail_count++;
            break;
        case TEST_STATUS_ERROR:
            error_count++;
            break;
        case TEST_STATUS_SKIP:
            skip_count++;
            break;
        }
        run_count++;
    }

    libusb_testlib_logf("---");
    libusb_testlib_logf("Ran %d tests", run_count);
    libusb_testlib_logf("Passed %d tests", pass_count);
    libusb_testlib_logf("Failed %d tests", fail_count);
    libusb_testlib_logf("Error in %d tests", error_count);
    libusb_testlib_logf("Skipped %d tests", skip_count);

    return pass_count != run_count;
}
