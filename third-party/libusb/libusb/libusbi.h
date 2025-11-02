/*
 * Internal header for libusb
 * Copyright © 2007-2009 Daniel Drake <dsd@gentoo.org>
 * Copyright © 2001 Johannes Erdfelt <johannes@erdfelt.com>
 * Copyright © 2019 Nathan Hjelm <hjelmn@cs.umm.edu>
 * Copyright © 2019-2020 Google LLC. All rights reserved.
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

#ifndef LIBUSBI_H
#define LIBUSBI_H

#include <libusb/config.h>

#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include "libusb.h"

/* Not all C standard library headers define static_assert in assert.h
 * Additionally, Visual Studio treats static_assert as a keyword.
 */
#if !defined(__cplusplus) && !defined(static_assert) && !defined(_MSC_VER)
#define static_assert(cond, msg) _Static_assert(cond, msg)
#endif

#ifdef NDEBUG
#define ASSERT_EQ(expression, value) (void)expression
#define ASSERT_NE(expression, value) (void)expression
#else
#define ASSERT_EQ(expression, value) assert(expression == value)
#define ASSERT_NE(expression, value) assert(expression != value)
#endif

#define container_of(ptr, type, member) ((type *)((uintptr_t)(ptr) - (uintptr_t)offsetof(type, member)))

#ifndef ARRAYSIZE
#define ARRAYSIZE(array) (sizeof(array) / sizeof(array[0]))
#endif

#ifndef CLAMP
#define CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

/* The following is used to silence warnings for unused variables */
#if defined(UNREFERENCED_PARAMETER)
#define UNUSED(var) UNREFERENCED_PARAMETER(var)
#else
#define UNUSED(var)  \
    do {             \
        (void)(var); \
    } while(0)
#endif

/* Macro to align a value up to the next multiple of the size of a pointer */
#define PTR_ALIGN(v) (((v) + (sizeof(void *) - 1)) & ~(sizeof(void *) - 1))

/* Atomic operations
 *
 * Useful for reference counting or when accessing a value without a lock
 *
 * The following atomic operations are defined:
 *   usbi_atomic_load() - Atomically read a variable's value
 *   usbi_atomic_store() - Atomically write a new value value to a variable
 *   usbi_atomic_inc() - Atomically increment a variable's value and return the new value
 *   usbi_atomic_dec() - Atomically decrement a variable's value and return the new value
 *
 * All of these operations are ordered with each other, thus the effects of
 * any one operation is guaranteed to be seen by any other operation.
 */
#ifdef _MSC_VER
typedef volatile LONG usbi_atomic_t;
#define usbi_atomic_load(a) (*(a))
#define usbi_atomic_store(a, v) (*(a)) = (v)
#define usbi_atomic_inc(a) InterlockedIncrement((a))
#define usbi_atomic_dec(a) InterlockedDecrement((a))
#else
#include <stdatomic.h>
typedef atomic_long usbi_atomic_t;
#define usbi_atomic_load(a) atomic_load((a))
#define usbi_atomic_store(a, v) atomic_store((a), (v))
#define usbi_atomic_inc(a) (atomic_fetch_add((a), 1) + 1)
#define usbi_atomic_dec(a) (atomic_fetch_add((a), -1) - 1)
#endif

/* Internal abstractions for event handling and thread synchronization */
#if defined(PLATFORM_POSIX)
#include "os/events_posix.h"
#include "os/threads_posix.h"
#elif defined(PLATFORM_WINDOWS)
#include "os/events_windows.h"
#include "os/threads_windows.h"
#endif

/* Inside the libusb code, mark all public functions as follows:
 *   return_type API_EXPORTED function_name(params) { ... }
 * But if the function returns a pointer, mark it as follows:
 *   DEFAULT_VISIBILITY return_type * LIBUSB_CALL function_name(params) { ... }
 * In the libusb public header, mark all declarations as:
 *   return_type LIBUSB_CALL function_name(params);
 */
#define API_EXPORTED LIBUSB_CALL DEFAULT_VISIBILITY
#define API_EXPORTEDV LIBUSB_CALLV DEFAULT_VISIBILITY

