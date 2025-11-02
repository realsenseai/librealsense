/* -*- Mode: C; indent-tabs-mode:nil -*- */
/*
 * darwin backend for libusb 1.0
 * Copyright © 2008-2021 Nathan Hjelm <hjelmn@cs.unm.edu>
 * Copyright © 2019-2022 Google LLC. All rights reserved.
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
#include <assert.h>
#include <time.h>
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/sysctl.h>

#include <mach/mach_time.h>

/* Suppress warnings about the use of the deprecated objc_registerThreadWithCollector
 * function. Its use is also conditionalized to only older deployment targets. */
#define OBJC_SILENCE_GC_DEPRECATIONS 1

/* Default timeout to 10s for reenumerate. This is needed because USBDeviceReEnumerate
 * does not return error status on macOS. */
#define DARWIN_REENUMERATE_TIMEOUT_US (10 * USEC_PER_SEC)

#include <AvailabilityMacros.h>
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060 && MAC_OS_X_VERSION_MIN_REQUIRED < 101200
#include <objc/objc-auto.h>
#endif

#include "darwin_usb.h"

static int init_count = 0;

/* Both kIOMasterPortDefault or kIOMainPortDefault are synonyms for 0. */
static const mach_port_t darwin_default_master_port = 0;

/* async event thread */
/* if both this mutex and darwin_cached_devices_mutex are to be acquired then
   darwin_cached_devices_mutex must be acquired first. */
static pthread_mutex_t libusb_darwin_at_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  libusb_darwin_at_cond  = PTHREAD_COND_INITIALIZER;

#define LIBUSB_DARWIN_STARTUP_FAILURE ((CFRunLoopRef)-1)

static CFRunLoopRef       libusb_darwin_acfl  = NULL; /* event cf loop */
static CFRunLoopSourceRef libusb_darwin_acfls = NULL; /* shutdown signal for event cf loop */

static usbi_mutex_t     darwin_cached_devices_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct list_head darwin_cached_devices;
static const char      *darwin_device_class = "IOUSBDevice";

#define DARWIN_CACHED_DEVICE(a) (((struct darwin_device_priv *)usbi_get_device_priv((a)))->dev)

/* async event thread */
static pthread_t libusb_darwin_at;

static void darwin_exit(struct libusb_context *ctx);
static int  darwin_get_config_descriptor(struct libusb_device *dev, uint8_t config_index, void *buffer, size_t len);
static int  darwin_claim_interface(struct libusb_device_handle *dev_handle, uint8_t iface);
static int  darwin_release_interface(struct libusb_device_handle *dev_handle, uint8_t iface);
static int  darwin_reenumerate_device(struct libusb_device_handle *dev_handle, bool capture);
static int  darwin_clear_halt(struct libusb_device_handle *dev_handle, unsigned char endpoint);
static int  darwin_reset_device(struct libusb_device_handle *dev_handle);
static int  darwin_detach_kernel_driver(struct libusb_device_handle *dev_handle, uint8_t interface);
static void darwin_async_io_callback(void *refcon, IOReturn result, void *arg0);

static enum libusb_error darwin_scan_devices(struct libusb_context *ctx);
static enum libusb_error process_new_device(struct libusb_context *ctx, struct darwin_cached_device *cached_device, UInt64 old_session_id);

static enum libusb_error darwin_get_cached_device(struct libusb_context *ctx, io_service_t service, struct darwin_cached_device **cached_out,
                                                  UInt64 *old_session_id);

#if defined(ENABLE_LOGGING)
static const char *darwin_error_str(IOReturn result) {
    static char string_buffer[50];
    switch(result) {
    case kIOReturnSuccess:
        return "no error";
    case kIOReturnNotOpen:
        return "device not opened for exclusive access";
    case kIOReturnNoDevice:
        return "no connection to an IOService";
    case kIOUSBNoAsyncPortErr:
        return "no async port has been opened for interface";
    case kIOReturnExclusiveAccess:
        return "another process has device opened for exclusive access";
    case kIOUSBPipeStalled:
#if defined(kUSBHostReturnPipeStalled)
    case kUSBHostReturnPipeStalled:
#endif
        return "pipe is stalled";
    case kIOReturnError:
        return "could not establish a connection to the Darwin kernel";
    case kIOUSBTransactionTimeout:
        return "transaction timed out";
    case kIOReturnBadArgument:
        return "invalid argument";
    case kIOReturnAborted:
        return "transaction aborted";
    case kIOReturnNotResponding:
        return "device not responding";
    case kIOReturnOverrun:
        return "data overrun";
    case kIOReturnCannotWire:
        return "physical memory can not be wired down";
    case kIOReturnNoResources:
        return "out of resources";
    case kIOUSBHighSpeedSplitError:
        return "high speed split error";
    case kIOUSBUnknownPipeErr:
        return "pipe ref not recognized";
    default:
        snprintf(string_buffer, sizeof(string_buffer), "unknown error (0x%x)", result);
        return string_buffer;
    }
}
#endif

static enum libusb_error darwin_to_libusb(IOReturn result) {
    switch(result) {
    case kIOReturnUnderrun:
    case kIOReturnSuccess:
        return LIBUSB_SUCCESS;
    case kIOReturnNotOpen:
    case kIOReturnNoDevice:
        return LIBUSB_ERROR_NO_DEVICE;
    case kIOReturnExclusiveAccess:
        return LIBUSB_ERROR_ACCESS;
    case kIOUSBPipeStalled:
#if defined(kUSBHostReturnPipeStalled)
    case kUSBHostReturnPipeStalled:
#endif
        return LIBUSB_ERROR_PIPE;
    case kIOReturnBadArgument:
        return LIBUSB_ERROR_INVALID_PARAM;
    case kIOUSBTransactionTimeout:
        return LIBUSB_ERROR_TIMEOUT;
    case kIOUSBUnknownPipeErr:
        return LIBUSB_ERROR_NOT_FOUND;
    case kIOReturnNotResponding:
    case kIOReturnAborted:
    case kIOReturnError:
    case kIOUSBNoAsyncPortErr:
    default:
        return LIBUSB_ERROR_OTHER;
    }
}

/* this function must be called with the darwin_cached_devices_mutex held */
static void darwin_deref_cached_device(struct darwin_cached_device *cached_dev) {
    cached_dev->refcount--;
    /* free the device and remove it from the cache */
    if(0 == cached_dev->refcount) {
        list_del(&cached_dev->list);

        if(cached_dev->device) {
            (*(cached_dev->device))->Release(cached_dev->device);
            cached_dev->device = NULL;
        }
        IOObjectRelease(cached_dev->service);
        free(cached_dev);
    }
}

static void darwin_ref_cached_device(struct darwin_cached_device *cached_dev) {
    cached_dev->refcount++;
}

static int ep_to_pipeRef(struct libusb_device_handle *dev_handle, uint8_t ep, uint8_t *pipep, uint8_t *ifcp, struct darwin_interface **interface_out) {
    struct darwin_device_handle_priv *priv = usbi_get_device_handle_priv(dev_handle);

    /* current interface */
    struct darwin_interface *cInterface;

    uint8_t i, iface;

    struct libusb_context *ctx = HANDLE_CTX(dev_handle);

    usbi_dbg(ctx, "converting ep address 0x%02x to pipeRef and interface", ep);

    for(iface = 0; iface < USB_MAXINTERFACES; iface++) {
        cInterface = &priv->interfaces[iface];

        if(dev_handle->claimed_interfaces & (1U << iface)) {
            for(i = 0; i < cInterface->num_endpoints; i++) {
                if(cInterface->endpoint_addrs[i] == ep) {
                    *pipep = i + 1;

                    if(ifcp)
                        *ifcp = iface;

                    if(interface_out)
                        *interface_out = cInterface;

                    usbi_dbg(ctx, "pipe %d on interface %d matches", *pipep, iface);
                    return LIBUSB_SUCCESS;
                }
            }
        }
    }

    /* No pipe found with the correct endpoint address */
    usbi_warn(HANDLE_CTX(dev_handle), "no pipeRef found with endpoint address 0x%02x.", ep);

    return LIBUSB_ERROR_NOT_FOUND;
}

static IOReturn usb_setup_device_iterator(io_iterator_t *deviceIterator, UInt32 location) {
    CFMutableDictionaryRef matchingDict = IOServiceMatching(darwin_device_class);

    if(!matchingDict)
        return kIOReturnError;

    if(location) {
        CFMutableDictionaryRef propertyMatchDict =
            CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

        /* there are no unsigned CFNumber types so treat the value as signed. the OS seems to do this
             internally (CFNumberType of locationID is kCFNumberSInt32Type) */
        CFTypeRef locationCF = CFNumberCreate(NULL, kCFNumberSInt32Type, &location);

        if(propertyMatchDict && locationCF) {
            CFDictionarySetValue(propertyMatchDict, CFSTR(kUSBDevicePropertyLocationID), locationCF);
            CFDictionarySetValue(matchingDict, CFSTR(kIOPropertyMatchKey), propertyMatchDict);
        }
        /* else we can still proceed as long as the caller accounts for the possibility of other devices in the iterator */

        /* release our references as per the Create Rule */
        if(propertyMatchDict)
            CFRelease(propertyMatchDict);
        if(locationCF)
            CFRelease(locationCF);
    }

    return IOServiceGetMatchingServices(darwin_default_master_port, matchingDict, deviceIterator);
}

/* Returns 1 on success, 0 on failure. */
static bool get_ioregistry_value_number(io_service_t service, CFStringRef property, CFNumberType type, void *p) {
    CFTypeRef cfNumber = IORegistryEntryCreateCFProperty(service, property, kCFAllocatorDefault, 0);
    Boolean   success  = 0;

    if(cfNumber) {
        if(CFGetTypeID(cfNumber) == CFNumberGetTypeID()) {
            success = CFNumberGetValue(cfNumber, type, p);
        }

        CFRelease(cfNumber);
    }

    return (success != 0);
}

/* Returns 1 on success, 0 on failure. */
static bool get_ioregistry_value_data(io_service_t service, CFStringRef property, ssize_t size, void *p) {
    CFTypeRef cfData  = IORegistryEntryCreateCFProperty(service, property, kCFAllocatorDefault, 0);
    bool      success = false;

    if(cfData) {
        if(CFGetTypeID(cfData) == CFDataGetTypeID()) {
            CFIndex length = CFDataGetLength(cfData);
            if(length < size) {
                size = length;
            }

            CFDataGetBytes(cfData, CFRangeMake(0, size), p);
            success = true;
        }

        CFRelease(cfData);
    }

    return success;
}

static usb_device_t **darwin_device_from_service(struct libusb_context *ctx, io_service_t service) {
    io_cf_plugin_ref_t *plugInInterface = NULL;
    usb_device_t      **device;
    IOReturn            kresult;
    SInt32              score;
    const int           max_retries = 5;

    /* The IOCreatePlugInInterfaceForService function might consistently return
       an "out of resources" error with certain USB devices the first time we run
       it. The reason is still unclear, but retrying fixes the problem */
    for(int count = 0; count < max_retries; count++) {
        kresult = IOCreatePlugInInterfaceForService(service, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plugInInterface, &score);
        if(kIOReturnSuccess == kresult && plugInInterface) {
            break;
        }

        usbi_dbg(ctx, "set up plugin for service retry: %s", darwin_error_str(kresult));

        /* sleep for a little while before trying again */
        nanosleep(&(struct timespec){ .tv_sec = 0, .tv_nsec = 1000 }, NULL);
    }

    if(kIOReturnSuccess != kresult || !plugInInterface) {
        usbi_dbg(ctx, "could not set up plugin for service: %s", darwin_error_str(kresult));
        return NULL;
    }

    (void)(*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(DeviceInterfaceID), (LPVOID)&device);
    /* Use release instead of IODestroyPlugInInterface to avoid stopping IOServices associated with this device */
    (*plugInInterface)->Release(plugInInterface);

    return device;
}

static void darwin_devices_attached(void *ptr, io_iterator_t add_devices) {
    UNUSED(ptr);
    struct darwin_cached_device *cached_device;
    UInt64                       old_session_id;
    struct libusb_context       *ctx;
    io_service_t                 service;
    int                          ret;

    usbi_mutex_lock(&active_contexts_lock);

    while((service = IOIteratorNext(add_devices))) {
        ret = darwin_get_cached_device(NULL, service, &cached_device, &old_session_id);
        if(ret < 0 || !cached_device->can_enumerate) {
            continue;
        }

        /* add this device to each active context's device list */
        for_each_context(ctx) {
            process_new_device(ctx, cached_device, old_session_id);
        }

        if(cached_device->in_reenumerate) {
            usbi_dbg(NULL, "cached device in reset state. reset complete...");
            cached_device->in_reenumerate = false;
        }

        IOObjectRelease(service);
    }

    usbi_mutex_unlock(&active_contexts_lock);
}

