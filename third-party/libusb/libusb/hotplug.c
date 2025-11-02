/* -*- Mode: C; indent-tabs-mode:t ; c-basic-offset:8 -*- */
/*
 * Hotplug functions for libusb
 * Copyright © 2012-2021 Nathan Hjelm <hjelmn@mac.com>
 * Copyright © 2012-2013 Peter Stuge <peter@stuge.se>
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

/**
 * @defgroup libusb_hotplug Device hotplug event notification
 * This page details how to use the libusb hotplug interface, where available.
 *
 * Be mindful that not all platforms currently implement hotplug notification and
 * that you should first call on \ref libusb_has_capability() with parameter
 * \ref LIBUSB_CAP_HAS_HOTPLUG to confirm that hotplug support is available.
 *
 * \page libusb_hotplug Device hotplug event notification
 *
 * \section hotplug_intro Introduction
 *
 * Version 1.0.16, \ref LIBUSB_API_VERSION >= 0x01000102, has added support
 * for hotplug events on <b>some</b> platforms (you should test if your platform
 * supports hotplug notification by calling \ref libusb_has_capability() with
 * parameter \ref LIBUSB_CAP_HAS_HOTPLUG).
 *
 * This interface allows you to request notification for the arrival and departure
 * of matching USB devices.
 *
 * To receive hotplug notification you register a callback by calling
 * \ref libusb_hotplug_register_callback(). This function will optionally return
 * a callback handle that can be passed to \ref libusb_hotplug_deregister_callback().
 *
 * A callback function must return an int (0 or 1) indicating whether the callback is
 * expecting additional events. Returning 0 will rearm the callback and 1 will cause
 * the callback to be deregistered. Note that when callbacks are called from
 * libusb_hotplug_register_callback() because of the \ref LIBUSB_HOTPLUG_ENUMERATE
 * flag, the callback return value is ignored. In other words, you cannot cause a
 * callback to be deregistered by returning 1 when it is called from
 * libusb_hotplug_register_callback().
 *
 * Callbacks for a particular context are automatically deregistered by libusb_exit().
 *
 * As of 1.0.16 there are two supported hotplug events:
 *  - LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED: A device has arrived and is ready to use
 *  - LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT: A device has left and is no longer available
 *
 * A hotplug event can listen for either or both of these events.
 *
 * Note: If you receive notification that a device has left and you have any
 * libusb_device_handles for the device it is up to you to call libusb_close()
 * on each device handle to free up any remaining resources associated with the device.
 * Once a device has left any libusb_device_handle associated with the device
 * are invalid and will remain so even if the device comes back.
 *
 * When handling a LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED event it is considered
 * safe to call any libusb function that takes a libusb_device. It also safe to
 * open a device and submit asynchronous transfers. However, most other functions
 * that take a libusb_device_handle are <b>not</b> safe to call. Examples of such
 * functions are any of the \ref libusb_syncio "synchronous API" functions or the blocking
 * functions that retrieve various \ref libusb_desc "USB descriptors". These functions must
 * be used outside of the context of the hotplug callback.
 *
 * When handling a LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT event the only safe function
 * is libusb_get_device_descriptor().
 *
 * The following code provides an example of the usage of the hotplug interface:
\code
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <libusb.h>

static int count = 0;

int hotplug_callback(struct libusb_context *ctx, struct libusb_device *dev,
                     libusb_hotplug_event event, void *user_data) {
  static libusb_device_handle *dev_handle = NULL;
  struct libusb_device_descriptor desc;
  int rc;

  (void)libusb_get_device_descriptor(dev, &desc);

  if (LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED == event) {
    rc = libusb_open(dev, &dev_handle);
    if (LIBUSB_SUCCESS != rc) {
      printf("Could not open USB device\n");
    }
  } else if (LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT == event) {
    if (dev_handle) {
      libusb_close(dev_handle);
      dev_handle = NULL;
    }
  } else {
    printf("Unhandled event %d\n", event);
  }
  count++;

  return 0;
}

int main (void) {
  libusb_hotplug_callback_handle callback_handle;
  int rc;

  libusb_init(NULL);

  rc = libusb_hotplug_register_callback(NULL, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
                                        LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, 0, 0x045a, 0x5005,
                                        LIBUSB_HOTPLUG_MATCH_ANY, hotplug_callback, NULL,
                                        &callback_handle);
  if (LIBUSB_SUCCESS != rc) {
    printf("Error creating a hotplug callback\n");
    libusb_exit(NULL);
    return EXIT_FAILURE;
  }

  while (count < 2) {
    libusb_handle_events_completed(NULL, NULL);
    nanosleep(&(struct timespec){0, 10000000UL}, NULL);
  }

  libusb_hotplug_deregister_callback(NULL, callback_handle);
  libusb_exit(NULL);

  return 0;
}
\endcode
 */