#ifdef __cplusplus
extern "C" {
#endif

#define USB_MAXENDPOINTS 32
#define USB_MAXINTERFACES 32
#define USB_MAXCONFIG 8

/* Backend specific capabilities */
#define USBI_CAP_HAS_HID_ACCESS 0x00010000
#define USBI_CAP_SUPPORTS_DETACH_KERNEL_DRIVER 0x00020000

/* Maximum number of bytes in a log line */
#define USBI_MAX_LOG_LEN 1024
/* Terminator for log lines */
#define USBI_LOG_LINE_END "\n"

struct list_head {
    struct list_head *prev, *next;
};

/* Get an entry from the list
 *  ptr - the address of this list_head element in "type"
 *  type - the data type that contains "member"
 *  member - the list_head element in "type"
 */
#define list_entry(ptr, type, member) container_of(ptr, type, member)

#define list_first_entry(ptr, type, member) list_entry((ptr)->next, type, member)

#define list_next_entry(ptr, type, member) list_entry((ptr)->member.next, type, member)

/* Get each entry from a list
 *  pos - A structure pointer has a "member" element
 *  head - list head
 *  member - the list_head element in "pos"
 *  type - the type of the first parameter
 */
#define list_for_each_entry(pos, head, member, type) \
    for(pos = list_first_entry(head, type, member); &pos->member != (head); pos = list_next_entry(pos, type, member))

#define list_for_each_entry_safe(pos, n, head, member, type)                                                        \
    for(pos = list_first_entry(head, type, member), n = list_next_entry(pos, type, member); &pos->member != (head); \
        pos = n, n = list_next_entry(n, type, member))

/* Helper macros to iterate over a list. The structure pointed
 * to by "pos" must have a list_head member named "list".
 */
#define for_each_helper(pos, head, type) list_for_each_entry(pos, head, list, type)

#define for_each_safe_helper(pos, n, head, type) list_for_each_entry_safe(pos, n, head, list, type)

#define list_empty(entry) ((entry)->next == (entry))

static inline void list_init(struct list_head *entry) {
    entry->prev = entry->next = entry;
}

static inline void list_add(struct list_head *entry, struct list_head *head) {
    entry->next = head->next;
    entry->prev = head;

    head->next->prev = entry;
    head->next       = entry;
}

static inline void list_add_tail(struct list_head *entry, struct list_head *head) {
    entry->next = head;
    entry->prev = head->prev;

    head->prev->next = entry;
    head->prev       = entry;
}

static inline void list_del(struct list_head *entry) {
    entry->next->prev = entry->prev;
    entry->prev->next = entry->next;
    entry->next = entry->prev = NULL;
}

static inline void list_cut(struct list_head *list, struct list_head *head) {
    if(list_empty(head)) {
        list_init(list);
        return;
    }

    list->next       = head->next;
    list->next->prev = list;
    list->prev       = head->prev;
    list->prev->next = list;

    list_init(head);
}

static inline void list_splice_front(struct list_head *list, struct list_head *head) {
    list->next->prev = head;
    list->prev->next = head->next;
    head->next->prev = list->prev;
    head->next       = list->next;
}

static inline void *usbi_reallocf(void *ptr, size_t size) {
    void *ret = realloc(ptr, size);

    if(!ret)
        free(ptr);
    return ret;
}

#if !defined(USEC_PER_SEC)
#define USEC_PER_SEC 1000000L
#endif

#if !defined(NSEC_PER_SEC)
#define NSEC_PER_SEC 1000000000L
#endif

#define TIMEVAL_IS_VALID(tv) ((tv)->tv_sec >= 0 && (tv)->tv_usec >= 0 && (tv)->tv_usec < USEC_PER_SEC)

#define TIMESPEC_IS_SET(ts) ((ts)->tv_sec || (ts)->tv_nsec)
#define TIMESPEC_CLEAR(ts) (ts)->tv_sec = (ts)->tv_nsec = 0
#define TIMESPEC_CMP(a, b, CMP) (((a)->tv_sec == (b)->tv_sec) ? ((a)->tv_nsec CMP(b)->tv_nsec) : ((a)->tv_sec CMP(b)->tv_sec))
#define TIMESPEC_SUB(a, b, result)                       \
    do {                                                 \
        (result)->tv_sec  = (a)->tv_sec - (b)->tv_sec;   \
        (result)->tv_nsec = (a)->tv_nsec - (b)->tv_nsec; \
        if((result)->tv_nsec < 0L) {                     \
            --(result)->tv_sec;                          \
            (result)->tv_nsec += NSEC_PER_SEC;           \
        }                                                \
    } while(0)

#if defined(PLATFORM_WINDOWS)
#define TIMEVAL_TV_SEC_TYPE long
#else
#define TIMEVAL_TV_SEC_TYPE time_t
#endif

/* Some platforms don't have this define */
#ifndef TIMESPEC_TO_TIMEVAL
#define TIMESPEC_TO_TIMEVAL(tv, ts)                        \
    do {                                                   \
        (tv)->tv_sec  = (TIMEVAL_TV_SEC_TYPE)(ts)->tv_sec; \
        (tv)->tv_usec = (ts)->tv_nsec / 1000L;             \
    } while(0)
#endif

#ifdef ENABLE_LOGGING

#if defined(_MSC_VER) && (_MSC_VER < 1900)
#include <stdio.h>
#define snprintf usbi_snprintf
#define vsnprintf usbi_vsnprintf
int usbi_snprintf(char *dst, size_t size, const char *format, ...);
int usbi_vsnprintf(char *dst, size_t size, const char *format, va_list args);
#define LIBUSB_PRINTF_WIN32
#endif /* defined(_MSC_VER) && (_MSC_VER < 1900) */

void usbi_log(struct libusb_context *ctx, enum libusb_log_level level, const char *function, const char *format, ...) PRINTF_FORMAT(4, 5);

#define _usbi_log(ctx, level, ...) usbi_log(ctx, level, __func__, __VA_ARGS__)

#define usbi_err(ctx, ...) _usbi_log(ctx, LIBUSB_LOG_LEVEL_ERROR, __VA_ARGS__)
#define usbi_warn(ctx, ...) _usbi_log(ctx, LIBUSB_LOG_LEVEL_WARNING, __VA_ARGS__)
#define usbi_info(ctx, ...) _usbi_log(ctx, LIBUSB_LOG_LEVEL_INFO, __VA_ARGS__)
#define usbi_dbg(ctx, ...) _usbi_log(ctx, LIBUSB_LOG_LEVEL_DEBUG, __VA_ARGS__)

#else /* ENABLE_LOGGING */

#define usbi_err(ctx, ...) \
    do {                   \
        (void)(ctx);       \
    } while(0)
#define usbi_warn(ctx, ...) \
    do {                    \
        (void)(ctx);        \
    } while(0)
#define usbi_info(ctx, ...) \
    do {                    \
        (void)(ctx);        \
    } while(0)
#define usbi_dbg(ctx, ...) \
    do {                   \
        (void)(ctx);       \
    } while(0)

#endif /* ENABLE_LOGGING */

#define DEVICE_CTX(dev) ((dev)->ctx)
#define HANDLE_CTX(handle) ((handle) ? DEVICE_CTX((handle)->dev) : NULL)
#define ITRANSFER_CTX(itransfer) ((itransfer)->dev ? DEVICE_CTX((itransfer)->dev) : NULL)
#define TRANSFER_CTX(transfer) (ITRANSFER_CTX(LIBUSB_TRANSFER_TO_USBI_TRANSFER(transfer)))

#define IS_EPIN(ep) (0 != ((ep)&LIBUSB_ENDPOINT_IN))
#define IS_EPOUT(ep) (!IS_EPIN(ep))
#define IS_XFERIN(xfer) (0 != ((xfer)->endpoint & LIBUSB_ENDPOINT_IN))
#define IS_XFEROUT(xfer) (!IS_XFERIN(xfer))

struct libusb_context {
#if defined(ENABLE_LOGGING) && !defined(ENABLE_DEBUG_LOGGING)
    enum libusb_log_level debug;
    int                   debug_fixed;
    libusb_log_cb         log_handler;
#endif

    /* used for signalling occurrence of an internal event. */
    usbi_event_t event;

#ifdef HAVE_OS_TIMER
    /* used for timeout handling, if supported by OS.
     * this timer is maintained to trigger on the next pending timeout */
    usbi_timer_t timer;
#endif

    struct list_head usb_devs;
    usbi_mutex_t     usb_devs_lock;

    /* A list of open handles. Backends are free to traverse this if required.
     */
    struct list_head open_devs;
    usbi_mutex_t     open_devs_lock;

    /* A list of registered hotplug callbacks */
    struct list_head               hotplug_cbs;
    libusb_hotplug_callback_handle next_hotplug_cb_handle;
    usbi_mutex_t                   hotplug_cbs_lock;

    /* A flag to indicate that the context is ready for hotplug notifications */
    usbi_atomic_t hotplug_ready;

    /* this is a list of in-flight transfer handles, sorted by timeout
     * expiration. URBs to timeout the soonest are placed at the beginning of
     * the list, URBs that will time out later are placed after, and urbs with
     * infinite timeout are always placed at the very end. */
    struct list_head flying_transfers;
    /* Note paths taking both this and usbi_transfer->lock must always
     * take this lock first */
    usbi_mutex_t flying_transfers_lock;

#if !defined(PLATFORM_WINDOWS)
    /* user callbacks for pollfd changes */
    libusb_pollfd_added_cb   fd_added_cb;
    libusb_pollfd_removed_cb fd_removed_cb;
    void                    *fd_cb_user_data;
#endif

    /* ensures that only one thread is handling events at any one time */
    usbi_mutex_t events_lock;

    /* used to see if there is an active thread doing event handling */
    int event_handler_active;

    /* A thread-local storage key to track which thread is performing event
     * handling */
    usbi_tls_key_t event_handling_key;

    /* used to wait for event completion in threads other than the one that is
     * event handling */
    usbi_mutex_t event_waiters_lock;
    usbi_cond_t  event_waiters_cond;

    /* A lock to protect internal context event data. */
    usbi_mutex_t event_data_lock;

    /* A bitmask of flags that are set to indicate specific events that need to
     * be handled. Protected by event_data_lock. */
    unsigned int event_flags;

    /* A counter that is set when we want to interrupt and prevent event handling,
     * in order to safely close a device. Protected by event_data_lock. */
    unsigned int device_close;

    /* A list of currently active event sources. Protected by event_data_lock. */
    struct list_head event_sources;

    /* A list of event sources that have been removed since the last time
     * event sources were waited on. Protected by event_data_lock. */
    struct list_head removed_event_sources;

    /* A pointer and count to platform-specific data used for monitoring event
     * sources. Only accessed during event handling. */
    void        *event_data;
    unsigned int event_data_cnt;

    /* A list of pending hotplug messages. Protected by event_data_lock. */
    struct list_head hotplug_msgs;

    /* A list of pending completed transfers. Protected by event_data_lock. */
    struct list_head completed_transfers;

    struct list_head list;
};

extern struct libusb_context *usbi_default_context;
extern struct libusb_context *usbi_fallback_context;

extern struct list_head    active_contexts_list;
extern usbi_mutex_static_t active_contexts_lock;

static inline struct libusb_context *usbi_get_context(struct libusb_context *ctx) {
    static int warned = 0;

    if(!ctx) {
        ctx = usbi_default_context;
    }
    if(!ctx) {
        ctx = usbi_fallback_context;
        if(ctx && warned == 0) {
            usbi_err(ctx, "API misuse! Using non-default context as implicit default.");
            warned = 1;
        }
    }
    return ctx;
}

enum usbi_event_flags {
    /* The list of event sources has been modified */
    USBI_EVENT_EVENT_SOURCES_MODIFIED = 1U << 0,

    /* The user has interrupted the event handler */
    USBI_EVENT_USER_INTERRUPT = 1U << 1,

    /* A hotplug callback deregistration is pending */
    USBI_EVENT_HOTPLUG_CB_DEREGISTERED = 1U << 2,

    /* One or more hotplug messages are pending */
    USBI_EVENT_HOTPLUG_MSG_PENDING = 1U << 3,

    /* One or more completed transfers are pending */
    USBI_EVENT_TRANSFER_COMPLETED = 1U << 4,

    /* A device is in the process of being closed */
    USBI_EVENT_DEVICE_CLOSE = 1U << 5,
};

/* Macros for managing event handling state */
static inline int usbi_handling_events(struct libusb_context *ctx) {
    return usbi_tls_key_get(ctx->event_handling_key) != NULL;
}

static inline void usbi_start_event_handling(struct libusb_context *ctx) {
    usbi_tls_key_set(ctx->event_handling_key, ctx);
}

static inline void usbi_end_event_handling(struct libusb_context *ctx) {
    usbi_tls_key_set(ctx->event_handling_key, NULL);
}

struct libusb_device {
    usbi_atomic_t refcnt;

    struct libusb_context *ctx;
    struct libusb_device  *parent_dev;

    uint8_t           bus_number;
    uint8_t           port_number;
    uint8_t           device_address;
    enum libusb_speed speed;

    struct list_head list;
    unsigned long    session_data;

    struct libusb_device_descriptor device_descriptor;
    usbi_atomic_t                   attached;
    char                           *dev_path;

    char *interface_path[32];
    char *sub_composite_device_id[32];
};

struct libusb_device_handle {
    /* lock protects claimed_interfaces */
    usbi_mutex_t  lock;
    unsigned long claimed_interfaces;

    struct list_head      list;
    struct libusb_device *dev;
    int                   auto_detach_kernel_driver;
};

/* Function called by backend during device initialization to convert
 * multi-byte fields in the device descriptor to host-endian format.
 */
static inline void usbi_localize_device_descriptor(struct libusb_device_descriptor *desc) {
    desc->bcdUSB    = libusb_le16_to_cpu(desc->bcdUSB);
    desc->idVendor  = libusb_le16_to_cpu(desc->idVendor);
    desc->idProduct = libusb_le16_to_cpu(desc->idProduct);
    desc->bcdDevice = libusb_le16_to_cpu(desc->bcdDevice);
}

#if defined(HAVE_CLOCK_GETTIME) && !defined(__APPLE__)
static inline void usbi_get_monotonic_time(struct timespec *tp) {
    ASSERT_EQ(clock_gettime(CLOCK_MONOTONIC, tp), 0);
}
static inline void usbi_get_real_time(struct timespec *tp) {
    ASSERT_EQ(clock_gettime(CLOCK_REALTIME, tp), 0);
}
#else
/* If the platform doesn't provide the clock_gettime() function, the backend
 * must provide its own clock implementations.  Two clock functions are
 * required:
 *
 *   usbi_get_monotonic_time(): returns the time since an unspecified starting
 *                              point (usually boot) that is monotonically
 *                              increasing.
 *   usbi_get_real_time(): returns the time since system epoch.
 */
void usbi_get_monotonic_time(struct timespec *tp);
void usbi_get_real_time(struct timespec *tp);
#endif

/* in-memory transfer layout:
 *
 * 1. os private data
 * 2. struct usbi_transfer
 * 3. struct libusb_transfer (which includes iso packets) [variable size]
 *
 * from a libusb_transfer, you can get the usbi_transfer by rewinding the
 * appropriate number of bytes.
 */

struct usbi_transfer {
    int              num_iso_packets;
    struct list_head list;
    struct list_head completed_list;
    struct timespec  timeout;
    int              transferred;
    uint32_t         stream_id;
    uint32_t         state_flags;   /* Protected by usbi_transfer->lock */
    uint32_t         timeout_flags; /* Protected by the flying_stransfers_lock */

    /* The device reference is held until destruction for logging
     * even after dev_handle is set to NULL.  */
    struct libusb_device *dev;

    /* this lock is held during libusb_submit_transfer() and
     * libusb_cancel_transfer() (allowing the OS backend to prevent duplicate
     * cancellation, submission-during-cancellation, etc). the OS backend
     * should also take this lock in the handle_events path, to prevent the user
     * cancelling the transfer from another thread while you are processing
     * its completion (presumably there would be races within your OS backend
     * if this were possible).
     * Note paths taking both this and the flying_transfers_lock must
     * always take the flying_transfers_lock first */
    usbi_mutex_t lock;

    void *priv;
};

enum usbi_transfer_state_flags {
    /* Transfer successfully submitted by backend */
    USBI_TRANSFER_IN_FLIGHT = 1U << 0,

    /* Cancellation was requested via libusb_cancel_transfer() */
    USBI_TRANSFER_CANCELLING = 1U << 1,

    /* Operation on the transfer failed because the device disappeared */
    USBI_TRANSFER_DEVICE_DISAPPEARED = 1U << 2,
};

enum usbi_transfer_timeout_flags {
    /* Set by backend submit_transfer() if the OS handles timeout */
    USBI_TRANSFER_OS_HANDLES_TIMEOUT = 1U << 0,

    /* The transfer timeout has been handled */
    USBI_TRANSFER_TIMEOUT_HANDLED = 1U << 1,

    /* The transfer timeout was successfully processed */
    USBI_TRANSFER_TIMED_OUT = 1U << 2,
};

#define USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer) ((struct libusb_transfer *)((unsigned char *)(itransfer) + PTR_ALIGN(sizeof(struct usbi_transfer))))
#define LIBUSB_TRANSFER_TO_USBI_TRANSFER(transfer) ((struct usbi_transfer *)((unsigned char *)(transfer)-PTR_ALIGN(sizeof(struct usbi_transfer))))

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif

/* All standard descriptors have these 2 fields in common */
struct usbi_descriptor_header {
    uint8_t bLength;
    uint8_t bDescriptorType;
} LIBUSB_PACKED;

struct usbi_device_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} LIBUSB_PACKED;

