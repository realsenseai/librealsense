// Copyright 2015 Open Source Robotics Foundation, Inc.
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

/*
 * An implementation of C11 stdatomic.h for Win32, part borrowed from FreeBSD
 * (original copyright follows), with major modifications for
 * portability to Win32 systems.
 *
 * Caveats and limitations:
 * - Only the ``_Atomic parentheses'' notation is implemented, while
 *   the ``_Atomic space'' one is not.
 * - _Atomic types must be typedef'ed, or programs using them will
 *   not type check correctly (incompatible anonymous structure
 *   types).
 * - Support is limited to 16, 32, and 64 bit types only.
 * - Stripped down to only the functions used in rcutils and dependents like rcl.
 */

/*-
 * Copyright (c) 2011 Ed Schouten <ed@FreeBSD.org>
 *                    David Chisnall <theraven@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/include/stdatomic.h,v 1.10.2.2 2012/05/30 19:21:54 theraven Exp $
 */

#if !defined(_WIN32)
#error "this stdatomic.h does not support your compiler"
#endif  // !defined(_WIN32)

#ifndef RCUTILS__STDATOMIC_HELPER__WIN32__STDATOMIC_H_
#define RCUTILS__STDATOMIC_HELPER__WIN32__STDATOMIC_H_

// When building with MSVC 19.28.29333.0 on Windows 10 (as of 2020-11-11),
// there appears to be a problem with winbase.h (which is included by
// Windows.h).  In particular, warnings of the form:
//
// warning C5105: macro expansion producing 'defined' has undefined behavior
//
// See https://developercommunity.visualstudio.com/content/problem/695656/wdk-and-sdk-are-not-compatible-with-experimentalpr.html
// for more information.  For now disable that warning when including windows.h

#pragma warning(push)
#pragma warning(disable : 5105)
#include <Windows.h>
#pragma warning(pop)

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <rcutils/logging_macros.h>

// In MSVC, correct alignment of each type is already ensured.
#define _Atomic(T) struct { T __val; }

/*
 * 7.17.2 Initialization.
 */

#define ATOMIC_VAR_INIT(value) {.__val = (value)}
#define atomic_init(obj, value) do { \
    (obj)->__val = (value); \
} while (0)

/*
 * Clang and recent GCC both provide predefined macros for the memory
 * orderings.  If we are using a compiler that doesn't define them, use the
 * clang values - these will be ignored in the fallback path.
 */

#ifndef __ATOMIC_RELAXED
#define __ATOMIC_RELAXED 0
#endif
#ifndef __ATOMIC_CONSUME
#define __ATOMIC_CONSUME 1
#endif
#ifndef __ATOMIC_ACQUIRE
#define __ATOMIC_ACQUIRE 2
#endif
#ifndef __ATOMIC_RELEASE
#define __ATOMIC_RELEASE 3
#endif
#ifndef __ATOMIC_ACQ_REL
#define __ATOMIC_ACQ_REL 4
#endif
#ifndef __ATOMIC_SEQ_CST
#define __ATOMIC_SEQ_CST 5
#endif

/*
 * 7.17.3 Order and consistency.
 *
 * The memory_order_* constants that denote the barrier behaviour of the
 * atomic operations.
 */

enum memory_order
{
  memory_order_relaxed = __ATOMIC_RELAXED,
  memory_order_consume = __ATOMIC_CONSUME,
  memory_order_acquire = __ATOMIC_ACQUIRE,
  memory_order_release = __ATOMIC_RELEASE,
  memory_order_acq_rel = __ATOMIC_ACQ_REL,
  memory_order_seq_cst = __ATOMIC_SEQ_CST
};

typedef enum memory_order memory_order;

/*
 * 7.17.4 Fences.
 */

#define atomic_thread_fence(order)  MemoryBarrier()
#define atomic_signal_fence(order)  _ReadWriteBarrier()

/*
 * 7.17.5 Lock-free property.
 */

#define atomic_is_lock_free(obj) (sizeof((obj)->__val) <= sizeof(void *))

