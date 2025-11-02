/*
 * libusb event abstraction on POSIX platforms
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

#include "libusbi.h"

#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_EVENTFD
#include <sys/eventfd.h>
#endif
#ifdef HAVE_TIMERFD
#include <sys/timerfd.h>
#endif

#ifdef __EMSCRIPTEN__
/* On Emscripten `pipe` does not conform to the spec and does not block
 * until events are available, which makes it unusable for event system
 * and often results in deadlocks when `pipe` is in a loop like it is
 * in libusb.
 *
 * Therefore use a custom event system based on browser event emitters. */
#include <emscripten.h>

EM_JS(void, em_libusb_notify, (void), { dispatchEvent(new Event("em-libusb")); });

EM_ASYNC_JS(int, em_libusb_wait, (int timeout), {
    let onEvent, timeoutId;

    try {
        return await new Promise(resolve = > {
            onEvent = () = > resolve(0);
            addEventListener('em-libusb', onEvent);

            timeoutId = setTimeout(resolve, timeout, -1);
        });
    }
    finally {
        removeEventListener('em-libusb', onEvent);
        clearTimeout(timeoutId);
    }
});
#endif
#include <unistd.h>

#ifdef HAVE_EVENTFD
#define EVENT_READ_FD(e) ((e)->eventfd)
#define EVENT_WRITE_FD(e) ((e)->eventfd)
#else
#define EVENT_READ_FD(e) ((e)->pipefd[0])
#define EVENT_WRITE_FD(e) ((e)->pipefd[1])
#endif

#ifdef HAVE_NFDS_T
typedef nfds_t usbi_nfds_t;
#else
typedef unsigned int usbi_nfds_t;
#endif

int usbi_create_event(usbi_event_t *event) {
#ifdef HAVE_EVENTFD
    event->eventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if(event->eventfd == -1) {
        usbi_err(NULL, "failed to create eventfd, errno=%d", errno);
        return LIBUSB_ERROR_OTHER;
    }

    return 0;
#else
#if defined(HAVE_PIPE2)
    int ret = pipe2(event->pipefd, O_CLOEXEC);
#else
    int ret = pipe(event->pipefd);
#endif

    if(ret != 0) {
        usbi_err(NULL, "failed to create pipe, errno=%d", errno);
        return LIBUSB_ERROR_OTHER;
    }

#if !defined(HAVE_PIPE2) && defined(FD_CLOEXEC)
    ret = fcntl(event->pipefd[0], F_GETFD);
    if(ret == -1) {
        usbi_err(NULL, "failed to get pipe fd flags, errno=%d", errno);
        goto err_close_pipe;
    }
    ret = fcntl(event->pipefd[0], F_SETFD, ret | FD_CLOEXEC);
    if(ret == -1) {
        usbi_err(NULL, "failed to set pipe fd flags, errno=%d", errno);
        goto err_close_pipe;
    }

    ret = fcntl(event->pipefd[1], F_GETFD);
    if(ret == -1) {
        usbi_err(NULL, "failed to get pipe fd flags, errno=%d", errno);
        goto err_close_pipe;
    }
    ret = fcntl(event->pipefd[1], F_SETFD, ret | FD_CLOEXEC);
    if(ret == -1) {
        usbi_err(NULL, "failed to set pipe fd flags, errno=%d", errno);
        goto err_close_pipe;
    }
#endif

    ret = fcntl(event->pipefd[1], F_GETFL);
    if(ret == -1) {
        usbi_err(NULL, "failed to get pipe fd status flags, errno=%d", errno);
        goto err_close_pipe;
    }
    ret = fcntl(event->pipefd[1], F_SETFL, ret | O_NONBLOCK);
    if(ret == -1) {
        usbi_err(NULL, "failed to set pipe fd status flags, errno=%d", errno);
        goto err_close_pipe;
    }

    return 0;

err_close_pipe:
    close(event->pipefd[1]);
    close(event->pipefd[0]);
    return LIBUSB_ERROR_OTHER;
#endif
}

void usbi_destroy_event(usbi_event_t *event) {
#ifdef HAVE_EVENTFD
    if(close(event->eventfd) == -1)
        usbi_warn(NULL, "failed to close eventfd, errno=%d", errno);
#else
    if(close(event->pipefd[1]) == -1)
        usbi_warn(NULL, "failed to close pipe write end, errno=%d", errno);
    if(close(event->pipefd[0]) == -1)
        usbi_warn(NULL, "failed to close pipe read end, errno=%d", errno);
#endif
}

void usbi_signal_event(usbi_event_t *event) {
    uint64_t dummy = 1;
    ssize_t  r;

    r = write(EVENT_WRITE_FD(event), &dummy, sizeof(dummy));
    if(r != sizeof(dummy))
        usbi_warn(NULL, "event write failed");
#ifdef __EMSCRIPTEN__
    em_libusb_notify();
#endif
}

void usbi_clear_event(usbi_event_t *event) {
    uint64_t dummy;
    ssize_t  r;

    r = read(EVENT_READ_FD(event), &dummy, sizeof(dummy));
    if(r != sizeof(dummy))
        usbi_warn(NULL, "event read failed");
}

#ifdef HAVE_TIMERFD
int usbi_create_timer(usbi_timer_t *timer) {
    timer->timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if(timer->timerfd == -1) {
        usbi_warn(NULL, "failed to create timerfd, errno=%d", errno);
        return LIBUSB_ERROR_OTHER;
    }

    return 0;
}