struct usbi_configuration_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
} LIBUSB_PACKED;

struct usbi_interface_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} LIBUSB_PACKED;

struct usbi_string_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wData[ZERO_SIZED_ARRAY];
} LIBUSB_PACKED;

struct usbi_bos_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumDeviceCaps;
} LIBUSB_PACKED;

#ifdef _MSC_VER
#pragma pack(pop)
#endif

union usbi_config_desc_buf {
    struct usbi_configuration_descriptor desc;
    uint8_t                              buf[LIBUSB_DT_CONFIG_SIZE];
    uint16_t                             align; /* Force 2-byte alignment */
};

union usbi_string_desc_buf {
    struct usbi_string_descriptor desc;
    uint8_t                       buf[255]; /* Some devices choke on size > 255 */
    uint16_t                      align;    /* Force 2-byte alignment */
};

union usbi_bos_desc_buf {
    struct usbi_bos_descriptor desc;
    uint8_t                    buf[LIBUSB_DT_BOS_SIZE];
    uint16_t                   align; /* Force 2-byte alignment */
};

enum usbi_hotplug_flags {
    /* This callback is interested in device arrivals */
    USBI_HOTPLUG_DEVICE_ARRIVED = LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED,

    /* This callback is interested in device removals */
    USBI_HOTPLUG_DEVICE_LEFT = LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,