static void darwin_devices_detached(void *ptr, io_iterator_t rem_devices) {
    UNUSED(ptr);
    struct libusb_device        *dev = NULL;
    struct libusb_context       *ctx;
    struct darwin_cached_device *old_device;

    io_service_t device;
    UInt64       session, locationID;
    int          ret;

    usbi_mutex_lock(&active_contexts_lock);

    while((device = IOIteratorNext(rem_devices)) != 0) {
        bool is_reenumerating = false;

        /* get the location from the i/o registry */
        ret = get_ioregistry_value_number(device, CFSTR("sessionID"), kCFNumberSInt64Type, &session);
        (void)get_ioregistry_value_number(device, CFSTR("locationID"), kCFNumberSInt32Type, &locationID);
        IOObjectRelease(device);
        if(!ret)
            continue;

        /* we need to match darwin_ref_cached_device call made in darwin_get_cached_device function
           otherwise no cached device will ever get freed */
        usbi_mutex_lock(&darwin_cached_devices_mutex);
        list_for_each_entry(old_device, &darwin_cached_devices, list, struct darwin_cached_device) {
            if(old_device->session == session) {
                if(old_device->in_reenumerate) {
                    /* device is re-enumerating. do not dereference the device at this time. libusb_reset_device()
                     * will deref if needed. */
                    usbi_dbg(NULL, "detected device detached due to re-enumeration. sessionID: 0x%" PRIx64 ", locationID: 0x%" PRIx64, session, locationID);

                    /* the device object is no longer usable so go ahead and release it */
                    if(old_device->device) {
                        (*(old_device->device))->Release(old_device->device);
                        old_device->device = NULL;
                    }

                    is_reenumerating = true;
                }
                else {
                    darwin_deref_cached_device(old_device);
                }

                break;
            }
        }

        usbi_mutex_unlock(&darwin_cached_devices_mutex);
        if(is_reenumerating) {
            continue;
        }

        for_each_context(ctx) {
            usbi_dbg(ctx, "notifying context %p of device disconnect", ctx);

            dev = usbi_get_device_by_session_id(ctx, (unsigned long)session);
            if(dev) {
                /* signal the core that this device has been disconnected. the core will tear down this device
                   when the reference count reaches 0 */
                usbi_disconnect_device(dev);
                libusb_unref_device(dev);
            }
        }
    }

    usbi_mutex_unlock(&active_contexts_lock);
}

static void darwin_hotplug_poll(void) {
    /* not sure if 1 ms will be too long/short but it should work ok */
    mach_timespec_t timeout = { .tv_sec = 0, .tv_nsec = 1000000ul };

    /* since a kernel thread may notify the IOIterators used for
     * hotplug notification we can't just clear the iterators.
     * instead just wait until all IOService providers are quiet */
    (void)IOKitWaitQuiet(darwin_default_master_port, &timeout);
}

static void darwin_clear_iterator(io_iterator_t iter) {
    io_service_t device;

    while((device = IOIteratorNext(iter)) != 0)
        IOObjectRelease(device);
}

static void darwin_fail_startup(void) {
    pthread_mutex_lock(&libusb_darwin_at_mutex);
    libusb_darwin_acfl = LIBUSB_DARWIN_STARTUP_FAILURE;
    pthread_cond_signal(&libusb_darwin_at_cond);
    pthread_mutex_unlock(&libusb_darwin_at_mutex);
    pthread_exit(NULL);
}

static void *darwin_event_thread_main(void *arg0) {
    UNUSED(arg0);
    IOReturn               kresult;
    CFRunLoopRef           runloop;
    CFRunLoopSourceRef     libusb_shutdown_cfsource;
    CFRunLoopSourceContext libusb_shutdown_cfsourcectx;

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
    /* Set this thread's name, so it can be seen in the debugger
       and crash reports. */
    pthread_setname_np("org.libusb.device-hotplug");
#endif

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060 && MAC_OS_X_VERSION_MIN_REQUIRED < 101200
    /* Tell the Objective-C garbage collector about this thread.
       This is required because, unlike NSThreads, pthreads are
       not automatically registered. Although we don't use
       Objective-C, we use CoreFoundation, which does.
       Garbage collection support was entirely removed in 10.12,
       so don't bother there. */
    objc_registerThreadWithCollector();
#endif

    /* hotplug (device arrival/removal) sources */
    CFRunLoopSourceRef     libusb_notification_cfsource;
    io_notification_port_t libusb_notification_port;
    io_iterator_t          libusb_rem_device_iterator;
    io_iterator_t          libusb_add_device_iterator;

    /* ctx must only be used for logging during thread startup */
    usbi_dbg(NULL, "creating hotplug event source");

    runloop = CFRunLoopGetCurrent();
    CFRetain(runloop);

    /* add the shutdown cfsource to the run loop */
    memset(&libusb_shutdown_cfsourcectx, 0, sizeof(libusb_shutdown_cfsourcectx));
    libusb_shutdown_cfsourcectx.info    = runloop;
    libusb_shutdown_cfsourcectx.perform = (void (*)(void *))CFRunLoopStop;
    libusb_shutdown_cfsource            = CFRunLoopSourceCreate(NULL, 0, &libusb_shutdown_cfsourcectx);
    CFRunLoopAddSource(runloop, libusb_shutdown_cfsource, kCFRunLoopDefaultMode);

    /* add the notification port to the run loop */
    libusb_notification_port     = IONotificationPortCreate(darwin_default_master_port);
    libusb_notification_cfsource = IONotificationPortGetRunLoopSource(libusb_notification_port);
    CFRunLoopAddSource(runloop, libusb_notification_cfsource, kCFRunLoopDefaultMode);

    /* create notifications for removed devices */
    kresult = IOServiceAddMatchingNotification(libusb_notification_port, kIOTerminatedNotification, IOServiceMatching(darwin_device_class),
                                               darwin_devices_detached, NULL, &libusb_rem_device_iterator);

    if(kresult != kIOReturnSuccess) {
        usbi_err(NULL, "could not add hotplug event source: %s", darwin_error_str(kresult));
        CFRelease(libusb_shutdown_cfsource);
        CFRelease(runloop);
        darwin_fail_startup();
    }

    /* create notifications for attached devices */
    kresult = IOServiceAddMatchingNotification(libusb_notification_port, kIOFirstMatchNotification, IOServiceMatching(darwin_device_class),
                                               darwin_devices_attached, NULL, &libusb_add_device_iterator);

    if(kresult != kIOReturnSuccess) {
        usbi_err(NULL, "could not add hotplug event source: %s", darwin_error_str(kresult));
        CFRelease(libusb_shutdown_cfsource);
        CFRelease(runloop);
        darwin_fail_startup();
    }

    /* arm notifiers */
    darwin_clear_iterator(libusb_rem_device_iterator);
    darwin_clear_iterator(libusb_add_device_iterator);

    usbi_dbg(NULL, "darwin event thread ready to receive events");

    /* signal the main thread that the hotplug runloop has been created. */
    pthread_mutex_lock(&libusb_darwin_at_mutex);
    libusb_darwin_acfl  = runloop;
    libusb_darwin_acfls = libusb_shutdown_cfsource;
    pthread_cond_signal(&libusb_darwin_at_cond);
    pthread_mutex_unlock(&libusb_darwin_at_mutex);

    /* run the runloop */
    CFRunLoopRun();

    usbi_dbg(NULL, "darwin event thread exiting");

    /* signal the main thread that the hotplug runloop has finished. */
    pthread_mutex_lock(&libusb_darwin_at_mutex);
    libusb_darwin_acfls = NULL;
    libusb_darwin_acfl  = NULL;
    pthread_cond_signal(&libusb_darwin_at_cond);
    pthread_mutex_unlock(&libusb_darwin_at_mutex);

    /* remove the notification cfsource */
    CFRunLoopRemoveSource(runloop, libusb_notification_cfsource, kCFRunLoopDefaultMode);

    /* remove the shutdown cfsource */
    CFRunLoopRemoveSource(runloop, libusb_shutdown_cfsource, kCFRunLoopDefaultMode);

    /* delete notification port */
    IONotificationPortDestroy(libusb_notification_port);

    /* delete iterators */
    IOObjectRelease(libusb_rem_device_iterator);
    IOObjectRelease(libusb_add_device_iterator);

    CFRelease(libusb_shutdown_cfsource);
    CFRelease(runloop);

    pthread_exit(NULL);
}

/* cleanup function to destroy cached devices. must be called with a lock on darwin_cached_devices_mutex */
static void darwin_cleanup_devices(void) {
    struct darwin_cached_device *dev, *next;

    list_for_each_entry_safe(dev, next, &darwin_cached_devices, list, struct darwin_cached_device) {
        if(dev->refcount > 1) {
            usbi_err(NULL, "device still referenced at libusb_exit");
        }
        darwin_deref_cached_device(dev);
    }
}

/* must be called with a lock on darwin_cached_devices_mutex */
static int darwin_first_time_init(void) {
    if(NULL == darwin_cached_devices.next) {
        list_init(&darwin_cached_devices);
    }

    if(!list_empty(&darwin_cached_devices)) {
        usbi_err(NULL, "libusb_device reference not released on last exit. will not continue");
        return LIBUSB_ERROR_OTHER;
    }

    int rc = pthread_create(&libusb_darwin_at, NULL, darwin_event_thread_main, NULL);
    if(0 != rc) {
        usbi_err(NULL, "could not create event thread, error %d", rc);
        return LIBUSB_ERROR_OTHER;
    }

    pthread_mutex_lock(&libusb_darwin_at_mutex);
    while(NULL == libusb_darwin_acfl) {
        pthread_cond_wait(&libusb_darwin_at_cond, &libusb_darwin_at_mutex);
    }

    if(libusb_darwin_acfl == LIBUSB_DARWIN_STARTUP_FAILURE) {
        libusb_darwin_acfl = NULL;
        rc                 = LIBUSB_ERROR_OTHER;
    }
    pthread_mutex_unlock(&libusb_darwin_at_mutex);

    return rc;
}

static int darwin_init_context(struct libusb_context *ctx) {
    usbi_mutex_lock(&darwin_cached_devices_mutex);

    bool first_init = (1 == ++init_count);

    if(first_init) {
        int rc = darwin_first_time_init();
        if(LIBUSB_SUCCESS != rc) {
            usbi_mutex_unlock(&darwin_cached_devices_mutex);
            return rc;
        }
    }
    usbi_mutex_unlock(&darwin_cached_devices_mutex);

    return darwin_scan_devices(ctx);
}

static int darwin_init(struct libusb_context *ctx) {
    int rc = darwin_init_context(ctx);
    if(LIBUSB_SUCCESS != rc) {
        /* clean up any allocated resources */
        darwin_exit(ctx);
    }

    return rc;
}

static void darwin_exit(struct libusb_context *ctx) {
    UNUSED(ctx);

    usbi_mutex_lock(&darwin_cached_devices_mutex);
    if(0 == --init_count) {
        /* stop the event runloop and wait for the thread to terminate. */
        pthread_mutex_lock(&libusb_darwin_at_mutex);
        CFRunLoopSourceSignal(libusb_darwin_acfls);
        CFRunLoopWakeUp(libusb_darwin_acfl);
        while(libusb_darwin_acfl)
            pthread_cond_wait(&libusb_darwin_at_cond, &libusb_darwin_at_mutex);
        pthread_mutex_unlock(&libusb_darwin_at_mutex);
        pthread_join(libusb_darwin_at, NULL);

        darwin_cleanup_devices();
    }
    usbi_mutex_unlock(&darwin_cached_devices_mutex);
}

static int get_configuration_index(struct libusb_device *dev, UInt8 config_value) {
    struct darwin_cached_device    *priv = DARWIN_CACHED_DEVICE(dev);
    UInt8                           i, numConfig;
    IOUSBConfigurationDescriptorPtr desc;
    IOReturn                        kresult;

    /* is there a simpler way to determine the index? */
    kresult = (*(priv->device))->GetNumberOfConfigurations(priv->device, &numConfig);
    if(kresult != kIOReturnSuccess)
        return darwin_to_libusb(kresult);

    for(i = 0; i < numConfig; i++) {
        (*(priv->device))->GetConfigurationDescriptorPtr(priv->device, i, &desc);

        if(desc->bConfigurationValue == config_value)
            return i;
    }

    /* configuration not found */
    return LIBUSB_ERROR_NOT_FOUND;
}

static int darwin_get_active_config_descriptor(struct libusb_device *dev, void *buffer, size_t len) {
    struct darwin_cached_device *priv = DARWIN_CACHED_DEVICE(dev);
    int                          config_index;

    if(0 == priv->active_config)
        return LIBUSB_ERROR_NOT_FOUND;

    config_index = get_configuration_index(dev, priv->active_config);
    if(config_index < 0)
        return config_index;

    assert(config_index >= 0 && config_index <= UINT8_MAX);
    return darwin_get_config_descriptor(dev, (UInt8)config_index, buffer, len);
}

static int darwin_get_config_descriptor(struct libusb_device *dev, uint8_t config_index, void *buffer, size_t len) {
    struct darwin_cached_device    *priv = DARWIN_CACHED_DEVICE(dev);
    IOUSBConfigurationDescriptorPtr desc;
    IOReturn                        kresult;
    int                             ret;

    if(!priv || !priv->device)
        return LIBUSB_ERROR_OTHER;

    kresult = (*priv->device)->GetConfigurationDescriptorPtr(priv->device, config_index, &desc);
    if(kresult == kIOReturnSuccess) {
        /* copy descriptor */
        if(libusb_le16_to_cpu(desc->wTotalLength) < len)
            len = libusb_le16_to_cpu(desc->wTotalLength);

        memmove(buffer, desc, len);
    }

    ret = darwin_to_libusb(kresult);
    if(ret != LIBUSB_SUCCESS)
        return ret;

    return (int)len;
}

/* check whether the os has configured the device */
static enum libusb_error darwin_check_configuration(struct libusb_context *ctx, struct darwin_cached_device *dev) {
    usb_device_t **darwin_device = dev->device;

    IOUSBConfigurationDescriptorPtr configDesc;
    IOUSBFindInterfaceRequest       request;
    IOReturn                        kresult;
    io_iterator_t                   interface_iterator;
    io_service_t                    firstInterface;

    if(dev->dev_descriptor.bNumConfigurations < 1) {
        usbi_err(ctx, "device has no configurations");
        return LIBUSB_ERROR_OTHER; /* no configurations at this speed so we can't use it */
    }