void usbi_destroy_timer(usbi_timer_t *timer) {
    if(close(timer->timerfd) == -1)
        usbi_warn(NULL, "failed to close timerfd, errno=%d", errno);
}

int usbi_arm_timer(usbi_timer_t *timer, const struct timespec *timeout) {
    const struct itimerspec it = { { 0, 0 }, { timeout->tv_sec, timeout->tv_nsec } };

    if(timerfd_settime(timer->timerfd, TFD_TIMER_ABSTIME, &it, NULL) == -1) {
        usbi_warn(NULL, "failed to arm timerfd, errno=%d", errno);
        return LIBUSB_ERROR_OTHER;
    }

    return 0;
}

int usbi_disarm_timer(usbi_timer_t *timer) {
    const struct itimerspec it = { { 0, 0 }, { 0, 0 } };

    if(timerfd_settime(timer->timerfd, 0, &it, NULL) == -1) {
        usbi_warn(NULL, "failed to disarm timerfd, errno=%d", errno);
        return LIBUSB_ERROR_OTHER;
    }

    return 0;
}
#endif

int usbi_alloc_event_data(struct libusb_context *ctx) {
    struct usbi_event_source *ievent_source;
    struct pollfd            *fds;
    size_t                    i = 0;

    if(ctx->event_data) {
        free(ctx->event_data);
        ctx->event_data = NULL;
    }

    ctx->event_data_cnt = 0;
    for_each_event_source(ctx, ievent_source) ctx->event_data_cnt++;

    fds = calloc(ctx->event_data_cnt, sizeof(*fds));
    if(!fds)
        return LIBUSB_ERROR_NO_MEM;

    for_each_event_source(ctx, ievent_source) {
        fds[i].fd     = ievent_source->data.os_handle;
        fds[i].events = ievent_source->data.poll_events;
        i++;
    }

    ctx->event_data = fds;
    return 0;
}

int usbi_wait_for_events(struct libusb_context *ctx, struct usbi_reported_events *reported_events, int timeout_ms) {
    struct pollfd *fds  = ctx->event_data;
    usbi_nfds_t    nfds = (usbi_nfds_t)ctx->event_data_cnt;
    int            internal_fds, num_ready;

    usbi_dbg(ctx, "poll() %u fds with timeout in %dms", (unsigned int)nfds, timeout_ms);
#ifdef __EMSCRIPTEN__
    /* TODO: improve event system to watch only for fd events we're interested in
     * (although a scenario where we have multiple watchers in parallel is very rare
     * in real world anyway). */
    double until_time = emscripten_get_now() + timeout_ms;
    for(;;) {
        /* Emscripten `poll` ignores timeout param, but pass 0 explicitly just in case. */
        num_ready = poll(fds, nfds, 0);
        if(num_ready != 0)
            break;
        int timeout = until_time - emscripten_get_now();
        if(timeout <= 0)
            break;
        int result = em_libusb_wait(timeout);
        if(result != 0)
            break;
    }
#else
    num_ready = poll(fds, nfds, timeout_ms);
#endif
    usbi_dbg(ctx, "poll() returned %d", num_ready);
    if(num_ready == 0) {
        if(usbi_using_timer(ctx))
            goto done;
        return LIBUSB_ERROR_TIMEOUT;
    }
    else if(num_ready == -1) {
        if(errno == EINTR)
            return LIBUSB_ERROR_INTERRUPTED;
        usbi_err(ctx, "poll() failed, errno=%d", errno);
        return LIBUSB_ERROR_IO;
    }

    /* fds[0] is always the internal signalling event */
    if(fds[0].revents) {
        reported_events->event_triggered = 1;
        num_ready--;
    }
    else {
        reported_events->event_triggered = 0;
    }

#ifdef HAVE_OS_TIMER
    /* on timer configurations, fds[1] is the timer */
    if(usbi_using_timer(ctx) && fds[1].revents) {
        reported_events->timer_triggered = 1;
        num_ready--;
    }
    else {
        reported_events->timer_triggered = 0;
    }
#endif

    if(!num_ready)
        goto done;

    /* the backend will never need to attempt to handle events on the
     * library's internal file descriptors, so we determine how many are
     * in use internally for this context and skip these when passing any
     * remaining pollfds to the backend. */
    internal_fds = usbi_using_timer(ctx) ? 2 : 1;
    fds += internal_fds;
    nfds -= internal_fds;

    usbi_mutex_lock(&ctx->event_data_lock);
    if(ctx->event_flags & USBI_EVENT_EVENT_SOURCES_MODIFIED) {
        struct usbi_event_source *ievent_source;

        for_each_removed_event_source(ctx, ievent_source) {
            usbi_nfds_t n;

            for(n = 0; n < nfds; n++) {
                if(ievent_source->data.os_handle != fds[n].fd)
                    continue;
                if(!fds[n].revents)
                    continue;
                /* pollfd was removed between the creation of the fds array and
                 * here. remove triggered revent as it is no longer relevant. */
                usbi_dbg(ctx, "fd %d was removed, ignoring raised events", fds[n].fd);
                fds[n].revents = 0;
                num_ready--;
                break;
            }
        }
    }
    usbi_mutex_unlock(&ctx->event_data_lock);

    if(num_ready) {
        assert(num_ready > 0);
        reported_events->event_data       = fds;
        reported_events->event_data_count = (unsigned int)nfds;
    }

done:
    reported_events->num_ready = num_ready;
    return LIBUSB_SUCCESS;
}