    /* IMPORTANT: The values for the below entries must start *after*
     * the highest value of the above entries!!!
     */

    /* The vendor_id field is valid for matching */
    USBI_HOTPLUG_VENDOR_ID_VALID = (1U << 3),

    /* The product_id field is valid for matching */
    USBI_HOTPLUG_PRODUCT_ID_VALID = (1U << 4),

    /* The dev_class field is valid for matching */
    USBI_HOTPLUG_DEV_CLASS_VALID = (1U << 5),

    /* This callback has been unregistered and needs to be freed */
    USBI_HOTPLUG_NEEDS_FREE = (1U << 6),
};

struct usbi_hotplug_callback {
    /* Flags that control how this callback behaves */
    uint8_t flags;

    /* Vendor ID to match (if flags says this is valid) */
    uint16_t vendor_id;

    /* Product ID to match (if flags says this is valid) */
    uint16_t product_id;

    /* Device class to match (if flags says this is valid) */
    uint8_t dev_class;

    /* Callback function to invoke for matching event/device */
    libusb_hotplug_callback_fn cb;

    /* Handle for this callback (used to match on deregister) */
    libusb_hotplug_callback_handle handle;

    /* User data that will be passed to the callback function */
    void *user_data;

    /* List this callback is registered in (ctx->hotplug_cbs) */
    struct list_head list;
};

struct usbi_hotplug_message {
    /* The hotplug event that occurred */
    libusb_hotplug_event event;

    /* The device for which this hotplug event occurred */
    struct libusb_device *device;

    /* List this message is contained in (ctx->hotplug_msgs) */
    struct list_head list;
};

/* shared data and functions */

void usbi_hotplug_init(struct libusb_context *ctx);
void usbi_hotplug_exit(struct libusb_context *ctx);
void usbi_hotplug_notification(struct libusb_context *ctx, struct libusb_device *dev, libusb_hotplug_event event);
void usbi_hotplug_process(struct libusb_context *ctx, struct list_head *hotplug_msgs);

int  usbi_io_init(struct libusb_context *ctx);
void usbi_io_exit(struct libusb_context *ctx);

struct libusb_device *usbi_alloc_device(struct libusb_context *ctx, unsigned long session_id);
struct libusb_device *usbi_get_device_by_session_id(struct libusb_context *ctx, unsigned long session_id);
int                   usbi_sanitize_device(struct libusb_device *dev);
void                  usbi_handle_disconnect(struct libusb_device_handle *dev_handle);

int  usbi_handle_transfer_completion(struct usbi_transfer *itransfer, enum libusb_transfer_status status);
int  usbi_handle_transfer_cancellation(struct usbi_transfer *itransfer);
void usbi_signal_transfer_completion(struct usbi_transfer *itransfer);