    /* checking the configuration of a root hub simulation takes ~1 s in 10.11. the device is
       not usable anyway */
    if(0x05ac == libusb_le16_to_cpu(dev->dev_descriptor.idVendor) && 0x8005 == libusb_le16_to_cpu(dev->dev_descriptor.idProduct)) {
        usbi_dbg(ctx, "ignoring configuration on root hub simulation");
        dev->active_config = 0;
        return LIBUSB_SUCCESS;
    }

    /* find the first configuration */
    kresult           = (*darwin_device)->GetConfigurationDescriptorPtr(darwin_device, 0, &configDesc);
    dev->first_config = (kIOReturnSuccess == kresult) ? configDesc->bConfigurationValue : 1;

    /* check if the device is already configured. there is probably a better way than iterating over the
       to accomplish this (the trick is we need to avoid a call to GetConfigurations since buggy devices
       might lock up on the device request) */

    /* Setup the Interface Request */
    request.bInterfaceClass    = kIOUSBFindInterfaceDontCare;
    request.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
    request.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
    request.bAlternateSetting  = kIOUSBFindInterfaceDontCare;

    kresult = (*(darwin_device))->CreateInterfaceIterator(darwin_device, &request, &interface_iterator);
    if(kresult != kIOReturnSuccess)
        return darwin_to_libusb(kresult);

    /* iterate once */
    firstInterface = IOIteratorNext(interface_iterator);

    /* done with the interface iterator */
    IOObjectRelease(interface_iterator);

    if(firstInterface) {
        IOObjectRelease(firstInterface);

        /* device is configured */
        if(dev->dev_descriptor.bNumConfigurations == 1)
            /* to avoid problems with some devices get the configurations value from the configuration descriptor */
            dev->active_config = dev->first_config;
        else
            /* devices with more than one configuration should work with GetConfiguration */
            (*darwin_device)->GetConfiguration(darwin_device, &dev->active_config);
    }
    else
        /* not configured */
        dev->active_config = 0;

    usbi_dbg(ctx, "active config: %u, first config: %u", dev->active_config, dev->first_config);

    return LIBUSB_SUCCESS;
}

static IOReturn darwin_request_descriptor(usb_device_t **device, UInt8 desc, UInt8 desc_index, void *buffer, size_t buffer_size) {
    IOUSBDevRequestTO req;

    assert(buffer_size <= UINT16_MAX);

    memset(buffer, 0, buffer_size);

    /* Set up request for descriptor/ */
    req.bmRequestType     = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
    req.bRequest          = kUSBRqGetDescriptor;
    req.wValue            = (UInt16)(desc << 8);
    req.wIndex            = desc_index;
    req.wLength           = (UInt16)buffer_size;
    req.pData             = buffer;
    req.noDataTimeout     = 20;
    req.completionTimeout = 100;

    return (*device)->DeviceRequestTO(device, &req);
}

static enum libusb_error darwin_cache_device_descriptor(struct libusb_context *ctx, struct darwin_cached_device *dev) {
    usb_device_t **device      = dev->device;
    int            retries     = 1;
    long           delay       = 30000;  // microseconds
    int            unsuspended = 0, try_unsuspend = 1, try_reconfigure = 1;
    int            is_open = 0;
    IOReturn       ret     = 0, ret2;
    UInt8          bDeviceClass;
    UInt16         idProduct, idVendor;

    dev->can_enumerate = 0;

    (*device)->GetDeviceClass(device, &bDeviceClass);
    (*device)->GetDeviceProduct(device, &idProduct);
    (*device)->GetDeviceVendor(device, &idVendor);

    /* According to Apple's documentation the device must be open for DeviceRequest but we may not be able to open some
     * devices and Apple's USB Prober doesn't bother to open the device before issuing a descriptor request.  Still,
     * to follow the spec as closely as possible, try opening the device */
    is_open = ((*device)->USBDeviceOpenSeize(device) == kIOReturnSuccess);

    do {
        /**** retrieve device descriptor ****/
        ret = darwin_request_descriptor(device, kUSBDeviceDesc, 0, &dev->dev_descriptor, sizeof(dev->dev_descriptor));

        if(kIOReturnOverrun == ret && kUSBDeviceDesc == dev->dev_descriptor.bDescriptorType)
            /* received an overrun error but we still received a device descriptor */
            ret = kIOReturnSuccess;

        if(kIOUSBVendorIDAppleComputer == idVendor) {
            /* NTH: don't bother retrying or unsuspending Apple devices */
            break;
        }

        if(kIOReturnSuccess == ret && (0 == dev->dev_descriptor.bNumConfigurations || 0 == dev->dev_descriptor.bcdUSB)) {
            /* work around for incorrectly configured devices */
            if(try_reconfigure && is_open) {
                usbi_dbg(ctx, "descriptor appears to be invalid. resetting configuration before trying again...");

                /* set the first configuration */
                (*device)->SetConfiguration(device, 1);

                /* don't try to reconfigure again */
                try_reconfigure = 0;
            }

            ret = kIOUSBPipeStalled;
        }

        if(kIOReturnSuccess != ret && is_open && try_unsuspend) {
            /* device may be suspended. unsuspend it and try again */
#if DeviceVersion >= 320
            UInt32 info = 0;

            /* IOUSBFamily 320+ provides a way to detect device suspension but earlier versions do not */
            (void)(*device)->GetUSBDeviceInformation(device, &info);

            /* note that the device was suspended */
            if(info & (1U << kUSBInformationDeviceIsSuspendedBit) || 0 == info)
                try_unsuspend = 1;
#endif

            if(try_unsuspend) {
                /* try to unsuspend the device */
                ret2 = (*device)->USBDeviceSuspend(device, 0);
                if(kIOReturnSuccess != ret2) {
                    /* prevent log spew from poorly behaving devices.  this indicates the
                       os actually had trouble communicating with the device */
                    usbi_dbg(ctx, "could not retrieve device descriptor. failed to unsuspend: %s", darwin_error_str(ret2));
                }
                else
                    unsuspended = 1;

                try_unsuspend = 0;
            }
        }

        if(kIOReturnSuccess != ret) {
            usbi_dbg(ctx, "kernel responded with code: 0x%08x. sleeping for %ld ms before trying again", ret, delay / 1000);
            /* sleep for a little while before trying again */
            nanosleep(&(struct timespec){ delay / 1000000, (delay * 1000) % 1000000000 }, NULL);
        }
    } while(kIOReturnSuccess != ret && retries--);

    if(unsuspended)
        /* resuspend the device */
        (void)(*device)->USBDeviceSuspend(device, 1);

    if(is_open)
        (void)(*device)->USBDeviceClose(device);

    if(ret != kIOReturnSuccess) {
        /* a debug message was already printed out for this error */
        if(LIBUSB_CLASS_HUB == bDeviceClass)
            usbi_dbg(ctx, "could not retrieve device descriptor %.4x:%.4x: %s (%x). skipping device", idVendor, idProduct, darwin_error_str(ret), ret);
        else
            usbi_warn(ctx, "could not retrieve device descriptor %.4x:%.4x: %s (%x). skipping device", idVendor, idProduct, darwin_error_str(ret), ret);
        return darwin_to_libusb(ret);
    }

    /* catch buggy hubs (which appear to be virtual). Apple's own USB prober has problems with these devices. */
    if(libusb_le16_to_cpu(dev->dev_descriptor.idProduct) != idProduct) {
        /* not a valid device */
        usbi_warn(NULL, "idProduct from iokit (%04x) does not match idProduct in descriptor (%04x). skipping device", idProduct,
                  libusb_le16_to_cpu(dev->dev_descriptor.idProduct));
        return LIBUSB_ERROR_NO_DEVICE;
    }

    usbi_dbg(ctx, "cached device descriptor:");
    usbi_dbg(ctx, "  bDescriptorType:    0x%02x", dev->dev_descriptor.bDescriptorType);
    usbi_dbg(ctx, "  bcdUSB:             0x%04x", libusb_le16_to_cpu(dev->dev_descriptor.bcdUSB));
    usbi_dbg(ctx, "  bDeviceClass:       0x%02x", dev->dev_descriptor.bDeviceClass);
    usbi_dbg(ctx, "  bDeviceSubClass:    0x%02x", dev->dev_descriptor.bDeviceSubClass);
    usbi_dbg(ctx, "  bDeviceProtocol:    0x%02x", dev->dev_descriptor.bDeviceProtocol);
    usbi_dbg(ctx, "  bMaxPacketSize0:    0x%02x", dev->dev_descriptor.bMaxPacketSize0);
    usbi_dbg(ctx, "  idVendor:           0x%04x", libusb_le16_to_cpu(dev->dev_descriptor.idVendor));
    usbi_dbg(ctx, "  idProduct:          0x%04x", libusb_le16_to_cpu(dev->dev_descriptor.idProduct));
    usbi_dbg(ctx, "  bcdDevice:          0x%04x", libusb_le16_to_cpu(dev->dev_descriptor.bcdDevice));
    usbi_dbg(ctx, "  iManufacturer:      0x%02x", dev->dev_descriptor.iManufacturer);
    usbi_dbg(ctx, "  iProduct:           0x%02x", dev->dev_descriptor.iProduct);
    usbi_dbg(ctx, "  iSerialNumber:      0x%02x", dev->dev_descriptor.iSerialNumber);
    usbi_dbg(ctx, "  bNumConfigurations: 0x%02x", dev->dev_descriptor.bNumConfigurations);

    dev->can_enumerate = 1;

    return LIBUSB_SUCCESS;
}

/* Returns 1 on success, 0 on failure. */
static bool get_device_port(io_service_t service, UInt8 *port) {
    IOReturn     kresult;
    io_service_t parent;
    bool         ret = false;

    if(get_ioregistry_value_number(service, CFSTR("PortNum"), kCFNumberSInt8Type, port)) {
        return true;
    }

    kresult = IORegistryEntryGetParentEntry(service, kIOServicePlane, &parent);
    if(kIOReturnSuccess == kresult) {
        ret = get_ioregistry_value_data(parent, CFSTR("port"), 1, port);
        IOObjectRelease(parent);
    }

    return ret;
}

/* Returns 1 on success, 0 on failure. */
static bool get_device_parent_sessionID(io_service_t service, UInt64 *parent_sessionID) {
    IOReturn     kresult;
    io_service_t parent;

    /* Walk up the tree in the IOService plane until we find a parent that has a sessionID */
    parent = service;
    while((kresult = IORegistryEntryGetParentEntry(parent, kIOUSBPlane, &parent)) == kIOReturnSuccess) {
        if(get_ioregistry_value_number(parent, CFSTR("sessionID"), kCFNumberSInt64Type, parent_sessionID)) {
            /* Success */
            return true;
        }
    }

    /* We ran out of parents */
    return false;
}

static enum libusb_error darwin_get_cached_device(struct libusb_context *ctx, io_service_t service, struct darwin_cached_device **cached_out,
                                                  UInt64 *old_session_id) {
    struct darwin_cached_device *new_device;
    UInt64                       sessionID = 0, parent_sessionID = 0;
    UInt32                       locationID = 0;
    enum libusb_error            ret        = LIBUSB_SUCCESS;
    usb_device_t               **device;
    UInt8                        port = 0;

    /* assuming sessionID != 0 normally (never seen it be 0) */
    *old_session_id = 0;
    *cached_out     = NULL;

    /* get some info from the io registry */
    (void)get_ioregistry_value_number(service, CFSTR("sessionID"), kCFNumberSInt64Type, &sessionID);
    (void)get_ioregistry_value_number(service, CFSTR("locationID"), kCFNumberSInt32Type, &locationID);
    if(!get_device_port(service, &port)) {
        usbi_dbg(ctx, "could not get connected port number");
    }

    usbi_dbg(ctx, "finding cached device for sessionID 0x%" PRIx64, sessionID);

    if(get_device_parent_sessionID(service, &parent_sessionID)) {
        usbi_dbg(ctx, "parent sessionID: 0x%" PRIx64, parent_sessionID);
    }

    usbi_mutex_lock(&darwin_cached_devices_mutex);
    do {
        list_for_each_entry(new_device, &darwin_cached_devices, list, struct darwin_cached_device) {
            usbi_dbg(ctx, "matching sessionID/locationID 0x%" PRIx64 "/0x%x against cached device with sessionID/locationID 0x%" PRIx64 "/0x%x", sessionID,
                     locationID, new_device->session, new_device->location);
            if(new_device->location == locationID && new_device->in_reenumerate) {
                usbi_dbg(ctx, "found cached device with matching location that is being re-enumerated");
                *old_session_id = new_device->session;
                break;
            }

            if(new_device->session == sessionID) {
                usbi_dbg(ctx, "using cached device for device");
                *cached_out = new_device;
                break;
            }
        }

        if(*cached_out)
            break;

        usbi_dbg(ctx, "caching new device with sessionID 0x%" PRIx64, sessionID);

        device = darwin_device_from_service(ctx, service);
        if(!device) {
            ret = LIBUSB_ERROR_NO_DEVICE;
            break;
        }

        if(!(*old_session_id)) {
            new_device = calloc(1, sizeof(*new_device));
            if(!new_device) {
                ret = LIBUSB_ERROR_NO_MEM;
                break;
            }

            /* add this device to the cached device list */
            list_add(&new_device->list, &darwin_cached_devices);

            (*device)->GetDeviceAddress(device, (USBDeviceAddress *)&new_device->address);

            /* keep a reference to this device */
            darwin_ref_cached_device(new_device);

            (*device)->GetLocationID(device, &new_device->location);
            new_device->port           = port;
            new_device->parent_session = parent_sessionID;
        }
        else {
            /* release the ref to old device's service */
            IOObjectRelease(new_device->service);
        }

        /* keep track of devices regardless of if we successfully enumerate them to
           prevent them from being enumerated multiple times */
        *cached_out = new_device;

        new_device->session = sessionID;
        new_device->device  = device;
        new_device->service = service;

        /* retain the service */
        IOObjectRetain(service);

        /* cache the device descriptor */
        ret = darwin_cache_device_descriptor(ctx, new_device);
        if(ret)
            break;

        if(new_device->can_enumerate) {
            snprintf(new_device->sys_path, 20, "%03i-%04x-%04x-%02x-%02x", new_device->address, libusb_le16_to_cpu(new_device->dev_descriptor.idVendor),
                     libusb_le16_to_cpu(new_device->dev_descriptor.idProduct), new_device->dev_descriptor.bDeviceClass,
                     new_device->dev_descriptor.bDeviceSubClass);
        }
    } while(0);

    usbi_mutex_unlock(&darwin_cached_devices_mutex);

    return ret;
}