#define VALID_HOTPLUG_EVENTS (LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT)

#define VALID_HOTPLUG_FLAGS (LIBUSB_HOTPLUG_ENUMERATE)

void usbi_hotplug_init(struct libusb_context *ctx) {
    /* check for hotplug support */
    if(!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG))
        return;

    usbi_mutex_init(&ctx->hotplug_cbs_lock);
    list_init(&ctx->hotplug_cbs);
    ctx->next_hotplug_cb_handle = 1;
    usbi_atomic_store(&ctx->hotplug_ready, 1);
}

void usbi_hotplug_exit(struct libusb_context *ctx) {
    struct usbi_hotplug_callback *hotplug_cb, *next_cb;
    struct usbi_hotplug_message  *msg;
    struct libusb_device         *dev, *next_dev;

    /* check for hotplug support */
    if(!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG))
        return;

    if(!usbi_atomic_load(&ctx->hotplug_ready))
        return;

    /* free all registered hotplug callbacks */
    for_each_hotplug_cb_safe(ctx, hotplug_cb, next_cb) {
        list_del(&hotplug_cb->list);
        free(hotplug_cb);
    }

    /* free all pending hotplug messages */
    while(!list_empty(&ctx->hotplug_msgs)) {
        msg = list_first_entry(&ctx->hotplug_msgs, struct usbi_hotplug_message, list);

        /* if the device left, the message holds a reference
         * and we must drop it */
        if(msg->event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT)
            libusb_unref_device(msg->device);

        list_del(&msg->list);
        free(msg);
    }

    /* free all discovered devices. due to parent references loop until no devices are freed. */
    for_each_device_safe(ctx, dev, next_dev) {
        /* remove the device from the usb_devs list only if there are no
         * references held, otherwise leave it on the list so that a
         * warning message will be shown */
        if(usbi_atomic_load(&dev->refcnt) == 1) {
            list_del(&dev->list);
        }
        if(dev->parent_dev && usbi_atomic_load(&dev->parent_dev->refcnt) == 1) {
            /* the parent was before this device in the list and will be released.
               remove it from the list. this is safe as parent_dev can not be
               equal to next_dev. */
            assert(dev->parent_dev != next_dev);
            list_del(&dev->parent_dev->list);
        }
        libusb_unref_device(dev);
    }

    usbi_mutex_destroy(&ctx->hotplug_cbs_lock);
}

static int usbi_hotplug_match_cb(struct libusb_device *dev, libusb_hotplug_event event, struct usbi_hotplug_callback *hotplug_cb) {
    if(!(hotplug_cb->flags & event)) {
        return 0;
    }

    if((hotplug_cb->flags & USBI_HOTPLUG_VENDOR_ID_VALID) && hotplug_cb->vendor_id != dev->device_descriptor.idVendor) {
        return 0;
    }

    if((hotplug_cb->flags & USBI_HOTPLUG_PRODUCT_ID_VALID) && hotplug_cb->product_id != dev->device_descriptor.idProduct) {
        return 0;
    }

    if((hotplug_cb->flags & USBI_HOTPLUG_DEV_CLASS_VALID) && hotplug_cb->dev_class != dev->device_descriptor.bDeviceClass) {
        return 0;
    }

    return hotplug_cb->cb(DEVICE_CTX(dev), dev, event, hotplug_cb->user_data);
}