void usbi_connect_device(struct libusb_device *dev);
void usbi_disconnect_device(struct libusb_device *dev);

struct usbi_event_source {
    struct usbi_event_source_data {
        usbi_os_handle_t os_handle;
        short            poll_events;
    } data;
    struct list_head list;
};

int  usbi_add_event_source(struct libusb_context *ctx, usbi_os_handle_t os_handle, short poll_events);
void usbi_remove_event_source(struct libusb_context *ctx, usbi_os_handle_t os_handle);

struct usbi_option {
    int is_set;
    union {
        int ival;
    } arg;
};

/* OS event abstraction */

int  usbi_create_event(usbi_event_t *event);
void usbi_destroy_event(usbi_event_t *event);
void usbi_signal_event(usbi_event_t *event);
void usbi_clear_event(usbi_event_t *event);

#ifdef HAVE_OS_TIMER
int  usbi_create_timer(usbi_timer_t *timer);
void usbi_destroy_timer(usbi_timer_t *timer);
int  usbi_arm_timer(usbi_timer_t *timer, const struct timespec *timeout);
int  usbi_disarm_timer(usbi_timer_t *timer);
#endif

static inline int usbi_using_timer(struct libusb_context *ctx) {
#ifdef HAVE_OS_TIMER
    return usbi_timer_valid(&ctx->timer);
#else
    UNUSED(ctx);
    return 0;
#endif
}

struct usbi_reported_events {
    union {
        struct {
            unsigned int event_triggered : 1;
#ifdef HAVE_OS_TIMER
            unsigned int timer_triggered : 1;
#endif
        };
        unsigned int event_bits;
    };
    void        *event_data;
    unsigned int event_data_count;
    unsigned int num_ready;
};

int usbi_alloc_event_data(struct libusb_context *ctx);
int usbi_wait_for_events(struct libusb_context *ctx, struct usbi_reported_events *reported_events, int timeout_ms);

/* accessor functions for structure private data */

static inline void *usbi_get_context_priv(struct libusb_context *ctx) {
    return (unsigned char *)ctx + PTR_ALIGN(sizeof(*ctx));
}

static inline void *usbi_get_device_priv(struct libusb_device *dev) {
    return (unsigned char *)dev + PTR_ALIGN(sizeof(*dev));
}

static inline void *usbi_get_device_handle_priv(struct libusb_device_handle *dev_handle) {
    return (unsigned char *)dev_handle + PTR_ALIGN(sizeof(*dev_handle));
}

static inline void *usbi_get_transfer_priv(struct usbi_transfer *itransfer) {
    return itransfer->priv;
}

/* device discovery */

/* we traverse usbfs without knowing how many devices we are going to find.
 * so we create this discovered_devs model which is similar to a linked-list
 * which grows when required. it can be freed once discovery has completed,
 * eliminating the need for a list node in the libusb_device structure
 * itself. */
struct discovered_devs {
    size_t                len;
    size_t                capacity;
    struct libusb_device *devices[ZERO_SIZED_ARRAY];
};

struct discovered_devs *discovered_devs_append(struct discovered_devs *discdevs, struct libusb_device *dev);

/* OS abstraction */

/* This is the interface that OS backends need to implement.
 * All fields are mandatory, except ones explicitly noted as optional. */
struct usbi_os_backend {
    /* A human-readable name for your backend, e.g. "Linux usbfs" */
    const char *name;

    /* Binary mask for backend specific capabilities */
    uint32_t caps;

    /* Perform initialization of your backend. You might use this function
     * to determine specific capabilities of the system, allocate required
     * data structures for later, etc.
     *
     * This function is called when a libusb user initializes the library
     * prior to use. Mutual exclusion with other init and exit calls is
     * guaranteed when this function is called.
     *
     * Return 0 on success, or a LIBUSB_ERROR code on failure.
     */
    int (*init)(struct libusb_context *ctx);

    /* Deinitialization. Optional. This function should destroy anything
     * that was set up by init.
     *
     * This function is called when the user deinitializes the library.
     * Mutual exclusion with other init and exit calls is guaranteed when
     * this function is called.
     */
    void (*exit)(struct libusb_context *ctx);

    /* Set a backend-specific option. Optional.
     *
     * This function is called when the user calls libusb_set_option() and
     * the option is not handled by the core library.
     *
     * Return 0 on success, or a LIBUSB_ERROR code on failure.
     */
    int (*set_option)(struct libusb_context *ctx, enum libusb_option option, va_list args);

    /* Enumerate all the USB devices on the system, returning them in a list
     * of discovered devices.
     *
     * Your implementation should enumerate all devices on the system,
     * regardless of whether they have been seen before or not.
     *
     * When you have found a device, compute a session ID for it. The session
     * ID should uniquely represent that particular device for that particular
     * connection session since boot (i.e. if you disconnect and reconnect a
     * device immediately after, it should be assigned a different session ID).
     * If your OS cannot provide a unique session ID as described above,
     * presenting a session ID of (bus_number << 8 | device_address) should
     * be sufficient. Bus numbers and device addresses wrap and get reused,
     * but that is an unlikely case.
     *
     * After computing a session ID for a device, call
     * usbi_get_device_by_session_id(). This function checks if libusb already
     * knows about the device, and if so, it provides you with a reference
     * to a libusb_device structure for it.
     *
     * If usbi_get_device_by_session_id() returns NULL, it is time to allocate
     * a new device structure for the device. Call usbi_alloc_device() to
     * obtain a new libusb_device structure with reference count 1. Populate
     * the bus_number and device_address attributes of the new device, and
     * perform any other internal backend initialization you need to do. At
     * this point, you should be ready to provide device descriptors and so
     * on through the get_*_descriptor functions. Finally, call
     * usbi_sanitize_device() to perform some final sanity checks on the
     * device. Assuming all of the above succeeded, we can now continue.
     * If any of the above failed, remember to unreference the device that
     * was returned by usbi_alloc_device().
     *
     * At this stage we have a populated libusb_device structure (either one
     * that was found earlier, or one that we have just allocated and
     * populated). This can now be added to the discovered devices list
     * using discovered_devs_append(). Note that discovered_devs_append()
     * may reallocate the list, returning a new location for it, and also
     * note that reallocation can fail. Your backend should handle these
     * error conditions appropriately.
     *
     * This function should not generate any bus I/O and should not block.
     * If I/O is required (e.g. reading the active configuration value), it is
     * OK to ignore these suggestions :)
     *
     * This function is executed when the user wishes to retrieve a list
     * of USB devices connected to the system.
     *
     * If the backend has hotplug support, this function is not used!
     *
     * Return 0 on success, or a LIBUSB_ERROR code on failure.
     */
    int (*get_device_list)(struct libusb_context *ctx, struct discovered_devs **discdevs);

