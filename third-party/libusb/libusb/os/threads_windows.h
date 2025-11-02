/*
 * libusb synchronization on Microsoft Windows
 *
 * Copyright © 2010 Michael Plante <michael.plante@gmail.com>
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

#ifndef LIBUSB_THREADS_WINDOWS_H
#define LIBUSB_THREADS_WINDOWS_H

#define WINAPI_CHECK(expression)	ASSERT_NE(expression, 0)

#define USBI_MUTEX_INITIALIZER	0L
typedef LONG usbi_mutex_static_t;
static inline void usbi_mutex_static_lock(usbi_mutex_static_t *mutex)
{
	while (InterlockedExchange(mutex, 1L) == 1L)
		SleepEx(0, TRUE);
}
static inline void usbi_mutex_static_unlock(usbi_mutex_static_t *mutex)
{
	InterlockedExchange(mutex, 0L);
}

typedef CRITICAL_SECTION usbi_mutex_t;
static inline void usbi_mutex_init(usbi_mutex_t *mutex)
{
	InitializeCriticalSection(mutex);
}
static inline void usbi_mutex_lock(usbi_mutex_t *mutex)
{
	EnterCriticalSection(mutex);
}
static inline void usbi_mutex_unlock(usbi_mutex_t *mutex)
{
	LeaveCriticalSection(mutex);
}
static inline int usbi_mutex_trylock(usbi_mutex_t *mutex)
{
	return TryEnterCriticalSection(mutex) != 0;
}
static inline void usbi_mutex_destroy(usbi_mutex_t *mutex)
{
	DeleteCriticalSection(mutex);
}

#if !defined(HAVE_STRUCT_TIMESPEC) && !defined(_TIMESPEC_DEFINED)
#define HAVE_STRUCT_TIMESPEC 1
#define _TIMESPEC_DEFINED 1
struct timespec {
	long tv_sec;
	long tv_nsec;
};
#endif /* HAVE_STRUCT_TIMESPEC || _TIMESPEC_DEFINED */

typedef CONDITION_VARIABLE usbi_cond_t;
static inline void usbi_cond_init(usbi_cond_t *cond)
{
	InitializeConditionVariable(cond);
}
static inline void usbi_cond_wait(usbi_cond_t *cond, usbi_mutex_t *mutex)
{
	WINAPI_CHECK(SleepConditionVariableCS(cond, mutex, INFINITE));
}
int usbi_cond_timedwait(usbi_cond_t *cond,
	usbi_mutex_t *mutex, const struct timeval *tv);
static inline void usbi_cond_broadcast(usbi_cond_t *cond)
{
	WakeAllConditionVariable(cond);
}
static inline void usbi_cond_destroy(usbi_cond_t *cond)
{
	UNUSED(cond);
}

typedef DWORD usbi_tls_key_t;
static inline void usbi_tls_key_create(usbi_tls_key_t *key)
{
	*key = TlsAlloc();
	assert(*key != TLS_OUT_OF_INDEXES);
}
static inline void *usbi_tls_key_get(usbi_tls_key_t key)
{
	return TlsGetValue(key);
}
static inline void usbi_tls_key_set(usbi_tls_key_t key, void *ptr)
{
	WINAPI_CHECK(TlsSetValue(key, ptr));
}
static inline void usbi_tls_key_delete(usbi_tls_key_t key)
{
	WINAPI_CHECK(TlsFree(key));
}

static inline unsigned int usbi_get_tid(void)
{
	return (unsigned int)GetCurrentThreadId();
}

#endif /* LIBUSB_THREADS_WINDOWS_H */