/*
 * 7.17.6 Atomic integer types.
 */

typedef _Atomic (_Bool) atomic_bool;
typedef _Atomic (char) atomic_char;
typedef _Atomic (signed char) atomic_schar;
typedef _Atomic (unsigned char) atomic_uchar;
typedef _Atomic (short) atomic_short;  // NOLINT
typedef _Atomic (unsigned short) atomic_ushort;  // NOLINT
typedef _Atomic (int) atomic_int;
typedef _Atomic (unsigned int) atomic_uint;
typedef _Atomic (long) atomic_long;  // NOLINT
typedef _Atomic (unsigned long) atomic_ulong;  // NOLINT
typedef _Atomic (long long) atomic_llong;  // NOLINT
typedef _Atomic (unsigned long long) atomic_ullong;  // NOLINT
#if 0
typedef _Atomic (char16_t) atomic_char16_t;
typedef _Atomic (char32_t) atomic_char32_t;
typedef _Atomic (wchar_t) atomic_wchar_t;
typedef _Atomic (int_least8_t) atomic_int_least8_t;
typedef _Atomic (uint_least8_t) atomic_uint_least8_t;
#endif
typedef _Atomic (int_least16_t) atomic_int_least16_t;
typedef _Atomic (uint_least16_t) atomic_uint_least16_t;
typedef _Atomic (int_least32_t) atomic_int_least32_t;
typedef _Atomic (uint_least32_t) atomic_uint_least32_t;
typedef _Atomic (int_least64_t) atomic_int_least64_t;
typedef _Atomic (uint_least64_t) atomic_uint_least64_t;
#if 0
typedef _Atomic (int_fast8_t) atomic_int_fast8_t;
typedef _Atomic (uint_fast8_t) atomic_uint_fast8_t;
#endif
typedef _Atomic (int_fast16_t) atomic_int_fast16_t;
typedef _Atomic (uint_fast16_t) atomic_uint_fast16_t;
typedef _Atomic (int_fast32_t) atomic_int_fast32_t;
typedef _Atomic (uint_fast32_t) atomic_uint_fast32_t;
typedef _Atomic (int_fast64_t) atomic_int_fast64_t;
typedef _Atomic (uint_fast64_t) atomic_uint_fast64_t;
typedef _Atomic (intptr_t) atomic_intptr_t;
typedef _Atomic (uintptr_t) atomic_uintptr_t;
typedef _Atomic (size_t) atomic_size_t;
typedef _Atomic (ptrdiff_t) atomic_ptrdiff_t;
typedef _Atomic (intmax_t) atomic_intmax_t;
typedef _Atomic (uintmax_t) atomic_uintmax_t;

#ifdef ROS_PACKAGE_NAME
  #define _RCUTILS_PACKAGE_NAME ROS_PACKAGE_NAME
#else
  #define _RCUTILS_PACKAGE_NAME "<Unknown Package>"
#endif

/*
 * 7.17.7 Operations on atomic types. (pruned modified for Windows' crappy C compiler)
 */

// TODO(emersonknapp) Regression in uncrustify breaks formatting for macros with __pragma
// remove indent-off when we have fix for https://github.com/uncrustify/uncrustify/issues/2314
// *INDENT-OFF*

#define rcutils_win32_atomic_compare_exchange_strong(object, out, expected, desired) \
  __pragma(warning(push)) \
  __pragma(warning(disable: 4244)) \
  __pragma(warning(disable: 4047)) \
  __pragma(warning(disable: 4024)) \
  do { \
    switch (sizeof(out)) { \
      case sizeof(uint64_t): \
        out = InterlockedCompareExchange64((LONGLONG *) object, desired, *expected); \
        break; \
      case sizeof(uint32_t): \
        out = _InterlockedCompareExchange((LONG *) object, desired, *expected); \
        break; \
      case sizeof(uint16_t): \
        out = _InterlockedCompareExchange16((SHORT *) object, desired, *expected); \
        break; \
      case sizeof(uint8_t): \
        out = _InterlockedCompareExchange8((char *) object, desired, *expected); \
        break; \
      default: \
        RCUTILS_LOG_ERROR_NAMED( \
          _RCUTILS_PACKAGE_NAME, "Unsupported integer type in atomic_compare_exchange_strong"); \
        exit(-1); \
        break; \
    } \
  } while (0); \
  __pragma(warning(pop))