    /* Apps which were written before hotplug support, may listen for
     * hotplug events on their own and call libusb_get_device_list on
     * device addition. In this case libusb_get_device_list will likely
     * return a list without the new device in there, as the hotplug
     * event thread will still be busy enumerating the device, which may
     * take a while, or may not even have seen the event yet.
     *
     * To avoid this libusb_get_device_list will call this optional
     * function for backends with hotplug support before copying
     * ctx->usb_devs to the user. In this function the backend should
     * ensure any pending hotplug events are fully processed before
     * returning.
     *
     * Optional, should be implemented by backends with hotplug support.
     */
    void (*hotplug_poll)(void);

    /* Wrap a platform-specific device handle for I/O and other USB
     * operations. The device handle is preallocated for you.
     *
     * Your backend should allocate any internal resources required for I/O
     * and other operations so that those operations can happen (hopefully)
     * without hiccup. This is also a good place to inform libusb that it
     * should monitor certain file descriptors related to this device -
     * see the usbi_add_event_source() function.
     *
     * Your backend should also initialize the device structure
     * (dev_handle->dev), which is NULL at the beginning of the call.
     *
     * This function should not generate any bus I/O and should not block.
     *
     * This function is called when the user attempts to wrap an existing
     * platform-specific device handle for a device.
     *
     * Return:
     * - 0 on success
     * - LIBUSB_ERROR_ACCESS if the user has insufficient permissions
     * - another LIBUSB_ERROR code on other failure
     *
     * Do not worry about freeing the handle on failed open, the upper layers
     * do this for you.
     */
    int (*wrap_sys_device)(struct libusb_context *ctx, struct libusb_device_handle *dev_handle, intptr_t sys_dev);

    /* Open a device for I/O and other USB operations. The device handle
     * is preallocated for you, you can retrieve the device in question
     * through handle->dev.
     *
     * Your backend should allocate any internal resources required for I/O
     * and other operations so that those operations can happen (hopefully)
     * without hiccup. This is also a good place to inform libusb that it
     * should monitor certain file descriptors related to this device -
     * see the usbi_add_event_source() function.
     *
     * This function should not generate any bus I/O and should not block.
     *
     * This function is called when the user attempts to obtain a device
     * handle for a device.
     *
     * Return:
     * - 0 on success
     * - LIBUSB_ERROR_ACCESS if the user has insufficient permissions
     * - LIBUSB_ERROR_NO_DEVICE if the device has been disconnected since
     *   discovery
     * - another LIBUSB_ERROR code on other failure
     *
     * Do not worry about freeing the handle on failed open, the upper layers
     * do this for you.
     */
    int (*open)(struct libusb_device_handle *dev_handle);

    /* Close a device such that the handle cannot be used again. Your backend
     * should destroy any resources that were allocated in the open path.
     * This may also be a good place to call usbi_remove_event_source() to
     * inform libusb of any event sources associated with this device that
     * should no longer be monitored.
     *
     * This function is called when the user closes a device handle.
     */
    void (*close)(struct libusb_device_handle *dev_handle);

    /* Get the ACTIVE configuration descriptor for a device.
     *
     * The descriptor should be retrieved from memory, NOT via bus I/O to the
     * device. This means that you may have to cache it in a private structure
     * during get_device_list enumeration. You may also have to keep track
     * of which configuration is active when the user changes it.
     *
     * This function is expected to write len bytes of data into buffer, which
     * is guaranteed to be big enough. If you can only do a partial write,
     * return an error code.
     *
     * This function is expected to return the descriptor in bus-endian format
     * (LE).
     *
     * Return:
     * - 0 on success
     * - LIBUSB_ERROR_NOT_FOUND if the device is in unconfigured state
     * - another LIBUSB_ERROR code on other failure
     */
    int (*get_active_config_descriptor)(struct libusb_device *device, void *buffer, size_t len);

    /* Get a specific configuration descriptor for a device.
     *
     * The descriptor should be retrieved from memory, NOT via bus I/O to the
     * device. This means that you may have to cache it in a private structure
     * during get_device_list enumeration.
     *
     * The requested descriptor is expressed as a zero-based index (i.e. 0
     * indicates that we are requesting the first descriptor). The index does
     * not (necessarily) equal the bConfigurationValue of the configuration
     * being requested.
     *
     * This function is expected to write len bytes of data into buffer, which
     * is guaranteed to be big enough. If you can only do a partial write,
     * return an error code.
     *
     * This function is expected to return the descriptor in bus-endian format
     * (LE).
     *
     * Return the length read on success or a LIBUSB_ERROR code on failure.
     */
    int (*get_config_descriptor)(struct libusb_device *device, uint8_t config_index, void *buffer, size_t len);

    /* Like get_config_descriptor but then by bConfigurationValue instead
     * of by index.
     *
     * Optional, if not present the core will call get_config_descriptor
     * for all configs until it finds the desired bConfigurationValue.
     *
     * Returns a pointer to the raw-descriptor in *buffer, this memory
     * is valid as long as device is valid.
     *
     * Returns the length of the returned raw-descriptor on success,
     * or a LIBUSB_ERROR code on failure.
     */
    int (*get_config_descriptor_by_value)(struct libusb_device *device, uint8_t bConfigurationValue, void **buffer);

    /* Get the bConfigurationValue for the active configuration for a device.
     * Optional. This should only be implemented if you can retrieve it from
     * cache (don't generate I/O).
     *
     * If you cannot retrieve this from cache, either do not implement this
     * function, or return LIBUSB_ERROR_NOT_SUPPORTED. This will cause
     * libusb to retrieve the information through a standard control transfer.
     *
     * This function must be non-blocking.
     * Return:
     * - 0 on success
     * - LIBUSB_ERROR_NO_DEVICE if the device has been disconnected since it
     *   was opened
     * - LIBUSB_ERROR_NOT_SUPPORTED if the value cannot be retrieved without
     *   blocking
     * - another LIBUSB_ERROR code on other failure.
     */
    int (*get_configuration)(struct libusb_device_handle *dev_handle, uint8_t *config);

