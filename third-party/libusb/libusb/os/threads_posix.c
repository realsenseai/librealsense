/*
 * libusb synchronization using POSIX Threads
 *
 * Copyright © 2011 Vitali Lovich <vlovich@aliph.com>
 * Copyright © 2011 Peter Stuge <peter@stuge.se>
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

#include "libusbi.h"

#include <errno.h>
#if defined(__ANDROID__)
#include <unistd.h>
#elif defined(__HAIKU__)
#include <os/kernel/OS.h>
#elif defined(__linux__)
#include <sys/syscall.h>
#include <unistd.h>
#elif defined(__NetBSD__)
#include <lwp.h>
#elif defined(__OpenBSD__)
#include <unistd.h>
#elif defined(__sun__)
#include <sys/lwp.h>
#endif

void usbi_cond_init(pthread_cond_t *cond) {
#ifdef HAVE_PTHREAD_CONDATTR_SETCLOCK
    pthread_condattr_t condattr;

    PTHREAD_CHECK(pthread_condattr_init(&condattr));
    PTHREAD_CHECK(pthread_condattr_setclock(&condattr, CLOCK_MONOTONIC));
    PTHREAD_CHECK(pthread_cond_init(cond, &condattr));
    PTHREAD_CHECK(pthread_condattr_destroy(&condattr));
#else
    PTHREAD_CHECK(pthread_cond_init(cond, NULL));
#endif
}

int usbi_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timeval *tv) {
    struct timespec timeout;
    int             r;

#ifdef HAVE_PTHREAD_CONDATTR_SETCLOCK
    usbi_get_monotonic_time(&timeout);
#else
    usbi_get_real_time(&timeout);
#endif

    timeout.tv_sec += tv->tv_sec;
    timeout.tv_nsec += tv->tv_usec * 1000L;
    if(timeout.tv_nsec >= NSEC_PER_SEC) {
        timeout.tv_nsec -= NSEC_PER_SEC;
        timeout.tv_sec++;
    }

    r = pthread_cond_timedwait(cond, mutex, &timeout);
    if(r == 0)
        return 0;
    else if(r == ETIMEDOUT)
        return LIBUSB_ERROR_TIMEOUT;
    else
        return LIBUSB_ERROR_OTHER;
}

unsigned int usbi_get_tid(void) {
    static _Thread_local unsigned int tl_tid;
    int                               tid;

    if(tl_tid)
        return tl_tid;

#if defined(__ANDROID__)
    tid = gettid();
#elif defined(__APPLE__)
#ifdef HAVE_PTHREAD_THREADID_NP
    uint64_t thread_id;

    if(pthread_threadid_np(NULL, &thread_id) == 0)
        tid = (int)thread_id;
    else
        tid = -1;
#else
    tid = (int)pthread_mach_thread_np(pthread_self());
#endif
#elif defined(__HAIKU__)
    tid = get_pthread_thread_id(pthread_self());
#elif defined(__linux__)
    tid = (int)syscall(SYS_gettid);
#elif defined(__NetBSD__)
    tid = _lwp_self();
#elif defined(__OpenBSD__)
    tid = getthrid();
#elif defined(__sun__)
    tid = _lwp_self();
#else
    tid = -1;
#endif

    if(tid == -1) {
        /* If we don't have a thread ID, at least return a unique
         * value that can be used to distinguish individual
         * threads. */
        tid = (int)(intptr_t)pthread_self();
    }

    return tl_tid = (unsigned int)tid;
}