static enum libusb_error process_new_device(struct libusb_context *ctx, struct darwin_cached_device *cached_device, UInt64 old_session_id) {
    struct darwin_device_priv *priv;
    struct libusb_device      *dev = NULL;
    UInt8                      devSpeed;
    enum libusb_error          ret = LIBUSB_SUCCESS;

    do {
        /* check current active configuration (and cache the first configuration value--
           which may be used by claim_interface) */
        ret = darwin_check_configuration(ctx, cached_device);
        if(ret)
            break;

        if(0 != old_session_id) {
            usbi_dbg(ctx, "re-using existing device from context %p for with session 0x%" PRIx64 " new session 0x%" PRIx64, ctx, old_session_id,
                     cached_device->session);
            /* save the libusb device before the session id is updated */
            dev = usbi_get_device_by_session_id(ctx, (unsigned long)old_session_id);
        }

        if(!dev) {
            usbi_dbg(ctx, "allocating new device in context %p for with session 0x%" PRIx64, ctx, cached_device->session);

            dev = usbi_alloc_device(ctx, (unsigned long)cached_device->session);
            if(!dev) {
                return LIBUSB_ERROR_NO_MEM;
            }

            priv = usbi_get_device_priv(dev);

            priv->dev = cached_device;
            darwin_ref_cached_device(priv->dev);
            dev->port_number = cached_device->port;
            /* the location ID encodes the path to the device. the top byte of the location ID contains the bus number
               (numbered from 0). the remaining bytes can be used to construct the device tree for that bus. */
            dev->bus_number = cached_device->location >> 24;
            assert(cached_device->address <= UINT8_MAX);
            dev->device_address = (uint8_t)cached_device->address;
        }
        else {
            priv = usbi_get_device_priv(dev);
        }

        static_assert(sizeof(dev->device_descriptor) == sizeof(cached_device->dev_descriptor), "mismatch between libusb and IOKit device descriptor sizes");
        memcpy(&dev->device_descriptor, &cached_device->dev_descriptor, LIBUSB_DT_DEVICE_SIZE);
        usbi_localize_device_descriptor(&dev->device_descriptor);
        dev->session_data = cached_device->session;

        if(NULL != dev->parent_dev) {
            libusb_unref_device(dev->parent_dev);
            dev->parent_dev = NULL;
        }

        if(cached_device->parent_session > 0) {
            dev->parent_dev = usbi_get_device_by_session_id(ctx, (unsigned long)cached_device->parent_session);
        }

        (*(priv->dev->device))->GetDeviceSpeed(priv->dev->device, &devSpeed);

        switch(devSpeed) {
        case kUSBDeviceSpeedLow:
            dev->speed = LIBUSB_SPEED_LOW;
            break;
        case kUSBDeviceSpeedFull:
            dev->speed = LIBUSB_SPEED_FULL;
            break;
        case kUSBDeviceSpeedHigh:
            dev->speed = LIBUSB_SPEED_HIGH;
            break;
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1080
        case kUSBDeviceSpeedSuper:
            dev->speed = LIBUSB_SPEED_SUPER;
            break;
#endif
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101200
        case kUSBDeviceSpeedSuperPlus:
            dev->speed = LIBUSB_SPEED_SUPER_PLUS;
            break;
#endif
        default:
            usbi_warn(ctx, "Got unknown device speed %d", devSpeed);
        }

        ret = usbi_sanitize_device(dev);
        if(ret < 0)
            break;

        usbi_dbg(ctx, "found device with address %d port = %d parent = %p at %p", dev->device_address, dev->port_number, (void *)dev->parent_dev,
                 priv->dev->sys_path);

    } while(0);

    if(!cached_device->in_reenumerate && 0 == ret) {
        usbi_connect_device(dev);
    }
    else {
        libusb_unref_device(dev);
    }

    return ret;
}

static enum libusb_error darwin_scan_devices(struct libusb_context *ctx) {
    struct darwin_cached_device *cached_device;
    UInt64                       old_session_id;
    io_iterator_t                deviceIterator;
    io_service_t                 service;
    IOReturn                     kresult;
    int                          ret;

    kresult = usb_setup_device_iterator(&deviceIterator, 0);
    if(kresult != kIOReturnSuccess)
        return darwin_to_libusb(kresult);

    while((service = IOIteratorNext(deviceIterator))) {
        ret = darwin_get_cached_device(ctx, service, &cached_device, &old_session_id);
        if(ret < 0 || !cached_device->can_enumerate) {
            continue;
        }

        (void)process_new_device(ctx, cached_device, old_session_id);

        IOObjectRelease(service);
    }

    IOObjectRelease(deviceIterator);

    return LIBUSB_SUCCESS;
}

static int darwin_open(struct libusb_device_handle *dev_handle) {
    struct darwin_device_handle_priv *priv  = usbi_get_device_handle_priv(dev_handle);
    struct darwin_cached_device      *dpriv = DARWIN_CACHED_DEVICE(dev_handle->dev);
    IOReturn                          kresult;

    if(0 == dpriv->open_count) {
        /* try to open the device */
        kresult = (*(dpriv->device))->USBDeviceOpenSeize(dpriv->device);
        if(kresult != kIOReturnSuccess) {
            usbi_warn(HANDLE_CTX(dev_handle), "USBDeviceOpen: %s", darwin_error_str(kresult));

            if(kIOReturnExclusiveAccess != kresult) {
                return darwin_to_libusb(kresult);
            }

            /* it is possible to perform some actions on a device that is not open so do not return an error */
            priv->is_open = false;
        }
        else {
            priv->is_open = true;
        }

        /* create async event source */
        kresult = (*(dpriv->device))->CreateDeviceAsyncEventSource(dpriv->device, &priv->cfSource);
        if(kresult != kIOReturnSuccess) {
            usbi_err(HANDLE_CTX(dev_handle), "CreateDeviceAsyncEventSource: %s", darwin_error_str(kresult));

            if(priv->is_open) {
                (*(dpriv->device))->USBDeviceClose(dpriv->device);
            }

            priv->is_open = false;

            return darwin_to_libusb(kresult);
        }

        CFRetain(libusb_darwin_acfl);

        /* add the cfSource to the async run loop */
        CFRunLoopAddSource(libusb_darwin_acfl, priv->cfSource, kCFRunLoopCommonModes);
    }

    /* device opened successfully */
    dpriv->open_count++;

    usbi_dbg(HANDLE_CTX(dev_handle), "device open for access");

    return 0;
}

static void darwin_close(struct libusb_device_handle *dev_handle) {
    struct darwin_device_handle_priv *priv  = usbi_get_device_handle_priv(dev_handle);
    struct darwin_cached_device      *dpriv = DARWIN_CACHED_DEVICE(dev_handle->dev);
    IOReturn                          kresult;
    int                               i;

    if(dpriv->open_count == 0) {
        /* something is probably very wrong if this is the case */
        usbi_err(HANDLE_CTX(dev_handle), "Close called on a device that was not open!");
        return;
    }

    dpriv->open_count--;
    if(NULL == dpriv->device) {
        usbi_warn(HANDLE_CTX(dev_handle), "darwin_close device missing IOService");
        return;
    }

    /* make sure all interfaces are released */
    for(i = 0; i < USB_MAXINTERFACES; i++)
        if(dev_handle->claimed_interfaces & (1U << i))
            libusb_release_interface(dev_handle, i);

    if(0 == dpriv->open_count) {
        /* delete the device's async event source */
        if(priv->cfSource) {
            CFRunLoopRemoveSource(libusb_darwin_acfl, priv->cfSource, kCFRunLoopDefaultMode);
            CFRelease(priv->cfSource);
            priv->cfSource = NULL;
            CFRelease(libusb_darwin_acfl);
        }

        if(priv->is_open) {
            /* close the device */
            kresult = (*(dpriv->device))->USBDeviceClose(dpriv->device);
            if(kresult != kIOReturnSuccess) {
                /* Log the fact that we had a problem closing the file, however failing a
                 * close isn't really an error, so return success anyway */
                usbi_warn(HANDLE_CTX(dev_handle), "USBDeviceClose: %s", darwin_error_str(kresult));
            }
        }
    }
}

static int darwin_get_configuration(struct libusb_device_handle *dev_handle, uint8_t *config) {
    struct darwin_cached_device *dpriv = DARWIN_CACHED_DEVICE(dev_handle->dev);

    *config = dpriv->active_config;

    return LIBUSB_SUCCESS;
}

static enum libusb_error darwin_set_configuration(struct libusb_device_handle *dev_handle, int config) {
    struct darwin_cached_device *dpriv = DARWIN_CACHED_DEVICE(dev_handle->dev);
    IOReturn                     kresult;
    uint8_t                      i;

    if(config == -1)
        config = 0;

    /* Setting configuration will invalidate the interface, so we need
       to reclaim it. First, dispose of existing interfaces, if any. */
    for(i = 0; i < USB_MAXINTERFACES; i++)
        if(dev_handle->claimed_interfaces & (1U << i))
            darwin_release_interface(dev_handle, i);

    kresult = (*(dpriv->device))->SetConfiguration(dpriv->device, (UInt8)config);
    if(kresult != kIOReturnSuccess)
        return darwin_to_libusb(kresult);

    /* Reclaim any interfaces. */
    for(i = 0; i < USB_MAXINTERFACES; i++)
        if(dev_handle->claimed_interfaces & (1U << i))
            darwin_claim_interface(dev_handle, i);

    dpriv->active_config = (UInt8)config;

    return LIBUSB_SUCCESS;
}

static IOReturn darwin_get_interface(usb_device_t **darwin_device, uint8_t ifc, io_service_t *usbInterfacep) {
    IOUSBFindInterfaceRequest request;
    IOReturn                  kresult;
    io_iterator_t             interface_iterator;
    UInt8                     bInterfaceNumber;
    bool                      ret;

    *usbInterfacep = IO_OBJECT_NULL;

    /* Setup the Interface Request */
    request.bInterfaceClass    = kIOUSBFindInterfaceDontCare;
    request.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
    request.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
    request.bAlternateSetting  = kIOUSBFindInterfaceDontCare;

    kresult = (*(darwin_device))->CreateInterfaceIterator(darwin_device, &request, &interface_iterator);
    if(kresult != kIOReturnSuccess)
        return kresult;

    while((*usbInterfacep = IOIteratorNext(interface_iterator))) {
        /* find the interface number */
        ret = get_ioregistry_value_number(*usbInterfacep, CFSTR("bInterfaceNumber"), kCFNumberSInt8Type, &bInterfaceNumber);

        if(ret && bInterfaceNumber == ifc) {
            break;
        }

        (void)IOObjectRelease(*usbInterfacep);
    }

    /* done with the interface iterator */
    IOObjectRelease(interface_iterator);

    return kIOReturnSuccess;
}

static const struct libusb_interface_descriptor *get_interface_descriptor_by_number(struct libusb_device_handle     *dev_handle,
                                                                                    struct libusb_config_descriptor *conf_desc, int iface, uint8_t altsetting) {
    int i;

    for(i = 0; i < conf_desc->bNumInterfaces; i++) {
        if(altsetting < conf_desc->interface[i].num_altsetting && conf_desc->interface[i].altsetting[altsetting].bInterfaceNumber == iface) {
            return &conf_desc->interface[i].altsetting[altsetting];
        }
    }

    usbi_err(HANDLE_CTX(dev_handle), "interface %d with altsetting %d not found for device", iface, (int)altsetting);
    return NULL;
}

static enum libusb_error get_endpoints(struct libusb_device_handle *dev_handle, uint8_t iface) {
    struct darwin_device_handle_priv *priv = usbi_get_device_handle_priv(dev_handle);

    /* current interface */
    struct darwin_interface *cInterface = &priv->interfaces[iface];
#if InterfaceVersion >= 550
    IOUSBEndpointProperties pipeProperties = { .bVersion = kUSBEndpointPropertiesVersion3 };
#else
    UInt8  dont_care1, dont_care3;
    UInt16 dont_care2;
#endif

    IOReturn kresult;

    UInt8                  numep, direction, number;
    int                    rc;
    struct libusb_context *ctx = HANDLE_CTX(dev_handle);

    usbi_dbg(ctx, "building table of endpoints.");

    /* retrieve the total number of endpoints on this interface */
    kresult = (*(cInterface->interface))->GetNumEndpoints(cInterface->interface, &numep);
    if(kresult != kIOReturnSuccess) {
        usbi_err(ctx, "can't get number of endpoints for interface: %s", darwin_error_str(kresult));
        return darwin_to_libusb(kresult);
    }