    /* Set the active configuration for a device.
     *
     * A configuration value of -1 should put the device in unconfigured state.
     *
     * This function can block.
     *
     * Return:
     * - 0 on success
     * - LIBUSB_ERROR_NOT_FOUND if the configuration does not exist
     * - LIBUSB_ERROR_BUSY if interfaces are currently claimed (and hence
     *   configuration cannot be changed)
     * - LIBUSB_ERROR_NO_DEVICE if the device has been disconnected since it
     *   was opened
     * - another LIBUSB_ERROR code on other failure.
     */
    int (*set_configuration)(struct libusb_device_handle *dev_handle, int config);

    /* Claim an interface. When claimed, the application can then perform
     * I/O to an interface's endpoints.
     *
     * This function should not generate any bus I/O and should not block.
     * Interface claiming is a logical operation that simply ensures that
     * no other drivers/applications are using the interface, and after
     * claiming, no other drivers/applications can use the interface because
     * we now "own" it.
     *
     * Return:
     * - 0 on success
     * - LIBUSB_ERROR_NOT_FOUND if the interface does not exist
     * - LIBUSB_ERROR_BUSY if the interface is in use by another driver/app
     * - LIBUSB_ERROR_NO_DEVICE if the device has been disconnected since it
     *   was opened
     * - another LIBUSB_ERROR code on other failure
     */
    int (*claim_interface)(struct libusb_device_handle *dev_handle, uint8_t interface_number);

    /* Release a previously claimed interface.
     *
     * This function should also generate a SET_INTERFACE control request,
     * resetting the alternate setting of that interface to 0. It's OK for
     * this function to block as a result.
     *
     * You will only ever be asked to release an interface which was
     * successfully claimed earlier.
     *
     * Return:
     * - 0 on success
     * - LIBUSB_ERROR_NO_DEVICE if the device has been disconnected since it
     *   was opened
     * - another LIBUSB_ERROR code on other failure
     */
    int (*release_interface)(struct libusb_device_handle *dev_handle, uint8_t interface_number);

    /* Set the alternate setting for an interface.
     *
     * You will only ever be asked to set the alternate setting for an
     * interface which was successfully claimed earlier.
     *
     * It's OK for this function to block.
     *
     * Return:
     * - 0 on success
     * - LIBUSB_ERROR_NOT_FOUND if the alternate setting does not exist
     * - LIBUSB_ERROR_NO_DEVICE if the device has been disconnected since it
     *   was opened
     * - another LIBUSB_ERROR code on other failure
     */
    int (*set_interface_altsetting)(struct libusb_device_handle *dev_handle, uint8_t interface_number, uint8_t altsetting);

    /* Clear a halt/stall condition on an endpoint.
     *
     * It's OK for this function to block.
     *
     * Return:
     * - 0 on success
     * - LIBUSB_ERROR_NOT_FOUND if the endpoint does not exist
     * - LIBUSB_ERROR_NO_DEVICE if the device has been disconnected since it
     *   was opened
     * - another LIBUSB_ERROR code on other failure
     */
    int (*clear_halt)(struct libusb_device_handle *dev_handle, unsigned char endpoint);

    /* Perform a USB port reset to reinitialize a device. Optional.
     *
     * If possible, the device handle should still be usable after the reset
     * completes, assuming that the device descriptors did not change during
     * reset and all previous interface state can be restored.
     *
     * If something changes, or you cannot easily locate/verify the reset
     * device, return LIBUSB_ERROR_NOT_FOUND. This prompts the application
     * to close the old handle and re-enumerate the device.
     *
     * Return:
     * - 0 on success
     * - LIBUSB_ERROR_NOT_FOUND if re-enumeration is required, or if the device
     *   has been disconnected since it was opened
     * - another LIBUSB_ERROR code on other failure
     */
    int (*reset_device)(struct libusb_device_handle *dev_handle);

    /* Alloc num_streams usb3 bulk streams on the passed in endpoints */
    int (*alloc_streams)(struct libusb_device_handle *dev_handle, uint32_t num_streams, unsigned char *endpoints, int num_endpoints);

    /* Free usb3 bulk streams allocated with alloc_streams */
    int (*free_streams)(struct libusb_device_handle *dev_handle, unsigned char *endpoints, int num_endpoints);

    /* Allocate persistent DMA memory for the given device, suitable for
     * zerocopy. May return NULL on failure. Optional to implement.
     */
    void *(*dev_mem_alloc)(struct libusb_device_handle *handle, size_t len);

    /* Free memory allocated by dev_mem_alloc. */
    int (*dev_mem_free)(struct libusb_device_handle *handle, void *buffer, size_t len);

    /* Determine if a kernel driver is active on an interface. Optional.
     *
     * The presence of a kernel driver on an interface indicates that any
     * calls to claim_interface would fail with the LIBUSB_ERROR_BUSY code.
     *
     * Return:
     * - 0 if no driver is active
     * - 1 if a driver is active
     * - LIBUSB_ERROR_NO_DEVICE if the device has been disconnected since it
     *   was opened
     * - another LIBUSB_ERROR code on other failure
     */
    int (*kernel_driver_active)(struct libusb_device_handle *dev_handle, uint8_t interface_number);

    /* Detach a kernel driver from an interface. Optional.
     *
     * After detaching a kernel driver, the interface should be available
     * for claim.
     *
     * Return:
     * - 0 on success
     * - LIBUSB_ERROR_NOT_FOUND if no kernel driver was active
     * - LIBUSB_ERROR_INVALID_PARAM if the interface does not exist
     * - LIBUSB_ERROR_NO_DEVICE if the device has been disconnected since it
     *   was opened
     * - another LIBUSB_ERROR code on other failure
     */
    int (*detach_kernel_driver)(struct libusb_device_handle *dev_handle, uint8_t interface_number);

    /* Attach a kernel driver to an interface. Optional.
     *
     * Reattach a kernel driver to the device.
     *
     * Return:
     * - 0 on success
     * - LIBUSB_ERROR_NOT_FOUND if no kernel driver was active
     * - LIBUSB_ERROR_INVALID_PARAM if the interface does not exist
     * - LIBUSB_ERROR_NO_DEVICE if the device has been disconnected since it
     *   was opened
     * - LIBUSB_ERROR_BUSY if a program or driver has claimed the interface,
     *   preventing reattachment
     * - another LIBUSB_ERROR code on other failure
     */
    int (*attach_kernel_driver)(struct libusb_device_handle *dev_handle, uint8_t interface_number);

