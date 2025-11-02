/*
 * libusb multi-thread test program
 * Copyright 2022-2023 Tormod Volden
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <libusb/config.h>

#include <libusb.h>
#include <stdio.h>

#if defined(PLATFORM_POSIX)

#include <pthread.h>
typedef pthread_t thread_t;
typedef void     *thread_return_t;
#define THREAD_RETURN_VALUE NULL
#define THREAD_CALL_TYPE

static inline int thread_create(thread_t *thread, thread_return_t (*thread_entry)(void *arg), void *arg) {
    return pthread_create(thread, NULL, thread_entry, arg) == 0 ? 0 : -1;
}

static inline void thread_join(thread_t thread) {
    (void)pthread_join(thread, NULL);
}

#elif defined(PLATFORM_WINDOWS)

typedef HANDLE thread_t;
#define THREAD_RETURN_VALUE 0
#define THREAD_CALL_TYPE __stdcall

#if defined(__CYGWIN__)
typedef DWORD  thread_return_t;
#else
#include <process.h>
typedef unsigned thread_return_t;
#endif

static inline int thread_create(thread_t *thread, thread_return_t(__stdcall *thread_entry)(void *arg), void *arg) {
#if defined(__CYGWIN__)
    *thread = CreateThread(NULL, 0, thread_entry, arg, 0, NULL);
#else
    *thread = (HANDLE)_beginthreadex(NULL, 0, thread_entry, arg, 0, NULL);
#endif
    return *thread != NULL ? 0 : -1;
}

static inline void thread_join(thread_t thread) {
    (void)WaitForSingleObject(thread, INFINITE);
    (void)CloseHandle(thread);
}
#endif /* PLATFORM_WINDOWS */

/* Test that creates and destroys contexts repeatedly */

#define NTHREADS 8
#define ITERS 64

struct thread_info {
    int     number;
    int     enumerate;
    ssize_t devcount;
    int     err;
    int     iteration;
} tinfo[NTHREADS];

static thread_return_t THREAD_CALL_TYPE init_and_exit(void *arg) {
    struct thread_info *ti = (struct thread_info *)arg;

    for(int i = 0; i < ITERS; ++i) {
        libusb_context *ctx = NULL;
        int             r;

        r = libusb_init_context(&ctx, /*options=*/NULL, /*num_options=*/0);
        if(r != LIBUSB_SUCCESS) {
            ti->err       = r;
            ti->iteration = i;
            return (thread_return_t)THREAD_RETURN_VALUE;
        }
        if(ti->enumerate) {
            libusb_device **devs;
            ti->devcount = libusb_get_device_list(ctx, &devs);
            if(ti->devcount < 0) {
                libusb_free_device_list(devs, 1);
                ti->iteration = i;
                break;
            }
            libusb_free_device_list(devs, 1);
        }

        libusb_exit(ctx);
    }
    return (thread_return_t)THREAD_RETURN_VALUE;
}

static int test_multi_init(int enumerate) {
    thread_t threadId[NTHREADS];
    int      errs = 0;
    int      t;

    printf("Starting %d threads\n", NTHREADS);
    for(t = 0; t < NTHREADS; t++) {
        tinfo[t].number    = t;
        tinfo[t].enumerate = enumerate;
        thread_create(&threadId[t], &init_and_exit, (void *)&tinfo[t]);
    }

    for(t = 0; t < NTHREADS; t++) {
        thread_join(threadId[t]);
        if(tinfo[t].err) {
            errs++;
            fprintf(stderr, "Thread %d failed (iteration %d): %s\n", tinfo[t].number, tinfo[t].iteration, libusb_error_name(tinfo[t].err));
        }
        else if(enumerate) {
            if(tinfo[t].devcount < 0) {
                errs++;
                fprintf(stderr, "Thread %d failed to enumerate devices (iteration %d)\n", tinfo[t].number, tinfo[t].iteration);
            }
            else {
                printf("Thread %d discovered %ld devices\n", tinfo[t].number, (long int)tinfo[t].devcount);
            }
        }
    }

    return errs;
}

int main(void) {
    int errs = 0;

    printf("Running multithreaded init/exit test...\n");
    errs += test_multi_init(0);
    printf("Running multithreaded init/exit test with enumeration...\n");
    errs += test_multi_init(1);
    printf("All done, %d errors\n", errs);

    return errs != 0;
}