    /* iterate through pipe references */
    for(UInt8 i = 1; i <= numep; i++) {
#if InterfaceVersion >= 550
        kresult   = (*(cInterface->interface))->GetPipePropertiesV3(cInterface->interface, i, &pipeProperties);
        number    = pipeProperties.bEndpointNumber;
        direction = pipeProperties.bDirection;
#else
        kresult = (*(cInterface->interface))->GetPipeProperties(cInterface->interface, i, &direction, &number, &dont_care1, &dont_care2, &dont_care3);
#endif
        if(kresult != kIOReturnSuccess) {
            /* probably a buggy device. try to get the endpoint address from the descriptors */
            struct libusb_config_descriptor          *config;
            const struct libusb_interface_descriptor *if_desc;
            const struct libusb_endpoint_descriptor  *endpoint_desc;
            UInt8                                     alt_setting;

            kresult = (*(cInterface->interface))->GetAlternateSetting(cInterface->interface, &alt_setting);
            if(kresult != kIOReturnSuccess) {
                usbi_err(HANDLE_CTX(dev_handle), "can't get alternate setting for interface");
                return darwin_to_libusb(kresult);
            }

            rc = libusb_get_active_config_descriptor(dev_handle->dev, &config);
            if(LIBUSB_SUCCESS != rc) {
                return rc;
            }

            if_desc = get_interface_descriptor_by_number(dev_handle, config, iface, alt_setting);
            if(if_desc == NULL) {
                libusb_free_config_descriptor(config);
                return LIBUSB_ERROR_NOT_FOUND;
            }

            endpoint_desc = if_desc->endpoint + i - 1;

            cInterface->endpoint_addrs[i - 1] = endpoint_desc->bEndpointAddress;
            libusb_free_config_descriptor(config);
        }
        else {
            cInterface->endpoint_addrs[i - 1] = (UInt8)(((kUSBIn == direction) << kUSBRqDirnShift) | (number & LIBUSB_ENDPOINT_ADDRESS_MASK));
        }

        usbi_dbg(ctx, "interface: %i pipe %i: dir: %i number: %i", iface, i, cInterface->endpoint_addrs[i - 1] >> kUSBRqDirnShift,
                 cInterface->endpoint_addrs[i - 1] & LIBUSB_ENDPOINT_ADDRESS_MASK);
    }

    cInterface->num_endpoints = numep;

    return LIBUSB_SUCCESS;
}

static int darwin_claim_interface(struct libusb_device_handle *dev_handle, uint8_t iface) {
    struct darwin_cached_device      *dpriv        = DARWIN_CACHED_DEVICE(dev_handle->dev);
    struct darwin_device_handle_priv *priv         = usbi_get_device_handle_priv(dev_handle);
    io_service_t                      usbInterface = IO_OBJECT_NULL;
    IOReturn                          kresult;
    enum libusb_error                 ret;
    IOCFPlugInInterface             **plugInInterface = NULL;
    SInt32                            score;

    /* current interface */
    struct darwin_interface *cInterface = &priv->interfaces[iface];

    struct libusb_context *ctx = HANDLE_CTX(dev_handle);

    kresult = darwin_get_interface(dpriv->device, iface, &usbInterface);
    if(kresult != kIOReturnSuccess)
        return darwin_to_libusb(kresult);

    /* make sure we have an interface */
    if(!usbInterface && dpriv->first_config != 0) {
        usbi_info(ctx, "no interface found; setting configuration: %d", dpriv->first_config);

        /* set the configuration */
        ret = darwin_set_configuration(dev_handle, (int)dpriv->first_config);
        if(ret != LIBUSB_SUCCESS) {
            usbi_err(ctx, "could not set configuration");
            return ret;
        }

        kresult = darwin_get_interface(dpriv->device, iface, &usbInterface);
        if(kresult != kIOReturnSuccess) {
            usbi_err(ctx, "darwin_get_interface: %s", darwin_error_str(kresult));
            return darwin_to_libusb(kresult);
        }
    }

    if(!usbInterface) {
        usbi_info(ctx, "interface not found");
        return LIBUSB_ERROR_NOT_FOUND;
    }

    /* get an interface to the device's interface */
    kresult = IOCreatePlugInInterfaceForService(usbInterface, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID, &plugInInterface, &score);

    /* ignore release error */
    (void)IOObjectRelease(usbInterface);

    if(kresult != kIOReturnSuccess) {
        usbi_err(ctx, "IOCreatePlugInInterfaceForService: %s", darwin_error_str(kresult));
        return darwin_to_libusb(kresult);
    }

    if(!plugInInterface) {
        usbi_err(ctx, "plugin interface not found");
        return LIBUSB_ERROR_NOT_FOUND;
    }

    /* Do the actual claim */
    kresult = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(InterfaceInterfaceID), (LPVOID)&cInterface->interface);
    /* We no longer need the intermediate plug-in */
    /* Use release instead of IODestroyPlugInInterface to avoid stopping IOServices associated with this device */
    (*plugInInterface)->Release(plugInInterface);
    if(kresult != kIOReturnSuccess || !cInterface->interface) {
        usbi_err(ctx, "QueryInterface: %s", darwin_error_str(kresult));
        return darwin_to_libusb(kresult);
    }

    /* claim the interface */
    kresult = (*(cInterface->interface))->USBInterfaceOpen(cInterface->interface);
    if(kresult != kIOReturnSuccess) {
        usbi_info(ctx, "USBInterfaceOpen: %s", darwin_error_str(kresult));
        return darwin_to_libusb(kresult);
    }

    /* update list of endpoints */
    ret = get_endpoints(dev_handle, iface);
    if(ret) {
        /* this should not happen */
        darwin_release_interface(dev_handle, iface);
        usbi_err(ctx, "could not build endpoint table");
        return ret;
    }

    cInterface->cfSource = NULL;

    /* create async event source */
    kresult = (*(cInterface->interface))->CreateInterfaceAsyncEventSource(cInterface->interface, &cInterface->cfSource);
    if(kresult != kIOReturnSuccess) {
        usbi_err(ctx, "could not create async event source");

        /* can't continue without an async event source */
        (void)darwin_release_interface(dev_handle, iface);

        return darwin_to_libusb(kresult);
    }

    /* add the cfSource to the async thread's run loop */
    CFRunLoopAddSource(libusb_darwin_acfl, cInterface->cfSource, kCFRunLoopDefaultMode);

    usbi_dbg(ctx, "interface opened");

    return LIBUSB_SUCCESS;
}

static int darwin_release_interface(struct libusb_device_handle *dev_handle, uint8_t iface) {
    struct darwin_device_handle_priv *priv = usbi_get_device_handle_priv(dev_handle);
    IOReturn                          kresult;

    /* current interface */
    struct darwin_interface *cInterface = &priv->interfaces[iface];

    /* Check to see if an interface is open */
    if(!cInterface->interface)
        return LIBUSB_SUCCESS;

    /* clean up endpoint data */
    cInterface->num_endpoints = 0;

    /* delete the interface's async event source */
    if(cInterface->cfSource) {
        CFRunLoopRemoveSource(libusb_darwin_acfl, cInterface->cfSource, kCFRunLoopDefaultMode);
        CFRelease(cInterface->cfSource);
        cInterface->cfSource = NULL;
    }

    kresult = (*(cInterface->interface))->USBInterfaceClose(cInterface->interface);
    if(kresult != kIOReturnSuccess)
        usbi_warn(HANDLE_CTX(dev_handle), "USBInterfaceClose: %s", darwin_error_str(kresult));

    kresult = (*(cInterface->interface))->Release(cInterface->interface);
    if(kresult != kIOReturnSuccess)
        usbi_warn(HANDLE_CTX(dev_handle), "Release: %s", darwin_error_str(kresult));

    cInterface->interface = (usb_interface_t **)IO_OBJECT_NULL;

    return darwin_to_libusb(kresult);
}

static int check_alt_setting_and_clear_halt(struct libusb_device_handle *dev_handle, uint8_t altsetting, struct darwin_interface *cInterface) {
    enum libusb_error ret;
    IOReturn          kresult;
    uint8_t           current_alt_setting;

    kresult = (*(cInterface->interface))->GetAlternateSetting(cInterface->interface, &current_alt_setting);
    if(kresult == kIOReturnSuccess && altsetting != current_alt_setting) {
        return LIBUSB_ERROR_PIPE;
    }

    for(int i = 0; i < cInterface->num_endpoints; i++) {
        ret = darwin_clear_halt(dev_handle, cInterface->endpoint_addrs[i]);
        if(LIBUSB_SUCCESS != ret) {
            usbi_warn(HANDLE_CTX(dev_handle), "error clearing pipe halt for endpoint %d", i);
            if(LIBUSB_ERROR_NOT_FOUND == ret) {
                /* may need to re-open the interface */
                return ret;
            }
        }
    }

    return LIBUSB_SUCCESS;
}

static int darwin_set_interface_altsetting(struct libusb_device_handle *dev_handle, uint8_t iface, uint8_t altsetting) {
    struct darwin_device_handle_priv *priv = usbi_get_device_handle_priv(dev_handle);
    IOReturn                          kresult;
    enum libusb_error                 ret;

    /* current interface */
    struct darwin_interface *cInterface = &priv->interfaces[iface];

    if(!cInterface->interface)
        return LIBUSB_ERROR_NO_DEVICE;

    kresult = (*(cInterface->interface))->SetAlternateInterface(cInterface->interface, altsetting);
    if(kresult == kIOReturnSuccess) {
        /* update the list of endpoints */
        ret = get_endpoints(dev_handle, iface);
        if(ret) {
            /* this should not happen */
            darwin_release_interface(dev_handle, iface);
            usbi_err(HANDLE_CTX(dev_handle), "could not build endpoint table");
        }
        return ret;
    }

    usbi_warn(HANDLE_CTX(dev_handle), "SetAlternateInterface: %s", darwin_error_str(kresult));

    ret = darwin_to_libusb(kresult);
    if(ret != LIBUSB_ERROR_PIPE) {
        return ret;
    }

    /* If a device only supports a default setting for the specified interface, then a STALL
       (kIOUSBPipeStalled) may be returned. Ref: USB 2.0 specs 9.4.10.
       Mimic the behaviour in e.g. the Linux kernel: in such case, reset all endpoints
       of the interface (as would have been done per 9.1.1.5) and return success. */

    ret = check_alt_setting_and_clear_halt(dev_handle, altsetting, cInterface);
    if(LIBUSB_ERROR_NOT_FOUND == ret) {
        /* For some reason we need to reclaim the interface after the pipe error with some versions of macOS */
        ret = darwin_claim_interface(dev_handle, iface);
        if(LIBUSB_SUCCESS != ret) {
            darwin_release_interface(dev_handle, iface);
            usbi_err(HANDLE_CTX(dev_handle), "could not reclaim interface: %s", darwin_error_str(kresult));
        }
        ret = check_alt_setting_and_clear_halt(dev_handle, altsetting, cInterface);
    }

    return ret;
}

static int darwin_clear_halt(struct libusb_device_handle *dev_handle, unsigned char endpoint) {
    /* current interface */
    struct darwin_interface *cInterface;
    IOReturn                 kresult;
    uint8_t                  pipeRef;

    /* determine the interface/endpoint to use */
    if(ep_to_pipeRef(dev_handle, endpoint, &pipeRef, NULL, &cInterface) != 0) {
        usbi_err(HANDLE_CTX(dev_handle), "endpoint not found on any open interface");

        return LIBUSB_ERROR_NOT_FOUND;
    }

    /* newer versions of darwin support clearing additional bits on the device's endpoint */
    kresult = (*(cInterface->interface))->ClearPipeStallBothEnds(cInterface->interface, pipeRef);
    if(kresult != kIOReturnSuccess)
        usbi_warn(HANDLE_CTX(dev_handle), "ClearPipeStall: %s", darwin_error_str(kresult));

    return darwin_to_libusb(kresult);
}

static int darwin_restore_state(struct libusb_device_handle *dev_handle, int8_t active_config, unsigned long claimed_interfaces) {
    struct darwin_cached_device      *dpriv      = DARWIN_CACHED_DEVICE(dev_handle->dev);
    struct darwin_device_handle_priv *priv       = usbi_get_device_handle_priv(dev_handle);
    int                               open_count = dpriv->open_count;
    int                               ret;

    struct libusb_context *ctx = HANDLE_CTX(dev_handle);

    /* clear claimed interfaces temporarily */
    dev_handle->claimed_interfaces = 0;

    /* close and re-open the device */
    priv->is_open     = false;
    dpriv->open_count = 1;

    /* clean up open interfaces */
    (void)darwin_close(dev_handle);

    /* re-open the device */
    ret               = darwin_open(dev_handle);
    dpriv->open_count = open_count;
    if(LIBUSB_SUCCESS != ret) {
        /* could not restore configuration */
        return LIBUSB_ERROR_NOT_FOUND;
    }

    if(dpriv->active_config != active_config) {
        usbi_dbg(ctx, "darwin/restore_state: restoring configuration %d...", active_config);

        ret = darwin_set_configuration(dev_handle, active_config);
        if(LIBUSB_SUCCESS != ret) {
            usbi_dbg(ctx, "darwin/restore_state: could not restore configuration");
            return LIBUSB_ERROR_NOT_FOUND;
        }
    }

    usbi_dbg(ctx, "darwin/restore_state: reclaiming interfaces");

    if(claimed_interfaces) {
        for(uint8_t iface = 0; iface < USB_MAXINTERFACES; ++iface) {
            if(!(claimed_interfaces & (1U << iface))) {
                continue;
            }

            usbi_dbg(ctx, "darwin/restore_state: re-claiming interface %u", iface);

            ret = darwin_claim_interface(dev_handle, iface);
            if(LIBUSB_SUCCESS != ret) {
                usbi_dbg(ctx, "darwin/restore_state: could not claim interface %u", iface);
                return LIBUSB_ERROR_NOT_FOUND;
            }

            dev_handle->claimed_interfaces |= 1U << iface;
        }
    }

    usbi_dbg(ctx, "darwin/restore_state: device state restored");

    return LIBUSB_SUCCESS;
}