    /* Destroy a device. Optional.
     *
     * This function is called when the last reference to a device is
     * destroyed. It should free any resources allocated in the get_device_list
     * path.
     */
    void (*destroy_device)(struct libusb_device *dev);

    /* Submit a transfer. Your implementation should take the transfer,
     * morph it into whatever form your platform requires, and submit it
     * asynchronously.
     *
     * This function must not block.
     *
     * This function gets called with the flying_transfers_lock locked!
     *
     * Return:
     * - 0 on success
     * - LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
     * - another LIBUSB_ERROR code on other failure
     */
    int (*submit_transfer)(struct usbi_transfer *itransfer);

    /* Cancel a previously submitted transfer.
     *
     * This function must not block. The transfer cancellation must complete
     * later, resulting in a call to usbi_handle_transfer_cancellation()
     * from the context of handle_events.
     */
    int (*cancel_transfer)(struct usbi_transfer *itransfer);

    /* Clear a transfer as if it has completed or cancelled, but do not
     * report any completion/cancellation to the library. You should free
     * all private data from the transfer as if you were just about to report
     * completion or cancellation.
     *
     * This function might seem a bit out of place. It is used when libusb
     * detects a disconnected device - it calls this function for all pending
     * transfers before reporting completion (with the disconnect code) to
     * the user. Maybe we can improve upon this internal interface in future.
     */
    void (*clear_transfer_priv)(struct usbi_transfer *itransfer);

    /* Handle any pending events on event sources. Optional.
     *
     * Provide this function when event sources directly indicate device
     * or transfer activity. If your backend does not have such event sources,
     * implement the handle_transfer_completion function below.
     *
     * This involves monitoring any active transfers and processing their
     * completion or cancellation.
     *
     * The function is passed a pointer that represents platform-specific
     * data for monitoring event sources (size count). This data is to be
     * (re)allocated as necessary when event sources are modified.
     * The num_ready parameter indicates the number of event sources that
     * have reported events. This should be enough information for you to
     * determine which actions need to be taken on the currently active
     * transfers.
     *
     * For any cancelled transfers, call usbi_handle_transfer_cancellation().
     * For completed transfers, call usbi_handle_transfer_completion().
     * For control/bulk/interrupt transfers, populate the "transferred"
     * element of the appropriate usbi_transfer structure before calling the
     * above functions. For isochronous transfers, populate the status and
     * transferred fields of the iso packet descriptors of the transfer.
     *
     * This function should also be able to detect disconnection of the
     * device, reporting that situation with usbi_handle_disconnect().
     *
     * When processing an event related to a transfer, you probably want to
     * take usbi_transfer.lock to prevent races. See the documentation for
     * the usbi_transfer structure.
     *
     * Return 0 on success, or a LIBUSB_ERROR code on failure.
     */
    int (*handle_events)(struct libusb_context *ctx, void *event_data, unsigned int count, unsigned int num_ready);

    /* Handle transfer completion. Optional.
     *
     * Provide this function when there are no event sources available that
     * directly indicate device or transfer activity. If your backend does
     * have such event sources, implement the handle_events function above.
     *
     * Your backend must tell the library when a transfer has completed by
     * calling usbi_signal_transfer_completion(). You should store any private
     * information about the transfer and its completion status in the transfer's
     * private backend data.
     *
     * During event handling, this function will be called on each transfer for
     * which usbi_signal_transfer_completion() was called.
     *
     * For any cancelled transfers, call usbi_handle_transfer_cancellation().
     * For completed transfers, call usbi_handle_transfer_completion().
     * For control/bulk/interrupt transfers, populate the "transferred"
     * element of the appropriate usbi_transfer structure before calling the
     * above functions. For isochronous transfers, populate the status and
     * transferred fields of the iso packet descriptors of the transfer.
     *
     * Return 0 on success, or a LIBUSB_ERROR code on failure.
     */
    int (*handle_transfer_completion)(struct usbi_transfer *itransfer);

    /* Number of bytes to reserve for per-context private backend data.
     * This private data area is accessible by calling
     * usbi_get_context_priv() on the libusb_context instance.
     */
    size_t context_priv_size;

    /* Number of bytes to reserve for per-device private backend data.
     * This private data area is accessible by calling
     * usbi_get_device_priv() on the libusb_device instance.
     */
    size_t device_priv_size;

    /* Number of bytes to reserve for per-handle private backend data.
     * This private data area is accessible by calling
     * usbi_get_device_handle_priv() on the libusb_device_handle instance.
     */
    size_t device_handle_priv_size;

    /* Number of bytes to reserve for per-transfer private backend data.
     * This private data area is accessible by calling
     * usbi_get_transfer_priv() on the usbi_transfer instance.
     */
    size_t transfer_priv_size;
};

extern const struct usbi_os_backend usbi_backend;

#define for_each_context(c) for_each_helper(c, &active_contexts_list, struct libusb_context)

#define for_each_device(ctx, d) for_each_helper(d, &(ctx)->usb_devs, struct libusb_device)

#define for_each_device_safe(ctx, d, n) for_each_safe_helper(d, n, &(ctx)->usb_devs, struct libusb_device)

#define for_each_open_device(ctx, h) for_each_helper(h, &(ctx)->open_devs, struct libusb_device_handle)

#define __for_each_transfer(list, t) for_each_helper(t, (list), struct usbi_transfer)

#define for_each_transfer(ctx, t) __for_each_transfer(&(ctx)->flying_transfers, t)

#define __for_each_transfer_safe(list, t, n) for_each_safe_helper(t, n, (list), struct usbi_transfer)

#define for_each_transfer_safe(ctx, t, n) __for_each_transfer_safe(&(ctx)->flying_transfers, t, n)

#define __for_each_completed_transfer_safe(list, t, n) list_for_each_entry_safe(t, n, (list), completed_list, struct usbi_transfer)

#define for_each_event_source(ctx, e) for_each_helper(e, &(ctx)->event_sources, struct usbi_event_source)

#define for_each_removed_event_source(ctx, e) for_each_helper(e, &(ctx)->removed_event_sources, struct usbi_event_source)

#define for_each_removed_event_source_safe(ctx, e, n) for_each_safe_helper(e, n, &(ctx)->removed_event_sources, struct usbi_event_source)

#define for_each_hotplug_cb(ctx, c) for_each_helper(c, &(ctx)->hotplug_cbs, struct usbi_hotplug_callback)

#define for_each_hotplug_cb_safe(ctx, c, n) for_each_safe_helper(c, n, &(ctx)->hotplug_cbs, struct usbi_hotplug_callback)

#ifdef __cplusplus
}
#endif

#endif