#define rcutils_win32_atomic_compare_exchange_weak(object, out, expected, desired) \
  rcutils_win32_atomic_compare_exchange_strong(object, out, expected, desired)

#define rcutils_win32_atomic_exchange(object, out, desired) \
  __pragma(warning(push)) \
  __pragma(warning(disable: 4244)) \
  __pragma(warning(disable: 4047)) \
  __pragma(warning(disable: 4024)) \
  do { \
    switch (sizeof(out)) { \
      case sizeof(uint64_t): \
        out = InterlockedExchange64((LONGLONG *) object, desired); \
        break; \
      case sizeof(uint32_t): \
        out = _InterlockedExchange((LONG *) object, desired); \
        break; \
      case sizeof(uint16_t): \
        out = _InterlockedExchange16((SHORT *) object, desired); \
        break; \
      case sizeof(uint8_t): \
        out = _InterlockedExchange8((char *) object, desired); \
        break; \
      default: \
        RCUTILS_LOG_ERROR_NAMED( \
          _RCUTILS_PACKAGE_NAME, "Unsupported integer type in atomic_exchange_strong"); \
        exit(-1); \
        break; \
    } \
  } while (0); \
  __pragma(warning(pop))

#define rcutils_win32_atomic_fetch_add(object, out, operand) \
  __pragma(warning(push)) \
  __pragma(warning(disable: 4244)) \
  __pragma(warning(disable: 4047)) \
  __pragma(warning(disable: 4024)) \
  do { \
    switch (sizeof(out)) { \
      case sizeof(uint64_t): \
        out = InterlockedExchangeAdd64((LONGLONG *) object, operand); \
        break; \
      case sizeof(uint32_t): \
        out = _InterlockedExchangeAdd((LONG *) object, operand); \
        break; \
      case sizeof(uint16_t): \
        out = _InterlockedExchangeAdd16((SHORT *) object, operand); \
        break; \
      case sizeof(uint8_t): \
        out = _InterlockedExchangeAdd8((char *) object, operand); \
        break; \
      default: \
        RCUTILS_LOG_ERROR_NAMED( \
          _RCUTILS_PACKAGE_NAME, "Unsupported integer type in atomic_fetch_add"); \
        exit(-1); \
        break; \
    } \
  } while (0); \
  __pragma(warning(pop))

#define rcutils_win32_atomic_fetch_and(object, out, operand) \
  __pragma(warning(push)) \
  __pragma(warning(disable: 4244)) \
  __pragma(warning(disable: 4047)) \
  __pragma(warning(disable: 4024)) \
  do { \
    switch (sizeof(out)) { \
      case sizeof(uint64_t): \
        out = InterlockedAnd64((LONGLONG *) object, operand); \
        break; \
      case sizeof(uint32_t): \
        out = _InterlockedAnd((LONG *) object, operand); \
        break; \
      case sizeof(uint16_t): \
        out = _InterlockedAnd16((SHORT *) object, operand); \
        break; \
      case sizeof(uint8_t): \
        out = _InterlockedAnd8((char *) object, operand); \
        break; \
      default: \
        RCUTILS_LOG_ERROR_NAMED( \
          _RCUTILS_PACKAGE_NAME, "Unsupported integer type in atomic_fetch_and"); \
        exit(-1); \
        break; \
    } \
  } while (0); \
  __pragma(warning(pop))