static int darwin_reenumerate_device(struct libusb_device_handle *dev_handle, bool capture) {
    struct darwin_cached_device    *dpriv              = DARWIN_CACHED_DEVICE(dev_handle->dev);
    unsigned long                   claimed_interfaces = dev_handle->claimed_interfaces;
    int8_t                          active_config      = dpriv->active_config;
    UInt32                          options            = 0;
    IOUSBDeviceDescriptor           descriptor;
    IOUSBConfigurationDescriptorPtr cached_configuration;
    IOUSBConfigurationDescriptor   *cached_configurations;
    IOReturn                        kresult;
    UInt8                           i;

    struct libusb_context *ctx = HANDLE_CTX(dev_handle);

    if(dpriv->in_reenumerate) {
        /* ack, two (or more) threads are trying to reset the device! abort! */
        return LIBUSB_ERROR_NOT_FOUND;
    }

    dpriv->in_reenumerate = true;

    /* store copies of descriptors so they can be compared after the reset */
    memcpy(&descriptor, &dpriv->dev_descriptor, sizeof(descriptor));
    cached_configurations = alloca(sizeof(*cached_configurations) * descriptor.bNumConfigurations);

    for(i = 0; i < descriptor.bNumConfigurations; ++i) {
        (*(dpriv->device))->GetConfigurationDescriptorPtr(dpriv->device, i, &cached_configuration);
        memcpy(cached_configurations + i, cached_configuration, sizeof(cached_configurations[i]));
    }

    /* if we need to release capture */
    if(HAS_CAPTURE_DEVICE()) {
        if(capture) {
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101000
            options |= kUSBReEnumerateCaptureDeviceMask;
#endif
        }
    }
    else {
        capture = false;
    }

    /* from macOS 10.11 ResetDevice no longer does anything so just use USBDeviceReEnumerate */
    kresult = (*(dpriv->device))->USBDeviceReEnumerate(dpriv->device, options);
    if(kresult != kIOReturnSuccess) {
        usbi_err(ctx, "USBDeviceReEnumerate: %s", darwin_error_str(kresult));
        dpriv->in_reenumerate = false;
        return darwin_to_libusb(kresult);
    }

    /* capture mode does not re-enumerate but it does require re-open */
    if(capture) {
        usbi_dbg(ctx, "darwin/reenumerate_device: restoring state...");
        dpriv->in_reenumerate = false;
        return darwin_restore_state(dev_handle, active_config, claimed_interfaces);
    }

    usbi_dbg(ctx, "darwin/reenumerate_device: waiting for re-enumeration to complete...");

    struct timespec start;
    usbi_get_monotonic_time(&start);

    while(dpriv->in_reenumerate) {
        struct timespec delay = { .tv_sec = 0, .tv_nsec = 1000 };
        nanosleep(&delay, NULL);

        struct timespec now;
        usbi_get_monotonic_time(&now);
        unsigned long elapsed_us = (now.tv_sec - start.tv_sec) * USEC_PER_SEC + (now.tv_nsec - start.tv_nsec) / 1000;

        if(elapsed_us >= DARWIN_REENUMERATE_TIMEOUT_US) {
            usbi_err(ctx, "darwin/reenumerate_device: timeout waiting for reenumerate");
            dpriv->in_reenumerate = false;
            return LIBUSB_ERROR_TIMEOUT;
        }
    }

    /* compare descriptors */
    usbi_dbg(ctx, "darwin/reenumerate_device: checking whether descriptors changed");

    if(memcmp(&descriptor, &dpriv->dev_descriptor, sizeof(descriptor))) {
        /* device descriptor changed. need to return not found. */
        usbi_dbg(ctx, "darwin/reenumerate_device: device descriptor changed");
        return LIBUSB_ERROR_NOT_FOUND;
    }

    for(i = 0; i < descriptor.bNumConfigurations; ++i) {
        (void)(*(dpriv->device))->GetConfigurationDescriptorPtr(dpriv->device, i, &cached_configuration);
        if(memcmp(cached_configuration, cached_configurations + i, sizeof(cached_configurations[i]))) {
            usbi_dbg(ctx, "darwin/reenumerate_device: configuration descriptor %d changed", i);
            return LIBUSB_ERROR_NOT_FOUND;
        }
    }

    usbi_dbg(ctx, "darwin/reenumerate_device: device reset complete. restoring state...");

    return darwin_restore_state(dev_handle, active_config, claimed_interfaces);
}

static int darwin_reset_device(struct libusb_device_handle *dev_handle) {
    struct darwin_cached_device *dpriv = DARWIN_CACHED_DEVICE(dev_handle->dev);
    IOReturn                     kresult;
    enum libusb_error            ret;

#if !defined(TARGET_OS_OSX) || TARGET_OS_OSX == 1
    if(dpriv->capture_count > 0) {
        /* we have to use ResetDevice as USBDeviceReEnumerate() loses the authorization for capture */
        kresult = (*(dpriv->device))->ResetDevice(dpriv->device);
        ret     = darwin_to_libusb(kresult);
    }
    else {
        ret = darwin_reenumerate_device(dev_handle, false);
    }
#else
    /* ResetDevice() is missing on non-macOS platforms */
    ret = darwin_reenumerate_device(dev_handle, false);
    if((ret == LIBUSB_SUCCESS || ret == LIBUSB_ERROR_NOT_FOUND) && dpriv->capture_count > 0) {
        int           capture_count;
        int8_t        active_config      = dpriv->active_config;
        unsigned long claimed_interfaces = dev_handle->claimed_interfaces;

        /* save old capture_count */
        capture_count = dpriv->capture_count;
        /* reset capture count */
        dpriv->capture_count = 0;
        /* attempt to detach kernel driver again as it is now re-attached */
        ret = darwin_detach_kernel_driver(dev_handle, 0);
        if(ret != LIBUSB_SUCCESS) {
            return ret;
        }
        /* restore capture_count */
        dpriv->capture_count = capture_count;
        /* restore configuration */
        ret = darwin_restore_state(dev_handle, active_config, claimed_interfaces);
    }
#endif
    return ret;
}

static io_service_t usb_find_interface_matching_location(const io_name_t class_name, UInt8 interface_number, UInt32 location) {
    CFMutableDictionaryRef matchingDict = IOServiceMatching(class_name);
    CFMutableDictionaryRef propertyMatchDict =
        CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFTypeRef locationCF  = CFNumberCreate(NULL, kCFNumberSInt32Type, &location);
    CFTypeRef interfaceCF = CFNumberCreate(NULL, kCFNumberSInt8Type, &interface_number);

    CFDictionarySetValue(matchingDict, CFSTR(kIOPropertyMatchKey), propertyMatchDict);
    CFDictionarySetValue(propertyMatchDict, CFSTR(kUSBDevicePropertyLocationID), locationCF);
    CFDictionarySetValue(propertyMatchDict, CFSTR(kUSBHostMatchingPropertyInterfaceNumber), interfaceCF);

    CFRelease(interfaceCF);
    CFRelease(locationCF);
    CFRelease(propertyMatchDict);

    return IOServiceGetMatchingService(darwin_default_master_port, matchingDict);
}

static int darwin_kernel_driver_active(struct libusb_device_handle *dev_handle, uint8_t interface) {
    struct darwin_cached_device *dpriv = DARWIN_CACHED_DEVICE(dev_handle->dev);
    io_service_t                 usb_interface, child = IO_OBJECT_NULL;

    /* locate the IO registry entry for this interface */
    usb_interface = usb_find_interface_matching_location(kIOUSBHostInterfaceClassName, interface, dpriv->location);
    if(0 == usb_interface) {
        /* check for the legacy class entry */
        usb_interface = usb_find_interface_matching_location(kIOUSBInterfaceClassName, interface, dpriv->location);
        if(0 == usb_interface) {
            return LIBUSB_ERROR_NOT_FOUND;
        }
    }

    /* if the IO object has a child entry in the IO Registry it has a kernel driver attached */
    (void)IORegistryEntryGetChildEntry(usb_interface, kIOServicePlane, &child);
    IOObjectRelease(usb_interface);
    if(IO_OBJECT_NULL != child) {
        IOObjectRelease(child);
        return 1;
    }

    /* no driver */
    return 0;
}

static void darwin_destroy_device(struct libusb_device *dev) {
    struct darwin_device_priv *dpriv = usbi_get_device_priv(dev);

    if(dpriv->dev) {
        /* need to hold the lock in case this is the last reference to the device */
        usbi_mutex_lock(&darwin_cached_devices_mutex);
        darwin_deref_cached_device(dpriv->dev);
        dpriv->dev = NULL;
        usbi_mutex_unlock(&darwin_cached_devices_mutex);
    }
}

static int submit_bulk_transfer(struct usbi_transfer *itransfer) {
    struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);

    IOReturn ret;
    uint8_t  transferType;
    uint8_t  pipeRef;
    uint16_t maxPacketSize;

    struct darwin_interface *cInterface;
#if InterfaceVersion >= 550
    IOUSBEndpointProperties pipeProperties = { .bVersion = kUSBEndpointPropertiesVersion3 };
#else
    /* None of the values below are used in libusb for bulk transfers */
    uint8_t direction, number, interval;
#endif

    if(ep_to_pipeRef(transfer->dev_handle, transfer->endpoint, &pipeRef, NULL, &cInterface) != 0) {
        usbi_err(TRANSFER_CTX(transfer), "endpoint not found on any open interface");

        return LIBUSB_ERROR_NOT_FOUND;
    }

#if InterfaceVersion >= 550
    ret = (*(cInterface->interface))->GetPipePropertiesV3(cInterface->interface, pipeRef, &pipeProperties);

    transferType  = pipeProperties.bTransferType;
    maxPacketSize = pipeProperties.wMaxPacketSize;
#else
    ret = (*(cInterface->interface))->GetPipeProperties(cInterface->interface, pipeRef, &direction, &number, &transferType, &maxPacketSize, &interval);
#endif

    if(ret) {
        usbi_err(TRANSFER_CTX(transfer), "bulk transfer failed (dir = %s): %s (code = 0x%08x)", IS_XFERIN(transfer) ? "In" : "Out", darwin_error_str(ret), ret);
        return darwin_to_libusb(ret);
    }

    if(0 != (transfer->length % maxPacketSize)) {
        /* do not need a zero packet */
        transfer->flags &= ~LIBUSB_TRANSFER_ADD_ZERO_PACKET;
    }

    /* submit the request */
    /* timeouts are unavailable on interrupt endpoints */
    if(transferType == kUSBInterrupt) {
        if(IS_XFERIN(transfer))
            ret = (*(cInterface->interface))
                      ->ReadPipeAsync(cInterface->interface, pipeRef, transfer->buffer, (UInt32)transfer->length, darwin_async_io_callback, itransfer);
        else
            ret = (*(cInterface->interface))
                      ->WritePipeAsync(cInterface->interface, pipeRef, transfer->buffer, (UInt32)transfer->length, darwin_async_io_callback, itransfer);
    }
    else {
        itransfer->timeout_flags |= USBI_TRANSFER_OS_HANDLES_TIMEOUT;

        if(IS_XFERIN(transfer))
            ret = (*(cInterface->interface))
                      ->ReadPipeAsyncTO(cInterface->interface, pipeRef, transfer->buffer, (UInt32)transfer->length, transfer->timeout, transfer->timeout,
                                        darwin_async_io_callback, itransfer);
        else
            ret = (*(cInterface->interface))
                      ->WritePipeAsyncTO(cInterface->interface, pipeRef, transfer->buffer, (UInt32)transfer->length, transfer->timeout, transfer->timeout,
                                         darwin_async_io_callback, itransfer);
    }

    if(ret)
        usbi_err(TRANSFER_CTX(transfer), "bulk transfer failed (dir = %s): %s (code = 0x%08x)", IS_XFERIN(transfer) ? "In" : "Out", darwin_error_str(ret), ret);

    return darwin_to_libusb(ret);
}

#if InterfaceVersion >= 550
static int submit_stream_transfer(struct usbi_transfer *itransfer) {
    struct libusb_transfer  *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    struct darwin_interface *cInterface;
    uint8_t                  pipeRef;
    IOReturn                 ret;

    if(ep_to_pipeRef(transfer->dev_handle, transfer->endpoint, &pipeRef, NULL, &cInterface) != 0) {
        usbi_err(TRANSFER_CTX(transfer), "endpoint not found on any open interface");

        return LIBUSB_ERROR_NOT_FOUND;
    }

    itransfer->timeout_flags |= USBI_TRANSFER_OS_HANDLES_TIMEOUT;

    if(IS_XFERIN(transfer))
        ret = (*(cInterface->interface))
                  ->ReadStreamsPipeAsyncTO(cInterface->interface, pipeRef, itransfer->stream_id, transfer->buffer, (UInt32)transfer->length, transfer->timeout,
                                           transfer->timeout, darwin_async_io_callback, itransfer);
    else
        ret = (*(cInterface->interface))
                  ->WriteStreamsPipeAsyncTO(cInterface->interface, pipeRef, itransfer->stream_id, transfer->buffer, (UInt32)transfer->length, transfer->timeout,
                                            transfer->timeout, darwin_async_io_callback, itransfer);

    if(ret)
        usbi_err(TRANSFER_CTX(transfer), "bulk stream transfer failed (dir = %s): %s (code = 0x%08x)", IS_XFERIN(transfer) ? "In" : "Out",
                 darwin_error_str(ret), ret);

    return darwin_to_libusb(ret);
}
#endif