void usbi_hotplug_notification(struct libusb_context *ctx, struct libusb_device *dev, libusb_hotplug_event event) {
    struct usbi_hotplug_message *msg;
    unsigned int                 event_flags;

    /* Only generate a notification if hotplug is ready. This prevents hotplug
     * notifications from being generated during initial enumeration or if the
     * backend does not support hotplug. */
    if(!usbi_atomic_load(&ctx->hotplug_ready))
        return;

    msg = calloc(1, sizeof(*msg));
    if(!msg) {
        usbi_err(ctx, "error allocating hotplug message");
        return;
    }

    msg->event  = event;
    msg->device = dev;

    /* Take the event data lock and add this message to the list.
     * Only signal an event if there are no prior pending events. */
    usbi_mutex_lock(&ctx->event_data_lock);
    event_flags = ctx->event_flags;
    ctx->event_flags |= USBI_EVENT_HOTPLUG_MSG_PENDING;
    list_add_tail(&msg->list, &ctx->hotplug_msgs);
    if(!event_flags)
        usbi_signal_event(&ctx->event);
    usbi_mutex_unlock(&ctx->event_data_lock);
}

void usbi_hotplug_process(struct libusb_context *ctx, struct list_head *hotplug_msgs) {
    struct usbi_hotplug_callback *hotplug_cb, *next_cb;
    struct usbi_hotplug_message  *msg;
    int                           r;

    usbi_mutex_lock(&ctx->hotplug_cbs_lock);

    /* dispatch all pending hotplug messages */
    while(!list_empty(hotplug_msgs)) {
        msg = list_first_entry(hotplug_msgs, struct usbi_hotplug_message, list);

        for_each_hotplug_cb_safe(ctx, hotplug_cb, next_cb) {
            /* skip callbacks that have unregistered */
            if(hotplug_cb->flags & USBI_HOTPLUG_NEEDS_FREE)
                continue;

            usbi_mutex_unlock(&ctx->hotplug_cbs_lock);
            r = usbi_hotplug_match_cb(msg->device, msg->event, hotplug_cb);
            usbi_mutex_lock(&ctx->hotplug_cbs_lock);

            if(r) {
                list_del(&hotplug_cb->list);
                free(hotplug_cb);
            }
        }

        /* if the device left, the message holds a reference
         * and we must drop it */
        if(msg->event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT)
            libusb_unref_device(msg->device);

        list_del(&msg->list);
        free(msg);
    }

    /* free any callbacks that have unregistered */
    for_each_hotplug_cb_safe(ctx, hotplug_cb, next_cb) {
        if(hotplug_cb->flags & USBI_HOTPLUG_NEEDS_FREE) {
            usbi_dbg(ctx, "freeing hotplug cb %p with handle %d", (void *)hotplug_cb, hotplug_cb->handle);
            list_del(&hotplug_cb->list);
            free(hotplug_cb);
        }
    }

    usbi_mutex_unlock(&ctx->hotplug_cbs_lock);
}