#define rcutils_win32_atomic_fetch_or(object, out, operand) \
  __pragma(warning(push)) \
  __pragma(warning(disable: 4244)) \
  __pragma(warning(disable: 4047)) \
  __pragma(warning(disable: 4024)) \
  do { \
    switch (sizeof(out)) { \
      case sizeof(uint64_t): \
        out = InterlockedOr64((LONGLONG *) object, operand); \
        break; \
      case sizeof(uint32_t): \
        out = _InterlockedOr((LONG *) object, operand); \
        break; \
      case sizeof(uint16_t): \
        out = _InterlockedOr16((SHORT *) object, operand); \
        break; \
      case sizeof(uint8_t): \
        out = _InterlockedOr8((char *) object, operand); \
        break; \
      default: \
        RCUTILS_LOG_ERROR_NAMED( \
          _RCUTILS_PACKAGE_NAME, "Unsupported integer type in atomic_fetch_or"); \
        exit(-1); \
        break; \
    } \
  } while (0); \
  __pragma(warning(pop))

#define rcutils_win32_atomic_fetch_sub(object, out, operand) \
  rcutils_win32_atomic_fetch_add(object, out, -(operand))

#define rcutils_win32_atomic_fetch_xor(object, out, operand) \
  __pragma(warning(push)) \
  __pragma(warning(disable: 4244)) \
  __pragma(warning(disable: 4047)) \
  __pragma(warning(disable: 4024)) \
  do { \
    switch (sizeof(out)) { \
      case sizeof(uint64_t): \
        out = InterlockedXor64((LONGLONG *) object, operand); \
        break; \
      case sizeof(uint32_t): \
        out = _InterlockedXor((LONG *) object, operand); \
        break; \
      case sizeof(uint16_t): \
        out = _InterlockedXor16((SHORT *) object, operand); \
        break; \
      case sizeof(uint8_t): \
        out = _InterlockedXor8((char *) object, operand); \
        break; \
      default: \
        RCUTILS_LOG_ERROR_NAMED( \
          _RCUTILS_PACKAGE_NAME, "Unsupported integer type in atomic_fetch_xor"); \
        exit(-1); \
        break; \
    } \
  } while (0); \
  __pragma(warning(pop))

#define rcutils_win32_atomic_load(object, out) \
  __pragma(warning(push)) \
  __pragma(warning(disable: 4244)) \
  __pragma(warning(disable: 4047)) \
  __pragma(warning(disable: 4024)) \
  do { \
    switch (sizeof(out)) { \
      case sizeof(uint64_t): \
        out = InterlockedExchangeAdd64((LONGLONG *) object, 0); \
        break; \
      case sizeof(uint32_t): \
        out = _InterlockedExchangeAdd((LONG *) object, 0); \
        break; \
      case sizeof(uint16_t): \
        out = _InterlockedExchangeAdd16((SHORT *) object, 0); \
        break; \
      case sizeof(uint8_t): \
        out = _InterlockedExchangeAdd8((char *) object, 0); \
        break; \
      default: \
        RCUTILS_LOG_ERROR_NAMED( \
          _RCUTILS_PACKAGE_NAME, "Unsupported integer type in atomic_load"); \
        exit(-1); \
        break; \
    } \
  } while (0); \
  __pragma(warning(pop))

// *INDENT-ON*

#define rcutils_win32_atomic_store(object, desired) \
  do { \
    MemoryBarrier(); \
    (object)->__val = (desired); \
    MemoryBarrier(); \
  } while (0)

/*
 * 7.17.8 Atomic flag type and operations. (disabled for now)
 */

// typedef atomic_bool     atomic_flag;

// #define ATOMIC_FLAG_INIT    ATOMIC_VAR_INIT(0)

// #define atomic_flag_clear_explicit(object, order) \
//   atomic_store_explicit(object, 0, order)
// #define atomic_flag_test_and_set_explicit(object, order) \
//   atomic_compare_exchange_strong_explicit(object, 0, 1, order, order)

// #define atomic_flag_clear(object) \
//   atomic_flag_clear_explicit(object, memory_order_seq_cst)
// #define atomic_flag_test_and_set(object) \
//   atomic_flag_test_and_set_explicit(object, memory_order_seq_cst)

#endif  // RCUTILS__STDATOMIC_HELPER__WIN32__STDATOMIC_H_