static int submit_iso_transfer(struct usbi_transfer *itransfer) {
    struct libusb_transfer      *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    struct darwin_transfer_priv *tpriv    = usbi_get_transfer_priv(itransfer);

    IOReturn     kresult;
    uint8_t      pipeRef, interval;
    UInt64       frame;
    AbsoluteTime atTime;
    int          i;
#if InterfaceVersion >= 550
    IOUSBEndpointProperties pipeProperties = { .bVersion = kUSBEndpointPropertiesVersion3 };
#else
    /* None of the values below are used in libusb for iso transfers */
    uint8_t  direction, number, transferType;
    uint16_t maxPacketSize;
#endif

    struct darwin_interface *cInterface;

    /* construct an array of IOUSBIsocFrames, reuse the old one if the sizes are the same */
    if(tpriv->num_iso_packets != transfer->num_iso_packets) {
        free(tpriv->isoc_framelist);
        tpriv->isoc_framelist = NULL;
    }

    if(!tpriv->isoc_framelist) {
        tpriv->num_iso_packets = transfer->num_iso_packets;
        tpriv->isoc_framelist  = (IOUSBIsocFrame *)calloc((size_t)transfer->num_iso_packets, sizeof(IOUSBIsocFrame));
        if(!tpriv->isoc_framelist)
            return LIBUSB_ERROR_NO_MEM;
    }

    /* copy the frame list from the libusb descriptor (the structures differ only is member order) */
    for(i = 0; i < transfer->num_iso_packets; i++) {
        unsigned int length = transfer->iso_packet_desc[i].length;
        assert(length <= UINT16_MAX);
        tpriv->isoc_framelist[i].frReqCount = (UInt16)length;
    }

    /* determine the interface/endpoint to use */
    if(ep_to_pipeRef(transfer->dev_handle, transfer->endpoint, &pipeRef, NULL, &cInterface) != 0) {
        usbi_err(TRANSFER_CTX(transfer), "endpoint not found on any open interface");

        return LIBUSB_ERROR_NOT_FOUND;
    }

    /* determine the properties of this endpoint and the speed of the device */
#if InterfaceVersion >= 550
    kresult  = (*(cInterface->interface))->GetPipePropertiesV3(cInterface->interface, pipeRef, &pipeProperties);
    interval = pipeProperties.bInterval;
#else
    kresult = (*(cInterface->interface))->GetPipeProperties(cInterface->interface, pipeRef, &direction, &number, &transferType, &maxPacketSize, &interval);
#endif
    if(kresult != kIOReturnSuccess) {
        usbi_err(TRANSFER_CTX(transfer), "failed to get pipe properties: %d", kresult);
        free(tpriv->isoc_framelist);
        tpriv->isoc_framelist = NULL;

        return darwin_to_libusb(kresult);
    }

    /* Last but not least we need the bus frame number */
    kresult = (*(cInterface->interface))->GetBusFrameNumber(cInterface->interface, &frame, &atTime);
    if(kresult != kIOReturnSuccess) {
        usbi_err(TRANSFER_CTX(transfer), "failed to get bus frame number: %d", kresult);
        free(tpriv->isoc_framelist);
        tpriv->isoc_framelist = NULL;

        return darwin_to_libusb(kresult);
    }

    /* schedule for a frame a little in the future */
    frame += 4;

    if(cInterface->frames[transfer->endpoint] && frame < cInterface->frames[transfer->endpoint])
        frame = cInterface->frames[transfer->endpoint];

    /* submit the request */
    if(IS_XFERIN(transfer))
        kresult = (*(cInterface->interface))
                      ->ReadIsochPipeAsync(cInterface->interface, pipeRef, transfer->buffer, frame, (UInt32)transfer->num_iso_packets, tpriv->isoc_framelist,
                                           darwin_async_io_callback, itransfer);
    else
        kresult = (*(cInterface->interface))
                      ->WriteIsochPipeAsync(cInterface->interface, pipeRef, transfer->buffer, frame, (UInt32)transfer->num_iso_packets, tpriv->isoc_framelist,
                                            darwin_async_io_callback, itransfer);

    if(LIBUSB_SPEED_FULL == transfer->dev_handle->dev->speed)
        /* Full speed */
        cInterface->frames[transfer->endpoint] = frame + (UInt32)transfer->num_iso_packets * (1U << (interval - 1));
    else
        /* High/super speed */
        cInterface->frames[transfer->endpoint] = frame + (UInt32)transfer->num_iso_packets * (1U << (interval - 1)) / 8;

    if(kresult != kIOReturnSuccess) {
        usbi_err(TRANSFER_CTX(transfer), "isochronous transfer failed (dir: %s): %s", IS_XFERIN(transfer) ? "In" : "Out", darwin_error_str(kresult));
        free(tpriv->isoc_framelist);
        tpriv->isoc_framelist = NULL;
    }

    return darwin_to_libusb(kresult);
}

static int submit_control_transfer(struct usbi_transfer *itransfer) {
    struct libusb_transfer      *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    struct libusb_control_setup *setup    = (struct libusb_control_setup *)transfer->buffer;
    struct darwin_cached_device *dpriv    = DARWIN_CACHED_DEVICE(transfer->dev_handle->dev);
    struct darwin_transfer_priv *tpriv    = usbi_get_transfer_priv(itransfer);

    IOReturn kresult;

    memset(&tpriv->req, 0, sizeof(tpriv->req));

    /* IOUSBDeviceInterface expects the request in cpu endianness */
    tpriv->req.bmRequestType = setup->bmRequestType;
    tpriv->req.bRequest      = setup->bRequest;
    /* these values should be in bus order from libusb_fill_control_setup */
    tpriv->req.wValue  = OSSwapLittleToHostInt16(setup->wValue);
    tpriv->req.wIndex  = OSSwapLittleToHostInt16(setup->wIndex);
    tpriv->req.wLength = OSSwapLittleToHostInt16(setup->wLength);
    /* data is stored after the libusb control block */
    tpriv->req.pData             = transfer->buffer + LIBUSB_CONTROL_SETUP_SIZE;
    tpriv->req.completionTimeout = transfer->timeout;
    tpriv->req.noDataTimeout     = transfer->timeout;

    itransfer->timeout_flags |= USBI_TRANSFER_OS_HANDLES_TIMEOUT;

    /* all transfers in libusb-1.0 are async */

    if(transfer->endpoint) {
        struct darwin_interface *cInterface;
        uint8_t                  pipeRef;

        if(ep_to_pipeRef(transfer->dev_handle, transfer->endpoint, &pipeRef, NULL, &cInterface) != 0) {
            usbi_err(TRANSFER_CTX(transfer), "endpoint not found on any open interface");

            return LIBUSB_ERROR_NOT_FOUND;
        }

        kresult = (*(cInterface->interface))->ControlRequestAsyncTO(cInterface->interface, pipeRef, &(tpriv->req), darwin_async_io_callback, itransfer);
    }
    else
        /* control request on endpoint 0 */
        kresult = (*(dpriv->device))->DeviceRequestAsyncTO(dpriv->device, &(tpriv->req), darwin_async_io_callback, itransfer);

    if(kresult != kIOReturnSuccess)
        usbi_err(TRANSFER_CTX(transfer), "control request failed: %s", darwin_error_str(kresult));

    return darwin_to_libusb(kresult);
}

static int darwin_submit_transfer(struct usbi_transfer *itransfer) {
    struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);

    switch(transfer->type) {
    case LIBUSB_TRANSFER_TYPE_CONTROL:
        return submit_control_transfer(itransfer);
    case LIBUSB_TRANSFER_TYPE_BULK:
    case LIBUSB_TRANSFER_TYPE_INTERRUPT:
        return submit_bulk_transfer(itransfer);
    case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
        return submit_iso_transfer(itransfer);
    case LIBUSB_TRANSFER_TYPE_BULK_STREAM:
#if InterfaceVersion >= 550
        return submit_stream_transfer(itransfer);
#else
        usbi_err(TRANSFER_CTX(transfer), "IOUSBFamily version does not support bulk stream transfers");
        return LIBUSB_ERROR_NOT_SUPPORTED;
#endif
    default:
        usbi_err(TRANSFER_CTX(transfer), "unknown endpoint type %d", transfer->type);
        return LIBUSB_ERROR_INVALID_PARAM;
    }
}

static int cancel_control_transfer(struct usbi_transfer *itransfer) {
    struct libusb_transfer      *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    struct darwin_cached_device *dpriv    = DARWIN_CACHED_DEVICE(transfer->dev_handle->dev);
    IOReturn                     kresult;

    usbi_warn(ITRANSFER_CTX(itransfer), "aborting all transactions control pipe");

    if(!dpriv->device)
        return LIBUSB_ERROR_NO_DEVICE;

    kresult = (*(dpriv->device))->USBDeviceAbortPipeZero(dpriv->device);

    return darwin_to_libusb(kresult);
}

static int darwin_abort_transfers(struct usbi_transfer *itransfer) {
    struct libusb_transfer      *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    struct darwin_cached_device *dpriv    = DARWIN_CACHED_DEVICE(transfer->dev_handle->dev);
    struct darwin_interface     *cInterface;
    uint8_t                      pipeRef, iface;
    IOReturn                     kresult;

    struct libusb_context *ctx = ITRANSFER_CTX(itransfer);

    if(ep_to_pipeRef(transfer->dev_handle, transfer->endpoint, &pipeRef, &iface, &cInterface) != 0) {
        usbi_err(ctx, "endpoint not found on any open interface");

        return LIBUSB_ERROR_NOT_FOUND;
    }

    if(!dpriv->device)
        return LIBUSB_ERROR_NO_DEVICE;

    usbi_warn(ctx, "aborting all transactions on interface %d pipe %d", iface, pipeRef);

    /* abort transactions */
#if InterfaceVersion >= 550
    if(LIBUSB_TRANSFER_TYPE_BULK_STREAM == transfer->type)
        kresult = (*(cInterface->interface))->AbortStreamsPipe(cInterface->interface, pipeRef, itransfer->stream_id);
    else
#endif
        kresult = (*(cInterface->interface))->AbortPipe(cInterface->interface, pipeRef);

#if InterfaceVersion <= 245
    /* with older releases of IOUSBFamily the OS always clears the host side data toggle. for
       consistency also clear the data toggle on the device. */
    usbi_dbg(ctx, "calling ClearPipeStallBothEnds to clear the data toggle bit");
    kresult = (*(cInterface->interface))->ClearPipeStallBothEnds(cInterface->interface, pipeRef);
#endif

    return darwin_to_libusb(kresult);
}

static int darwin_cancel_transfer(struct usbi_transfer *itransfer) {
    struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);

    switch(transfer->type) {
    case LIBUSB_TRANSFER_TYPE_CONTROL:
        return cancel_control_transfer(itransfer);
    case LIBUSB_TRANSFER_TYPE_BULK:
    case LIBUSB_TRANSFER_TYPE_INTERRUPT:
    case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
        return darwin_abort_transfers(itransfer);
    default:
        usbi_err(TRANSFER_CTX(transfer), "unknown endpoint type %d", transfer->type);
        return LIBUSB_ERROR_INVALID_PARAM;
    }
}

static void darwin_async_io_callback(void *refcon, IOReturn result, void *arg0) {
    struct usbi_transfer        *itransfer = (struct usbi_transfer *)refcon;
    struct libusb_transfer      *transfer  = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    struct darwin_transfer_priv *tpriv     = usbi_get_transfer_priv(itransfer);

    usbi_dbg(TRANSFER_CTX(transfer), "an async io operation has completed");

    /* if requested write a zero packet */
    if(kIOReturnSuccess == result && IS_XFEROUT(transfer) && transfer->flags & LIBUSB_TRANSFER_ADD_ZERO_PACKET) {
        struct darwin_interface *cInterface;
        uint8_t                  pipeRef;

        (void)ep_to_pipeRef(transfer->dev_handle, transfer->endpoint, &pipeRef, NULL, &cInterface);

        (*(cInterface->interface))->WritePipe(cInterface->interface, pipeRef, transfer->buffer, 0);
    }

    tpriv->result = result;
    tpriv->size   = (UInt32)(uintptr_t)arg0;

    /* signal the core that this transfer is complete */
    usbi_signal_transfer_completion(itransfer);
}

static enum libusb_transfer_status darwin_transfer_status(struct usbi_transfer *itransfer, IOReturn result) {
    if(itransfer->timeout_flags & USBI_TRANSFER_TIMED_OUT)
        result = kIOUSBTransactionTimeout;

    struct libusb_context *ctx = ITRANSFER_CTX(itransfer);

    switch(result) {
    case kIOReturnUnderrun:
    case kIOReturnSuccess:
        return LIBUSB_TRANSFER_COMPLETED;
    case kIOReturnAborted:
        return LIBUSB_TRANSFER_CANCELLED;
    case kIOUSBPipeStalled:
        usbi_dbg(ctx, "transfer error: pipe is stalled");
        return LIBUSB_TRANSFER_STALL;
    case kIOReturnOverrun:
        usbi_warn(ctx, "transfer error: data overrun");
        return LIBUSB_TRANSFER_OVERFLOW;
    case kIOUSBTransactionTimeout:
        usbi_warn(ctx, "transfer error: timed out");
        itransfer->timeout_flags |= USBI_TRANSFER_TIMED_OUT;
        return LIBUSB_TRANSFER_TIMED_OUT;
    default:
        usbi_warn(ctx, "transfer error: %s (value = 0x%08x)", darwin_error_str(result), result);
        return LIBUSB_TRANSFER_ERROR;
    }
}

