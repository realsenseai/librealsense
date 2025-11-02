/*
 * libusb event abstraction on Microsoft Windows
 *
 * Copyright © 2020 Chris Dickens <christopher.a.dickens@gmail.com>
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

#include "libusbi.h"
#include "windows_common.h"

int usbi_create_event(usbi_event_t *event) {
    event->hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if(event->hEvent == NULL) {
        usbi_err(NULL, "CreateEvent failed: %s", windows_error_str(0));
        return LIBUSB_ERROR_OTHER;
    }

    return 0;
}

void usbi_destroy_event(usbi_event_t *event) {
    if(!CloseHandle(event->hEvent))
        usbi_warn(NULL, "CloseHandle failed: %s", windows_error_str(0));
}

void usbi_signal_event(usbi_event_t *event) {
    if(!SetEvent(event->hEvent))
        usbi_warn(NULL, "SetEvent failed: %s", windows_error_str(0));
}

void usbi_clear_event(usbi_event_t *event) {
    if(!ResetEvent(event->hEvent))
        usbi_warn(NULL, "ResetEvent failed: %s", windows_error_str(0));
}

#ifdef HAVE_OS_TIMER
int usbi_create_timer(usbi_timer_t *timer) {
    timer->hTimer = CreateWaitableTimer(NULL, TRUE, NULL);
    if(timer->hTimer == NULL) {
        usbi_warn(NULL, "CreateWaitableTimer failed: %s", windows_error_str(0));
        return LIBUSB_ERROR_OTHER;
    }

    return 0;
}

void usbi_destroy_timer(usbi_timer_t *timer) {
    if(!CloseHandle(timer->hTimer))
        usbi_warn(NULL, "CloseHandle failed: %s", windows_error_str(0));
}

int usbi_arm_timer(usbi_timer_t *timer, const struct timespec *timeout) {
    struct timespec systime, remaining;
    FILETIME        filetime;
    LARGE_INTEGER   dueTime;

    /* Transfer timeouts are based on the monotonic clock and the waitable
     * timers on the system clock. This requires a conversion between the
     * two, so we calculate the remaining time relative to the monotonic
     * clock and calculate an absolute system time for the timer expiration.
     * Note that if the timeout has already passed, the remaining time will
     * be negative and thus an absolute system time in the past will be set.
     * This works just as intended because the timer becomes signalled
     * immediately. */
    usbi_get_monotonic_time(&systime);

    TIMESPEC_SUB(timeout, &systime, &remaining);

    GetSystemTimeAsFileTime(&filetime);
    dueTime.LowPart  = filetime.dwLowDateTime;
    dueTime.HighPart = filetime.dwHighDateTime;
    dueTime.QuadPart += (remaining.tv_sec * 10000000LL) + (remaining.tv_nsec / 100LL);

    if(!SetWaitableTimer(timer->hTimer, &dueTime, 0, NULL, NULL, FALSE)) {
        usbi_warn(NULL, "SetWaitableTimer failed: %s", windows_error_str(0));
        return LIBUSB_ERROR_OTHER;
    }

    return 0;
}

int usbi_disarm_timer(usbi_timer_t *timer) {
    LARGE_INTEGER dueTime;

    /* A manual-reset waitable timer will stay in the signalled state until
     * another call to SetWaitableTimer() is made. It is possible that the
     * timer has already expired by the time we come in to disarm it, so to
     * be entirely sure the timer is disarmed and not in the signalled state,
     * we will set it with an impossibly large expiration and immediately
     * cancel. */
    dueTime.QuadPart = LLONG_MAX;
    if(!SetWaitableTimer(timer->hTimer, &dueTime, 0, NULL, NULL, FALSE)) {
        usbi_warn(NULL, "SetWaitableTimer failed: %s", windows_error_str(0));
        return LIBUSB_ERROR_OTHER;
    }

    if(!CancelWaitableTimer(timer->hTimer)) {
        usbi_warn(NULL, "SetWaitableTimer failed: %s", windows_error_str(0));
        return LIBUSB_ERROR_OTHER;
    }

    return 0;
}
#endif

int usbi_alloc_event_data(struct libusb_context *ctx) {
    struct usbi_event_source *ievent_source;
    HANDLE                   *handles;
    size_t                    i = 0;

    /* Event sources are only added during usbi_io_init(). We should not
     * be running this function again if the event data has already been
     * allocated. */
    if(ctx->event_data) {
        usbi_warn(ctx, "program assertion failed - event data already allocated");
        return LIBUSB_ERROR_OTHER;
    }

    ctx->event_data_cnt = 0;
    for_each_event_source(ctx, ievent_source) ctx->event_data_cnt++;

    /* We only expect up to two HANDLEs to wait on, one for the internal
     * signalling event and the other for the timer. */
    if(ctx->event_data_cnt != 1 && ctx->event_data_cnt != 2) {
        usbi_err(ctx, "program assertion failed - expected exactly 1 or 2 HANDLEs");
        return LIBUSB_ERROR_OTHER;
    }

    handles = calloc(ctx->event_data_cnt, sizeof(HANDLE));
    if(!handles)
        return LIBUSB_ERROR_NO_MEM;

    for_each_event_source(ctx, ievent_source) {
        handles[i] = ievent_source->data.os_handle;
        i++;
    }

    ctx->event_data = handles;
    return 0;
}

int usbi_wait_for_events(struct libusb_context *ctx, struct usbi_reported_events *reported_events, int timeout_ms) {
    HANDLE *handles     = ctx->event_data;
    DWORD   num_handles = (DWORD)ctx->event_data_cnt;
    DWORD   result;

    usbi_dbg(ctx, "WaitForMultipleObjects() for %lu HANDLEs with timeout in %dms", ULONG_CAST(num_handles), timeout_ms);
    result = WaitForMultipleObjects(num_handles, handles, FALSE, (DWORD)timeout_ms);
    usbi_dbg(ctx, "WaitForMultipleObjects() returned %lu", ULONG_CAST(result));
    if(result == WAIT_TIMEOUT) {
        if(usbi_using_timer(ctx))
            goto done;
        return LIBUSB_ERROR_TIMEOUT;
    }
    else if(result == WAIT_FAILED) {
        usbi_err(ctx, "WaitForMultipleObjects() failed: %s", windows_error_str(0));
        return LIBUSB_ERROR_IO;
    }

    result -= WAIT_OBJECT_0;

    /* handles[0] is always the internal signalling event */
    if(result == 0)
        reported_events->event_triggered = 1;
    else
        reported_events->event_triggered = 0;

#ifdef HAVE_OS_TIMER
    /* on timer configurations, handles[1] is the timer */
    if(usbi_using_timer(ctx)) {
        /* The WaitForMultipleObjects() function reports the index of
         * the first object that became signalled. If the internal
         * signalling event was reported, we need to also check and
         * report whether the timer is in the signalled state. */
        if(result == 1 || WaitForSingleObject(handles[1], 0) == WAIT_OBJECT_0)
            reported_events->timer_triggered = 1;
        else
            reported_events->timer_triggered = 0;
    }
    else {
        reported_events->timer_triggered = 0;
    }
#endif

done:
    /* no events are ever reported to the backend */
    reported_events->num_ready = 0;
    return LIBUSB_SUCCESS;
}
