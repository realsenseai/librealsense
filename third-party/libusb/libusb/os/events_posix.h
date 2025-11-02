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

#ifndef LIBUSB_EVENTS_POSIX_H
#define LIBUSB_EVENTS_POSIX_H

#include <poll.h>

typedef int usbi_os_handle_t;
#define USBI_OS_HANDLE_FORMAT_STRING	"fd %d"

#ifdef HAVE_EVENTFD
typedef struct usbi_event {
	int eventfd;
} usbi_event_t;
#define USBI_EVENT_OS_HANDLE(e)	((e)->eventfd)
#define USBI_EVENT_POLL_EVENTS	POLLIN
#define USBI_INVALID_EVENT	{ -1 }
#else
typedef struct usbi_event {
	int pipefd[2];
} usbi_event_t;
#define USBI_EVENT_OS_HANDLE(e)	((e)->pipefd[0])
#define USBI_EVENT_POLL_EVENTS	POLLIN
#define USBI_INVALID_EVENT	{ { -1, -1 } }
#endif

#ifdef HAVE_TIMERFD
#define HAVE_OS_TIMER 1
typedef struct usbi_timer {
	int timerfd;
} usbi_timer_t;
#define USBI_TIMER_OS_HANDLE(t)	((t)->timerfd)
#define USBI_TIMER_POLL_EVENTS	POLLIN

static inline int usbi_timer_valid(usbi_timer_t *timer)
{
	return timer->timerfd >= 0;
}
#endif

#endif