static int darwin_handle_transfer_completion(struct usbi_transfer *itransfer) {
    struct libusb_transfer      *transfer          = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    struct darwin_transfer_priv *tpriv             = usbi_get_transfer_priv(itransfer);
    const unsigned char          max_transfer_type = LIBUSB_TRANSFER_TYPE_BULK_STREAM;
    const char                  *transfer_types[]  = { "control", "isoc", "bulk", "interrupt", "bulk-stream", NULL };
    bool                         is_isoc           = LIBUSB_TRANSFER_TYPE_ISOCHRONOUS == transfer->type;
    struct libusb_context       *ctx               = ITRANSFER_CTX(itransfer);

    if(transfer->type > max_transfer_type) {
        usbi_err(ctx, "unknown endpoint type %d", transfer->type);
        return LIBUSB_ERROR_INVALID_PARAM;
    }

    if(NULL == tpriv) {
        usbi_err(ctx, "malformed request is missing transfer priv");
        return LIBUSB_ERROR_INVALID_PARAM;
    }

    usbi_dbg(ctx, "handling transfer completion type %s with kernel status %d", transfer_types[transfer->type], tpriv->result);

    if(kIOReturnSuccess == tpriv->result || kIOReturnUnderrun == tpriv->result || kIOUSBTransactionTimeout == tpriv->result) {
        if(is_isoc && tpriv->isoc_framelist) {
            /* copy isochronous results back */

            for(int i = 0; i < transfer->num_iso_packets; i++) {
                struct libusb_iso_packet_descriptor *lib_desc = &transfer->iso_packet_desc[i];
                lib_desc->status                              = darwin_transfer_status(itransfer, tpriv->isoc_framelist[i].frStatus);
                lib_desc->actual_length                       = tpriv->isoc_framelist[i].frActCount;
            }
        }
        else if(!is_isoc) {
            itransfer->transferred += tpriv->size;
        }
    }

    /* it is ok to handle cancelled transfers without calling usbi_handle_transfer_cancellation (we catch timeout transfers) */
    return usbi_handle_transfer_completion(itransfer, darwin_transfer_status(itransfer, tpriv->result));
}

void usbi_get_monotonic_time(struct timespec *tp) {
/* Check if the SDK is new enough to declare clock_gettime(), and the deployment target is at least 10.12. */
#if((MAC_OS_X_VERSION_MAX_ALLOWED >= 101200) && (MAC_OS_X_VERSION_MIN_REQUIRED >= 101200))
    clock_gettime(CLOCK_MONOTONIC, tp);
#else
    mach_timebase_info_data_t machTimeBaseInfo;
    mach_timebase_info(&machTimeBaseInfo);

    uint64_t uptime     = mach_absolute_time();
    uint64_t uptimeNano = uptime * machTimeBaseInfo.numer / machTimeBaseInfo.denom;

    uint64_t uptimeSeconds       = uptimeNano / NSEC_PER_SEC;
    uint64_t uptimeNanoRemainder = uptimeNano - (uptimeSeconds * NSEC_PER_SEC);

    tp->tv_sec  = uptimeSeconds;
    tp->tv_nsec = uptimeNanoRemainder;
#endif
}

void usbi_get_real_time(struct timespec *tp) {
/* Check if the SDK is new enough to declare clock_gettime(), and the deployment target is at least 10.12. */
#if((MAC_OS_X_VERSION_MAX_ALLOWED >= 101200) && (MAC_OS_X_VERSION_MIN_REQUIRED >= 101200))
    clock_gettime(CLOCK_REALTIME, tp);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    tp->tv_sec  = tv.tv_sec;
    tp->tv_nsec = tv.tv_usec * NSEC_PER_USEC;
#endif
}

#if InterfaceVersion >= 550
static int darwin_alloc_streams(struct libusb_device_handle *dev_handle, uint32_t num_streams, unsigned char *endpoints, int num_endpoints) {
    struct darwin_interface *cInterface;
    UInt32                   supportsStreams;
    uint8_t                  pipeRef;
    int                      rc, i;

    /* find the minimum number of supported streams on the endpoint list */
    for(i = 0; i < num_endpoints; ++i) {
        if(0 != (rc = ep_to_pipeRef(dev_handle, endpoints[i], &pipeRef, NULL, &cInterface))) {
            return rc;
        }

        (*(cInterface->interface))->SupportsStreams(cInterface->interface, pipeRef, &supportsStreams);
        if(num_streams > supportsStreams)
            num_streams = supportsStreams;
    }

    /* it is an error if any endpoint in endpoints does not support streams */
    if(0 == num_streams)
        return LIBUSB_ERROR_INVALID_PARAM;

    /* create the streams */
    for(i = 0; i < num_endpoints; ++i) {
        (void)ep_to_pipeRef(dev_handle, endpoints[i], &pipeRef, NULL, &cInterface);

        rc = (*(cInterface->interface))->CreateStreams(cInterface->interface, pipeRef, num_streams);
        if(kIOReturnSuccess != rc)
            return darwin_to_libusb(rc);
    }

    assert(num_streams <= INT_MAX);
    return (int)num_streams;
}

static int darwin_free_streams(struct libusb_device_handle *dev_handle, unsigned char *endpoints, int num_endpoints) {
    struct darwin_interface *cInterface;
    UInt32                   supportsStreams;
    uint8_t                  pipeRef;
    int                      rc;

    for(int i = 0; i < num_endpoints; ++i) {
        if(0 != (rc = ep_to_pipeRef(dev_handle, endpoints[i], &pipeRef, NULL, &cInterface)))
            return rc;

        (*(cInterface->interface))->SupportsStreams(cInterface->interface, pipeRef, &supportsStreams);
        if(0 == supportsStreams)
            return LIBUSB_ERROR_INVALID_PARAM;

        rc = (*(cInterface->interface))->CreateStreams(cInterface->interface, pipeRef, 0);
        if(kIOReturnSuccess != rc)
            return darwin_to_libusb(rc);
    }

    return LIBUSB_SUCCESS;
}
#endif

#if InterfaceVersion >= 700

/* macOS APIs for getting entitlement values */

#if !defined(TARGET_OS_OSX) || TARGET_OS_OSX == 1
#include <Security/Security.h>
#else
typedef struct __SecTask *SecTaskRef;
extern SecTaskRef         SecTaskCreateFromSelf(CFAllocatorRef allocator);
extern CFTypeRef          SecTaskCopyValueForEntitlement(SecTaskRef task, CFStringRef entitlement, CFErrorRef *error);
#endif

static bool darwin_has_capture_entitlements(void) {
    SecTaskRef task;
    CFTypeRef  value;
    bool       entitled;

    task = SecTaskCreateFromSelf(kCFAllocatorDefault);
    if(task == NULL) {
        return false;
    }
    value = SecTaskCopyValueForEntitlement(task, CFSTR("com.apple.vm.device-access"), NULL);
    CFRelease(task);
    entitled = value && (CFGetTypeID(value) == CFBooleanGetTypeID()) && CFBooleanGetValue(value);
    if(value) {
        CFRelease(value);
    }
    return entitled;
}

static int darwin_reload_device(struct libusb_device_handle *dev_handle) {
    struct darwin_cached_device *dpriv = DARWIN_CACHED_DEVICE(dev_handle->dev);
    enum libusb_error            err;

    usbi_mutex_lock(&darwin_cached_devices_mutex);
    (*(dpriv->device))->Release(dpriv->device);
    dpriv->device = darwin_device_from_service(HANDLE_CTX(dev_handle), dpriv->service);
    if(!dpriv->device) {
        err = LIBUSB_ERROR_NO_DEVICE;
    }
    else {
        err = LIBUSB_SUCCESS;
    }
    usbi_mutex_unlock(&darwin_cached_devices_mutex);

    return err;
}

/* On macOS, we capture an entire device at once, not individual interfaces. */

static int darwin_detach_kernel_driver(struct libusb_device_handle *dev_handle, uint8_t interface) {
    UNUSED(interface);
    struct darwin_cached_device *dpriv = DARWIN_CACHED_DEVICE(dev_handle->dev);
    IOReturn                     kresult;
    enum libusb_error            err;
    struct libusb_context       *ctx = HANDLE_CTX(dev_handle);

    if(HAS_CAPTURE_DEVICE()) {
    }
    else {
        return LIBUSB_ERROR_NOT_SUPPORTED;
    }

    if(dpriv->capture_count == 0) {
        usbi_dbg(ctx, "attempting to detach kernel driver from device");

        if(darwin_has_capture_entitlements()) {
            /* request authorization */
            kresult = IOServiceAuthorize(dpriv->service, kIOServiceInteractionAllowed);
            if(kresult != kIOReturnSuccess) {
                usbi_warn(ctx, "IOServiceAuthorize: %s", darwin_error_str(kresult));
                return darwin_to_libusb(kresult);
            }

            /* we need start() to be called again for authorization status to refresh */
            err = darwin_reload_device(dev_handle);
            if(err != LIBUSB_SUCCESS) {
                return err;
            }
        }
        else {
            usbi_info(ctx, "no capture entitlements. may not be able to detach the kernel driver for this device");
            if(0 != geteuid()) {
                usbi_warn(ctx, "USB device capture requires either an entitlement (com.apple.vm.device-access) or root privilege");
                return LIBUSB_ERROR_ACCESS;
            }
        }

        /* reset device to release existing drivers */
        err = darwin_reenumerate_device(dev_handle, true);
        if(err != LIBUSB_SUCCESS) {
            return err;
        }
    }
    dpriv->capture_count++;
    return LIBUSB_SUCCESS;
}

static int darwin_attach_kernel_driver(struct libusb_device_handle *dev_handle, uint8_t interface) {
    UNUSED(interface);
    struct darwin_cached_device *dpriv = DARWIN_CACHED_DEVICE(dev_handle->dev);

    if(HAS_CAPTURE_DEVICE()) {
    }
    else {
        return LIBUSB_ERROR_NOT_SUPPORTED;
    }

    dpriv->capture_count--;
    if(dpriv->capture_count > 0) {
        return LIBUSB_SUCCESS;
    }

    usbi_dbg(HANDLE_CTX(dev_handle), "reenumerating device for kernel driver attach");

    /* reset device to attach kernel drivers */
    return darwin_reenumerate_device(dev_handle, false);
}

static int darwin_capture_claim_interface(struct libusb_device_handle *dev_handle, uint8_t iface) {
    enum libusb_error ret;
    if(dev_handle->auto_detach_kernel_driver && darwin_kernel_driver_active(dev_handle, iface)) {
        ret = darwin_detach_kernel_driver(dev_handle, iface);
        if(ret != LIBUSB_SUCCESS) {
            usbi_info(HANDLE_CTX(dev_handle), "failed to auto-detach the kernel driver for this device, ret=%d", ret);
        }
    }

    return darwin_claim_interface(dev_handle, iface);
}

static int darwin_capture_release_interface(struct libusb_device_handle *dev_handle, uint8_t iface) {
    enum libusb_error            ret;
    struct darwin_cached_device *dpriv = DARWIN_CACHED_DEVICE(dev_handle->dev);

    ret = darwin_release_interface(dev_handle, iface);
    if(ret != LIBUSB_SUCCESS) {
        return ret;
    }

    if(dev_handle->auto_detach_kernel_driver && dpriv->capture_count > 0) {
        ret = darwin_attach_kernel_driver(dev_handle, iface);
        if(LIBUSB_SUCCESS != ret) {
            usbi_info(HANDLE_CTX(dev_handle), "on attempt to reattach the kernel driver got ret=%d", ret);
        }
        /* ignore the error as the interface was successfully released */
    }

    return LIBUSB_SUCCESS;
}

#endif

const struct usbi_os_backend usbi_backend = {
    .name                         = "Darwin",
    .caps                         = USBI_CAP_SUPPORTS_DETACH_KERNEL_DRIVER,
    .init                         = darwin_init,
    .exit                         = darwin_exit,
    .get_active_config_descriptor = darwin_get_active_config_descriptor,
    .get_config_descriptor        = darwin_get_config_descriptor,
    .hotplug_poll                 = darwin_hotplug_poll,

    .open              = darwin_open,
    .close             = darwin_close,
    .get_configuration = darwin_get_configuration,
    .set_configuration = darwin_set_configuration,

    .set_interface_altsetting = darwin_set_interface_altsetting,
    .clear_halt               = darwin_clear_halt,
    .reset_device             = darwin_reset_device,

#if InterfaceVersion >= 550
    .alloc_streams = darwin_alloc_streams,
    .free_streams  = darwin_free_streams,
#endif

    .kernel_driver_active = darwin_kernel_driver_active,

#if InterfaceVersion >= 700
    .detach_kernel_driver = darwin_detach_kernel_driver,
    .attach_kernel_driver = darwin_attach_kernel_driver,
    .claim_interface      = darwin_capture_claim_interface,
    .release_interface    = darwin_capture_release_interface,
#else
    .claim_interface   = darwin_claim_interface,
    .release_interface = darwin_release_interface,
#endif

    .destroy_device = darwin_destroy_device,

    .submit_transfer = darwin_submit_transfer,
    .cancel_transfer = darwin_cancel_transfer,

    .handle_transfer_completion = darwin_handle_transfer_completion,

    .device_priv_size        = sizeof(struct darwin_device_priv),
    .device_handle_priv_size = sizeof(struct darwin_device_handle_priv),
    .transfer_priv_size      = sizeof(struct darwin_transfer_priv),
};