int API_EXPORTED libusb_hotplug_register_callback(libusb_context *ctx, int events, int flags, int vendor_id, int product_id, int dev_class,
                                                  libusb_hotplug_callback_fn cb_fn, void *user_data, libusb_hotplug_callback_handle *callback_handle) {
    struct usbi_hotplug_callback *hotplug_cb;

    /* check for sane values */
    if(!events || (~VALID_HOTPLUG_EVENTS & events) || (~VALID_HOTPLUG_FLAGS & flags) || (LIBUSB_HOTPLUG_MATCH_ANY != vendor_id && (~0xffff & vendor_id))
       || (LIBUSB_HOTPLUG_MATCH_ANY != product_id && (~0xffff & product_id)) || (LIBUSB_HOTPLUG_MATCH_ANY != dev_class && (~0xff & dev_class)) || !cb_fn) {
        return LIBUSB_ERROR_INVALID_PARAM;
    }

    /* check for hotplug support */
    if(!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG))
        return LIBUSB_ERROR_NOT_SUPPORTED;

    ctx = usbi_get_context(ctx);

    hotplug_cb = calloc(1, sizeof(*hotplug_cb));
    if(!hotplug_cb)
        return LIBUSB_ERROR_NO_MEM;

    hotplug_cb->flags = (uint8_t)events;
    if(LIBUSB_HOTPLUG_MATCH_ANY != vendor_id) {
        hotplug_cb->flags |= USBI_HOTPLUG_VENDOR_ID_VALID;
        hotplug_cb->vendor_id = (uint16_t)vendor_id;
    }
    if(LIBUSB_HOTPLUG_MATCH_ANY != product_id) {
        hotplug_cb->flags |= USBI_HOTPLUG_PRODUCT_ID_VALID;
        hotplug_cb->product_id = (uint16_t)product_id;
    }
    if(LIBUSB_HOTPLUG_MATCH_ANY != dev_class) {
        hotplug_cb->flags |= USBI_HOTPLUG_DEV_CLASS_VALID;
        hotplug_cb->dev_class = (uint8_t)dev_class;
    }
    hotplug_cb->cb        = cb_fn;
    hotplug_cb->user_data = user_data;

    usbi_mutex_lock(&ctx->hotplug_cbs_lock);

    /* protect the handle by the context hotplug lock */
    hotplug_cb->handle = ctx->next_hotplug_cb_handle++;

    /* handle the unlikely case of overflow */
    if(ctx->next_hotplug_cb_handle < 0)
        ctx->next_hotplug_cb_handle = 1;

    list_add(&hotplug_cb->list, &ctx->hotplug_cbs);

    usbi_mutex_unlock(&ctx->hotplug_cbs_lock);

    usbi_dbg(ctx, "new hotplug cb %p with handle %d", (void *)hotplug_cb, hotplug_cb->handle);

    if((flags & LIBUSB_HOTPLUG_ENUMERATE) && (events & LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED)) {
        ssize_t                i, len;
        struct libusb_device **devs;

        len = libusb_get_device_list(ctx, &devs);
        if(len < 0) {
            libusb_hotplug_deregister_callback(ctx, hotplug_cb->handle);
            return (int)len;
        }

        for(i = 0; i < len; i++) {
            usbi_hotplug_match_cb(devs[i], LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, hotplug_cb);
        }

        libusb_free_device_list(devs, 1);
    }

    if(callback_handle)
        *callback_handle = hotplug_cb->handle;

    return LIBUSB_SUCCESS;
}

void API_EXPORTED libusb_hotplug_deregister_callback(libusb_context *ctx, libusb_hotplug_callback_handle callback_handle) {
    struct usbi_hotplug_callback *hotplug_cb;
    int                           deregistered = 0;

    /* check for hotplug support */
    if(!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG))
        return;

    usbi_dbg(ctx, "deregister hotplug cb %d", callback_handle);

    ctx = usbi_get_context(ctx);

    usbi_mutex_lock(&ctx->hotplug_cbs_lock);
    for_each_hotplug_cb(ctx, hotplug_cb) {
        if(callback_handle == hotplug_cb->handle) {
            /* mark this callback for deregistration */
            hotplug_cb->flags |= USBI_HOTPLUG_NEEDS_FREE;
            deregistered = 1;
            break;
        }
    }
    usbi_mutex_unlock(&ctx->hotplug_cbs_lock);

    if(deregistered) {
        unsigned int event_flags;

        usbi_mutex_lock(&ctx->event_data_lock);
        event_flags = ctx->event_flags;
        ctx->event_flags |= USBI_EVENT_HOTPLUG_CB_DEREGISTERED;
        if(!event_flags)
            usbi_signal_event(&ctx->event);
        usbi_mutex_unlock(&ctx->event_data_lock);
    }
}

DEFAULT_VISIBILITY
void *LIBUSB_CALL libusb_hotplug_get_user_data(libusb_context *ctx, libusb_hotplug_callback_handle callback_handle) {
    struct usbi_hotplug_callback *hotplug_cb;
    void                         *user_data = NULL;

    /* check for hotplug support */
    if(!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG))
        return NULL;

    usbi_dbg(ctx, "get hotplug cb %d user data", callback_handle);

    ctx = usbi_get_context(ctx);

    usbi_mutex_lock(&ctx->hotplug_cbs_lock);
    for_each_hotplug_cb(ctx, hotplug_cb) {
        if(callback_handle == hotplug_cb->handle) {
            user_data = hotplug_cb->user_data;
            break;
        }
    }
    usbi_mutex_unlock(&ctx->hotplug_cbs_lock);

    return user_data;
}
