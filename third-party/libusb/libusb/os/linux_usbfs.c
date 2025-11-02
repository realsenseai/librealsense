/* -*- Mode: C; c-basic-offset:8 ; indent-tabs-mode:t -*- */
/*
 * Linux usbfs backend for libusb
 * Copyright © 2007-2009 Daniel Drake <dsd@gentoo.org>
 * Copyright © 2001 Johannes Erdfelt <johannes@erdfelt.com>
 * Copyright © 2013 Nathan Hjelm <hjelmn@mac.com>
 * Copyright © 2012-2013 Hans de Goede <hdegoede@redhat.com>
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
#include "linux_usbfs.h"

#include <alloca.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <sys/vfs.h>
#include <unistd.h>

/* sysfs vs usbfs:
 * opening a usbfs node causes the device to be resumed, so we attempt to
 * avoid this during enumeration.
 *
 * sysfs allows us to read the kernel's in-memory copies of device descriptors
 * and so forth, avoiding the need to open the device:
 *  - The binary "descriptors" file contains all config descriptors since
 *    2.6.26, commit 217a9081d8e69026186067711131b77f0ce219ed
 *  - The binary "descriptors" file was added in 2.6.23, commit
 *    69d42a78f935d19384d1f6e4f94b65bb162b36df, but it only contains the
 *    active config descriptors
 *  - The "busnum" file was added in 2.6.22, commit
 *    83f7d958eab2fbc6b159ee92bf1493924e1d0f72
 *  - The "devnum" file has been present since pre-2.6.18
 *  - the "bConfigurationValue" file has been present since pre-2.6.18
 *
 * If we have bConfigurationValue, busnum, and devnum, then we can determine
 * the active configuration without having to open the usbfs node in RDWR mode.
 * The busnum file is important as that is the only way we can relate sysfs
 * devices to usbfs nodes.
 *
 * If we also have all descriptors, we can obtain the device descriptor and
 * configuration without touching usbfs at all.
 */

/* endianness for multi-byte fields:
 *
 * Descriptors exposed by usbfs have the multi-byte fields in the device
 * descriptor as host endian. Multi-byte fields in the other descriptors are
 * bus-endian. The kernel documentation says otherwise, but it is wrong.
 *
 * In sysfs all descriptors are bus-endian.
 */

#define USBDEV_PATH "/dev"
#define USB_DEVTMPFS_PATH "/dev/bus/usb"

/* use usbdev*.* device names in /dev instead of the usbfs bus directories */
static int usbdev_names = 0;

/* Linux has changed the maximum length of an individual isochronous packet
 * over time.  Initially this limit was 1,023 bytes, but Linux 2.6.18
 * (commit 3612242e527eb47ee4756b5350f8bdf791aa5ede) increased this value to
 * 8,192 bytes to support higher bandwidth devices.  Linux 3.10
 * (commit e2e2f0ea1c935edcf53feb4c4c8fdb4f86d57dd9) further increased this
 * value to 49,152 bytes to support super speed devices.  Linux 5.2
 * (commit 8a1dbc8d91d3d1602282c7e6b4222c7759c916fa) even further increased
 * this value to 98,304 bytes to support super speed plus devices.
 */
static unsigned int max_iso_packet_len = 0;

/* is sysfs available (mounted) ? */
static int sysfs_available = -1;

/* how many times have we initted (and not exited) ? */
static int init_count = 0;

/* Serialize scan-devices, event-thread, and poll */
usbi_mutex_static_t linux_hotplug_lock = USBI_MUTEX_INITIALIZER;

static int linux_scan_devices(struct libusb_context *ctx);
static int detach_kernel_driver_and_claim(struct libusb_device_handle *, uint8_t);

#if !defined(HAVE_LIBUDEV)
static int linux_default_scan_devices(struct libusb_context *ctx);
#endif

struct kernel_version {
    int major;
    int minor;
    int sublevel;
};

struct config_descriptor {
    struct usbi_configuration_descriptor *desc;
    size_t                                actual_len;
};

struct linux_context_priv {
    /* no enumeration or hot-plug detection */
    int no_device_discovery;
};

struct linux_device_priv {
    char                     *sysfs_dir;
    void                     *descriptors;
    size_t                    descriptors_len;
    struct config_descriptor *config_descriptors;
    int                       active_config; /* cache val for !sysfs_available  */
};

struct linux_device_handle_priv {
    int      fd;
    int      fd_removed;
    int      fd_keep;
    uint32_t caps;
};

enum reap_action {
    NORMAL = 0,
    /* submission failed after the first URB, so await cancellation/completion
     * of all the others */
    SUBMIT_FAILED,

    /* cancelled by user or timeout */
    CANCELLED,

    /* completed multi-URB transfer in non-final URB */
    COMPLETED_EARLY,

    /* one or more urbs encountered a low-level error */
    ERROR,
};

struct linux_transfer_priv {
    union {
        struct usbfs_urb  *urbs;
        struct usbfs_urb **iso_urbs;
    };

    enum reap_action            reap_action;
    int                         num_urbs;
    int                         num_retired;
    enum libusb_transfer_status reap_status;

    /* next iso packet in user-supplied transfer to be populated */
    int iso_packet_offset;
};

static int dev_has_config0(struct libusb_device *dev) {
    struct linux_device_priv *priv = usbi_get_device_priv(dev);
    struct config_descriptor *config;
    uint8_t                   idx;

    for(idx = 0; idx < dev->device_descriptor.bNumConfigurations; idx++) {
        config = &priv->config_descriptors[idx];
        if(config->desc->bConfigurationValue == 0)
            return 1;
    }

    return 0;
}

static int get_usbfs_fd(struct libusb_device *dev, mode_t mode, int silent) {
    struct libusb_context *ctx = DEVICE_CTX(dev);
    char                   path[24];
    int                    fd;

    if(usbdev_names)
        snprintf(path, sizeof(path), USBDEV_PATH "/usbdev%u.%u", dev->bus_number, dev->device_address);
    else
        snprintf(path, sizeof(path), USB_DEVTMPFS_PATH "/%03u/%03u", dev->bus_number, dev->device_address);

    fd = open(path, mode | O_CLOEXEC);
    if(fd != -1)
        return fd; /* Success */

    if(errno == ENOENT) {
        const long            delay_ms = 10L;
        const struct timespec delay_ts = { 0L, delay_ms * 1000L * 1000L };

        if(!silent)
            usbi_err(ctx, "File doesn't exist, wait %ld ms and try again", delay_ms);

        /* Wait 10ms for USB device path creation.*/
        nanosleep(&delay_ts, NULL);

        fd = open(path, mode | O_CLOEXEC);
        if(fd != -1)
            return fd; /* Success */
    }

    if(!silent) {
        usbi_err(ctx, "libusb couldn't open USB device %s, errno=%d", path, errno);
        if(errno == EACCES && mode == O_RDWR)
            usbi_err(ctx, "libusb requires write access to USB device nodes");
    }

    if(errno == EACCES)
        return LIBUSB_ERROR_ACCESS;
    if(errno == ENOENT)
        return LIBUSB_ERROR_NO_DEVICE;
    return LIBUSB_ERROR_IO;
}

/* check dirent for a /dev/usbdev%d.%d name
 * optionally return bus/device on success */
static int is_usbdev_entry(const char *name, uint8_t *bus_p, uint8_t *dev_p) {
    int busnum, devnum;

    if(sscanf(name, "usbdev%d.%d", &busnum, &devnum) != 2)
        return 0;
    if(busnum < 0 || busnum > UINT8_MAX || devnum < 0 || devnum > UINT8_MAX) {
        usbi_dbg(NULL, "invalid usbdev format '%s'", name);
        return 0;
    }

    usbi_dbg(NULL, "found: %s", name);
    if(bus_p)
        *bus_p = (uint8_t)busnum;
    if(dev_p)
        *dev_p = (uint8_t)devnum;
    return 1;
}

static const char *find_usbfs_path(void) {
    const char    *path;
    DIR           *dir;
    struct dirent *entry;

    path = USB_DEVTMPFS_PATH;
    dir  = opendir(path);
    if(dir) {
        while((entry = readdir(dir))) {
            if(entry->d_name[0] == '.')
                continue;

            /* We assume if we find any files that it must be the right place */
            break;
        }

        closedir(dir);

        if(entry)
            return path;
    }

    /* look for /dev/usbdev*.* if the normal place fails */
    path = USBDEV_PATH;
    dir  = opendir(path);
    if(dir) {
        while((entry = readdir(dir))) {
            if(entry->d_name[0] == '.')
                continue;

            if(is_usbdev_entry(entry->d_name, NULL, NULL)) {
                /* found one; that's enough */
                break;
            }
        }

        closedir(dir);

        if(entry) {
            usbdev_names = 1;
            return path;
        }
    }

/* On udev based systems without any usb-devices /dev/bus/usb will not
 * exist. So if we've not found anything and we're using udev for hotplug
 * simply assume /dev/bus/usb rather then making libusb_init fail.
 * Make the same assumption for Android where SELinux policies might block us
 * from reading /dev on newer devices. */
#if defined(HAVE_LIBUDEV) || defined(__ANDROID__)
    return USB_DEVTMPFS_PATH;
#else
    return NULL;
#endif
}

static int get_kernel_version(struct libusb_context *ctx, struct kernel_version *ver) {
    struct utsname uts;
    int            atoms;

    if(uname(&uts) < 0) {
        usbi_err(ctx, "uname failed, errno=%d", errno);
        return -1;
    }

    atoms = sscanf(uts.release, "%d.%d.%d", &ver->major, &ver->minor, &ver->sublevel);
    if(atoms < 2) {
        usbi_err(ctx, "failed to parse uname release '%s'", uts.release);
        return -1;
    }

    if(atoms < 3)
        ver->sublevel = -1;

    usbi_dbg(ctx, "reported kernel version is %s", uts.release);

    return 0;
}

static int kernel_version_ge(const struct kernel_version *ver, int major, int minor, int sublevel) {
    if(ver->major > major)
        return 1;
    else if(ver->major < major)
        return 0;

    /* kmajor == major */
    if(ver->minor > minor)
        return 1;
    else if(ver->minor < minor)
        return 0;

    /* kminor == minor */
    if(ver->sublevel == -1)
        return sublevel == 0;

    return ver->sublevel >= sublevel;
}

static int op_init(struct libusb_context *ctx) {
    struct kernel_version      kversion;
    const char                *usbfs_path;
    int                        r;
    struct linux_context_priv *cpriv = usbi_get_context_priv(ctx);

    if(get_kernel_version(ctx, &kversion) < 0)
        return LIBUSB_ERROR_OTHER;

    if(!kernel_version_ge(&kversion, 2, 6, 32)) {
        usbi_err(ctx, "kernel version is too old (reported as %d.%d.%d)", kversion.major, kversion.minor, kversion.sublevel != -1 ? kversion.sublevel : 0);
        return LIBUSB_ERROR_NOT_SUPPORTED;
    }

    usbfs_path = find_usbfs_path();
    if(!usbfs_path) {
        usbi_err(ctx, "could not find usbfs");
        return LIBUSB_ERROR_OTHER;
    }

    usbi_dbg(ctx, "found usbfs at %s", usbfs_path);

    if(!max_iso_packet_len) {
        if(kernel_version_ge(&kversion, 5, 2, 0))
            max_iso_packet_len = 98304;
        else if(kernel_version_ge(&kversion, 3, 10, 0))
            max_iso_packet_len = 49152;
        else
            max_iso_packet_len = 8192;
    }

    usbi_dbg(ctx, "max iso packet length is (likely) %u bytes", max_iso_packet_len);

    if(sysfs_available == -1) {
        struct statfs statfsbuf;

        r = statfs(SYSFS_MOUNT_PATH, &statfsbuf);
        if(r == 0 && statfsbuf.f_type == SYSFS_MAGIC) {
            usbi_dbg(ctx, "sysfs is available");
            sysfs_available = 1;
        }
        else {
            usbi_warn(ctx, "sysfs not mounted");
            sysfs_available = 0;
        }
    }

    if(cpriv->no_device_discovery) {
        return LIBUSB_SUCCESS;
    }

    r = LIBUSB_SUCCESS;
    if(init_count == 0) {
        /* start up hotplug event handler */
        r = linux_start_event_monitor();
    }
    if(r == LIBUSB_SUCCESS) {
        r = linux_scan_devices(ctx);
        if(r == LIBUSB_SUCCESS)
            init_count++;
        else if(init_count == 0)
            linux_stop_event_monitor();
    }
    else {
        usbi_err(ctx, "error starting hotplug event monitor");
    }

    return r;
}

static void op_exit(struct libusb_context *ctx) {
    struct linux_context_priv *cpriv = usbi_get_context_priv(ctx);

    if(cpriv->no_device_discovery) {
        return;
    }

    assert(init_count != 0);
    if(!--init_count) {
        /* tear down event handler */
        linux_stop_event_monitor();
    }
}

static int op_set_option(struct libusb_context *ctx, enum libusb_option option, va_list ap) {
    UNUSED(ap);

    if(option == LIBUSB_OPTION_NO_DEVICE_DISCOVERY) {
        struct linux_context_priv *cpriv = usbi_get_context_priv(ctx);

        usbi_dbg(ctx, "no device discovery will be performed");
        cpriv->no_device_discovery = 1;
        return LIBUSB_SUCCESS;
    }

    return LIBUSB_ERROR_NOT_SUPPORTED;
}

static int linux_scan_devices(struct libusb_context *ctx) {
    int ret;

    usbi_mutex_static_lock(&linux_hotplug_lock);

#if defined(HAVE_LIBUDEV)
    ret = linux_udev_scan_devices(ctx);
#else
    ret = linux_default_scan_devices(ctx);
#endif

    usbi_mutex_static_unlock(&linux_hotplug_lock);

    return ret;
}

static void op_hotplug_poll(void) {
    linux_hotplug_poll();
}

static int open_sysfs_attr(struct libusb_context *ctx, const char *sysfs_dir, const char *attr) {
    char filename[256];
    int  fd;

    snprintf(filename, sizeof(filename), SYSFS_DEVICE_PATH "/%s/%s", sysfs_dir, attr);
    fd = open(filename, O_RDONLY | O_CLOEXEC);
    if(fd < 0) {
        if(errno == ENOENT) {
            /* File doesn't exist. Assume the device has been
               disconnected (see trac ticket #70). */
            return LIBUSB_ERROR_NO_DEVICE;
        }
        usbi_err(ctx, "open %s failed, errno=%d", filename, errno);
        return LIBUSB_ERROR_IO;
    }

    return fd;
}

/* Note only suitable for attributes which always read >= 0, < 0 is error */
static int read_sysfs_attr(struct libusb_context *ctx, const char *sysfs_dir, const char *attr, int max_value, int *value_p) {
    char    buf[20], *endptr;
    long    value;
    ssize_t r;
    int     fd;

    fd = open_sysfs_attr(ctx, sysfs_dir, attr);
    if(fd < 0)
        return fd;

    r = read(fd, buf, sizeof(buf) - 1);
    if(r < 0) {
        r = errno;
        close(fd);
        if(r == ENODEV)
            return LIBUSB_ERROR_NO_DEVICE;
        usbi_err(ctx, "attribute %s read failed, errno=%zd", attr, r);
        return LIBUSB_ERROR_IO;
    }
    close(fd);

    if(r == 0) {
        /* Certain attributes (e.g. bConfigurationValue) are not
         * populated if the device is not configured. */
        *value_p = -1;
        return 0;
    }

    /* The kernel does *not* NUL-terminate the string, but every attribute
     * should be terminated with a newline character. */
    if(!isdigit(buf[0])) {
        usbi_err(ctx, "attribute %s doesn't have numeric value?", attr);
        return LIBUSB_ERROR_IO;
    }
    else if(buf[r - 1] != '\n') {
        usbi_warn(ctx, "attribute %s doesn't end with newline?", attr);
    }
    else {
        /* Remove the terminating newline character */
        r--;
    }
    buf[r] = '\0';

    errno = 0;
    value = strtol(buf, &endptr, 10);
    if(value < 0 || value > (long)max_value || errno) {
        usbi_err(ctx, "attribute %s contains an invalid value: '%s'", attr, buf);
        return LIBUSB_ERROR_INVALID_PARAM;
    }
    else if(*endptr != '\0') {
        /* Consider the value to be valid if the remainder is a '.'
         * character followed by numbers.  This occurs, for example,
         * when reading the "speed" attribute for a low-speed device
         * (e.g. "1.5") */
        if(*endptr == '.' && isdigit(*(endptr + 1))) {
            endptr++;
            while(isdigit(*endptr))
                endptr++;
        }
        if(*endptr != '\0') {
            usbi_err(ctx, "attribute %s contains an invalid value: '%s'", attr, buf);
            return LIBUSB_ERROR_INVALID_PARAM;
        }
    }

    *value_p = (int)value;
    return 0;
}

static int sysfs_scan_device(struct libusb_context *ctx, const char *devname) {
    uint8_t busnum, devaddr;
    int     ret;

    ret = linux_get_device_address(ctx, 0, &busnum, &devaddr, NULL, devname, -1);
    if(ret != LIBUSB_SUCCESS)
        return ret;

    return linux_enumerate_device(ctx, busnum, devaddr, devname);
}

/* read the bConfigurationValue for a device */
static int sysfs_get_active_config(struct libusb_device *dev, int *config) {
    struct linux_device_priv *priv = usbi_get_device_priv(dev);

    return read_sysfs_attr(DEVICE_CTX(dev), priv->sysfs_dir, "bConfigurationValue", UINT8_MAX, config);
}

int linux_get_device_address(struct libusb_context *ctx, int detached, uint8_t *busnum, uint8_t *devaddr, const char *dev_node, const char *sys_name, int fd) {
    int sysfs_val;
    int r;

    usbi_dbg(ctx, "getting address for device: %s detached: %d", sys_name, detached);
    /* can't use sysfs to read the bus and device number if the
     * device has been detached */
    if(!sysfs_available || detached || !sys_name) {
        if(!dev_node && fd >= 0) {
            char *fd_path = alloca(PATH_MAX);
            char  proc_path[32];

            /* try to retrieve the device node from fd */
            snprintf(proc_path, sizeof(proc_path), "/proc/self/fd/%d", fd);
            r = readlink(proc_path, fd_path, PATH_MAX - 1);
            if(r > 0) {
                fd_path[r] = '\0';
                dev_node   = fd_path;
            }
        }

        if(!dev_node)
            return LIBUSB_ERROR_OTHER;

        /* will this work with all supported kernel versions? */
        if(!strncmp(dev_node, "/dev/bus/usb", 12))
            sscanf(dev_node, "/dev/bus/usb/%hhu/%hhu", busnum, devaddr);
        else
            return LIBUSB_ERROR_OTHER;

        return LIBUSB_SUCCESS;
    }

    usbi_dbg(ctx, "scan %s", sys_name);

    r = read_sysfs_attr(ctx, sys_name, "busnum", UINT8_MAX, &sysfs_val);
    if(r < 0)
        return r;
    *busnum = (uint8_t)sysfs_val;

    r = read_sysfs_attr(ctx, sys_name, "devnum", UINT8_MAX, &sysfs_val);
    if(r < 0)
        return r;
    *devaddr = (uint8_t)sysfs_val;

    usbi_dbg(ctx, "bus=%u dev=%u", *busnum, *devaddr);

    return LIBUSB_SUCCESS;
}

/* Return offset of the next config descriptor */
static int seek_to_next_config(struct libusb_context *ctx, uint8_t *buffer, size_t len) {
    struct usbi_descriptor_header *header;
    int                            offset;

    /* Start seeking past the config descriptor */
    offset = LIBUSB_DT_CONFIG_SIZE;
    buffer += LIBUSB_DT_CONFIG_SIZE;
    len -= LIBUSB_DT_CONFIG_SIZE;

    while(len > 0) {
        if(len < 2) {
            usbi_err(ctx, "short descriptor read %zu/2", len);
            return LIBUSB_ERROR_IO;
        }

        header = (struct usbi_descriptor_header *)buffer;
        if(header->bDescriptorType == LIBUSB_DT_CONFIG)
            return offset;

        if(len < header->bLength) {
            usbi_err(ctx, "bLength overflow by %zu bytes", (size_t)header->bLength - len);
            return LIBUSB_ERROR_IO;
        }

        offset += header->bLength;
        buffer += header->bLength;
        len -= header->bLength;
    }

    usbi_err(ctx, "config descriptor not found");
    return LIBUSB_ERROR_IO;
}

static int parse_config_descriptors(struct libusb_device *dev) {
    struct libusb_context         *ctx  = DEVICE_CTX(dev);
    struct linux_device_priv      *priv = usbi_get_device_priv(dev);
    struct usbi_device_descriptor *device_desc;
    uint8_t                        idx, num_configs;
    uint8_t                       *buffer;
    size_t                         remaining;

    device_desc = priv->descriptors;
    num_configs = device_desc->bNumConfigurations;

    if(num_configs == 0)
        return 0; /* no configurations? */

    priv->config_descriptors = malloc(num_configs * sizeof(priv->config_descriptors[0]));
    if(!priv->config_descriptors)
        return LIBUSB_ERROR_NO_MEM;

    buffer    = (uint8_t *)priv->descriptors + LIBUSB_DT_DEVICE_SIZE;
    remaining = priv->descriptors_len - LIBUSB_DT_DEVICE_SIZE;

    for(idx = 0; idx < num_configs; idx++) {
        struct usbi_configuration_descriptor *config_desc;
        uint16_t                              config_len;

        if(remaining < LIBUSB_DT_CONFIG_SIZE) {
            usbi_err(ctx, "short descriptor read %zu/%d", remaining, LIBUSB_DT_CONFIG_SIZE);
            return LIBUSB_ERROR_IO;
        }

        config_desc = (struct usbi_configuration_descriptor *)buffer;
        if(config_desc->bDescriptorType != LIBUSB_DT_CONFIG) {
            usbi_err(ctx, "descriptor is not a config desc (type 0x%02x)", config_desc->bDescriptorType);
            return LIBUSB_ERROR_IO;
        }
        else if(config_desc->bLength < LIBUSB_DT_CONFIG_SIZE) {
            usbi_err(ctx, "invalid descriptor bLength %u", config_desc->bLength);
            return LIBUSB_ERROR_IO;
        }

        config_len = libusb_le16_to_cpu(config_desc->wTotalLength);
        if(config_len < LIBUSB_DT_CONFIG_SIZE) {
            usbi_err(ctx, "invalid wTotalLength %u", config_len);
            return LIBUSB_ERROR_IO;
        }

        if(priv->sysfs_dir) {
            /*
             * In sysfs wTotalLength is ignored, instead the kernel returns a
             * config descriptor with verified bLength fields, with descriptors
             * with an invalid bLength removed.
             */
            uint16_t sysfs_config_len;
            int      offset;

            if(num_configs > 1 && idx < num_configs - 1) {
                offset = seek_to_next_config(ctx, buffer, remaining);
                if(offset < 0)
                    return offset;
                sysfs_config_len = (uint16_t)offset;
            }
            else {
                sysfs_config_len = (uint16_t)remaining;
            }

            if(config_len != sysfs_config_len) {
                usbi_warn(ctx, "config length mismatch wTotalLength %u real %u", config_len, sysfs_config_len);
                config_len = sysfs_config_len;
            }
        }
        else {
            /*
             * In usbfs the config descriptors are wTotalLength bytes apart,
             * with any short reads from the device appearing as holes in the file.
             */
            if(config_len > remaining) {
                usbi_warn(ctx, "short descriptor read %zu/%u", remaining, config_len);
                config_len = (uint16_t)remaining;
            }
        }

        if(config_desc->bConfigurationValue == 0)
            usbi_warn(ctx, "device has configuration 0");

        priv->config_descriptors[idx].desc       = config_desc;
        priv->config_descriptors[idx].actual_len = config_len;

        buffer += config_len;
        remaining -= config_len;
    }

    return LIBUSB_SUCCESS;
}

static int op_get_config_descriptor_by_value(struct libusb_device *dev, uint8_t value, void **buffer) {
    struct linux_device_priv *priv = usbi_get_device_priv(dev);
    struct config_descriptor *config;
    uint8_t                   idx;

    for(idx = 0; idx < dev->device_descriptor.bNumConfigurations; idx++) {
        config = &priv->config_descriptors[idx];
        if(config->desc->bConfigurationValue == value) {
            *buffer = config->desc;
            return (int)config->actual_len;
        }
    }

    return LIBUSB_ERROR_NOT_FOUND;
}

static int op_get_active_config_descriptor(struct libusb_device *dev, void *buffer, size_t len) {
    struct linux_device_priv *priv = usbi_get_device_priv(dev);
    void                     *config_desc;
    int                       active_config;
    int                       r;

    if(priv->sysfs_dir) {
        r = sysfs_get_active_config(dev, &active_config);
        if(r < 0)
            return r;
    }
    else {
        /* Use cached bConfigurationValue */
        active_config = priv->active_config;
    }

    if(active_config == -1) {
        usbi_err(DEVICE_CTX(dev), "device unconfigured");
        return LIBUSB_ERROR_NOT_FOUND;
    }

    r = op_get_config_descriptor_by_value(dev, (uint8_t)active_config, &config_desc);
    if(r < 0)
        return r;

    len = MIN(len, (size_t)r);
    memcpy(buffer, config_desc, len);
    return len;
}

static int op_get_config_descriptor(struct libusb_device *dev, uint8_t config_index, void *buffer, size_t len) {
    struct linux_device_priv *priv = usbi_get_device_priv(dev);
    struct config_descriptor *config;

    if(config_index >= dev->device_descriptor.bNumConfigurations)
        return LIBUSB_ERROR_NOT_FOUND;

    config = &priv->config_descriptors[config_index];
    len    = MIN(len, config->actual_len);
    memcpy(buffer, config->desc, len);
    return len;
}

/* send a control message to retrieve active configuration */
static int usbfs_get_active_config(struct libusb_device *dev, int fd) {
    struct linux_device_priv *priv          = usbi_get_device_priv(dev);
    uint8_t                   active_config = 0;
    int                       r;

    struct usbfs_ctrltransfer ctrl = { .bmRequestType = LIBUSB_ENDPOINT_IN,
                                       .bRequest      = LIBUSB_REQUEST_GET_CONFIGURATION,
                                       .wValue        = 0,
                                       .wIndex        = 0,
                                       .wLength       = 1,
                                       .timeout       = 1000,
                                       .data          = &active_config };

    r = ioctl(fd, IOCTL_USBFS_CONTROL, &ctrl);
    if(r < 0) {
        if(errno == ENODEV)
            return LIBUSB_ERROR_NO_DEVICE;

        /* we hit this error path frequently with buggy devices :( */
        usbi_warn(DEVICE_CTX(dev), "get configuration failed, errno=%d", errno);

        /* assume the current configuration is the first one if we have
         * the configuration descriptors, otherwise treat the device
         * as unconfigured. */
        if(priv->config_descriptors)
            priv->active_config = (int)priv->config_descriptors[0].desc->bConfigurationValue;
        else
            priv->active_config = -1;
    }
    else if(active_config == 0) {
        if(dev_has_config0(dev)) {
            /* some buggy devices have a configuration 0, but we're
             * reaching into the corner of a corner case here. */
            priv->active_config = 0;
        }
        else {
            priv->active_config = -1;
        }
    }
    else {
        priv->active_config = (int)active_config;
    }

    return LIBUSB_SUCCESS;
}

static enum libusb_speed usbfs_get_speed(struct libusb_context *ctx, int fd) {
    int r;

    r = ioctl(fd, IOCTL_USBFS_GET_SPEED, NULL);
    switch(r) {
    case USBFS_SPEED_UNKNOWN:
        return LIBUSB_SPEED_UNKNOWN;
    case USBFS_SPEED_LOW:
        return LIBUSB_SPEED_LOW;
    case USBFS_SPEED_FULL:
        return LIBUSB_SPEED_FULL;
    case USBFS_SPEED_HIGH:
        return LIBUSB_SPEED_HIGH;
    case USBFS_SPEED_WIRELESS:
        return LIBUSB_SPEED_HIGH;
    case USBFS_SPEED_SUPER:
        return LIBUSB_SPEED_SUPER;
    case USBFS_SPEED_SUPER_PLUS:
        return LIBUSB_SPEED_SUPER_PLUS;
    default:
        usbi_warn(ctx, "Error getting device speed: %d", r);
    }

    return LIBUSB_SPEED_UNKNOWN;
}

static int initialize_device(struct libusb_device *dev, uint8_t busnum, uint8_t devaddr, const char *sysfs_dir, int wrapped_fd) {
    struct linux_device_priv *priv = usbi_get_device_priv(dev);
    struct libusb_context    *ctx  = DEVICE_CTX(dev);
    size_t                    alloc_len;
    int                       fd, speed, r;
    ssize_t                   nb;

    dev->bus_number     = busnum;
    dev->device_address = devaddr;

    if(sysfs_dir) {
        priv->sysfs_dir = strdup(sysfs_dir);
        if(!priv->sysfs_dir)
            return LIBUSB_ERROR_NO_MEM;

        /* Note speed can contain 1.5, in this case read_sysfs_attr()
           will stop parsing at the '.' and return 1 */
        if(read_sysfs_attr(ctx, sysfs_dir, "speed", INT_MAX, &speed) == 0) {
            switch(speed) {
            case 1:
                dev->speed = LIBUSB_SPEED_LOW;
                break;
            case 12:
                dev->speed = LIBUSB_SPEED_FULL;
                break;
            case 480:
                dev->speed = LIBUSB_SPEED_HIGH;
                break;
            case 5000:
                dev->speed = LIBUSB_SPEED_SUPER;
                break;
            case 10000:
                dev->speed = LIBUSB_SPEED_SUPER_PLUS;
                break;
            default:
                usbi_warn(ctx, "unknown device speed: %d Mbps", speed);
            }
        }
    }
    else if(wrapped_fd >= 0) {
        dev->speed = usbfs_get_speed(ctx, wrapped_fd);
    }

    /* cache descriptors in memory */
    if(sysfs_dir) {
        fd = open_sysfs_attr(ctx, sysfs_dir, "descriptors");
    }
    else if(wrapped_fd < 0) {
        fd = get_usbfs_fd(dev, O_RDONLY, 0);
    }
    else {
        fd = wrapped_fd;
        r  = lseek(fd, 0, SEEK_SET);
        if(r < 0) {
            usbi_err(ctx, "lseek failed, errno=%d", errno);
            return LIBUSB_ERROR_IO;
        }
    }
    if(fd < 0)
        return fd;

    alloc_len = 0;
    do {
        const size_t desc_read_length = 256;
        uint8_t     *read_ptr;

        alloc_len += desc_read_length;
        priv->descriptors = usbi_reallocf(priv->descriptors, alloc_len);
        if(!priv->descriptors) {
            if(fd != wrapped_fd)
                close(fd);
            return LIBUSB_ERROR_NO_MEM;
        }
        read_ptr = (uint8_t *)priv->descriptors + priv->descriptors_len;
        /* usbfs has holes in the file */
        if(!sysfs_dir)
            memset(read_ptr, 0, desc_read_length);
        nb = read(fd, read_ptr, desc_read_length);
        if(nb < 0) {
            usbi_err(ctx, "read descriptor failed, errno=%d", errno);
            if(fd != wrapped_fd)
                close(fd);
            return LIBUSB_ERROR_IO;
        }
        priv->descriptors_len += (size_t)nb;
    } while(priv->descriptors_len == alloc_len);

    if(fd != wrapped_fd)
        close(fd);

    if(priv->descriptors_len < LIBUSB_DT_DEVICE_SIZE) {
        usbi_err(ctx, "short descriptor read (%zu)", priv->descriptors_len);
        return LIBUSB_ERROR_IO;
    }

    r = parse_config_descriptors(dev);
    if(r < 0)
        return r;

    memcpy(&dev->device_descriptor, priv->descriptors, LIBUSB_DT_DEVICE_SIZE);

    if(sysfs_dir) {
        /* sysfs descriptors are in bus-endian format */
        usbi_localize_device_descriptor(&dev->device_descriptor);
        return LIBUSB_SUCCESS;
    }

    /* cache active config */
    if(wrapped_fd < 0)
        fd = get_usbfs_fd(dev, O_RDWR, 1);
    else
        fd = wrapped_fd;
    if(fd < 0) {
        /* cannot send a control message to determine the active
         * config. just assume the first one is active. */
        usbi_warn(ctx, "Missing rw usbfs access; cannot determine "
                       "active configuration descriptor");
        if(priv->config_descriptors)
            priv->active_config = (int)priv->config_descriptors[0].desc->bConfigurationValue;
        else
            priv->active_config = -1; /* No config dt */

        return LIBUSB_SUCCESS;
    }

    r = usbfs_get_active_config(dev, fd);
    if(fd != wrapped_fd)
        close(fd);

    return r;
}

static int linux_get_parent_info(struct libusb_device *dev, const char *sysfs_dir) {
    struct libusb_context *ctx = DEVICE_CTX(dev);
    struct libusb_device  *it;
    char                  *parent_sysfs_dir, *tmp;
    int                    ret, add_parent = 1;

    /* XXX -- can we figure out the topology when using usbfs? */
    if(!sysfs_dir || !strncmp(sysfs_dir, "usb", 3)) {
        /* either using usbfs or finding the parent of a root hub */
        return LIBUSB_SUCCESS;
    }

    parent_sysfs_dir = strdup(sysfs_dir);
    if(!parent_sysfs_dir)
        return LIBUSB_ERROR_NO_MEM;

    if((tmp = strrchr(parent_sysfs_dir, '.')) || (tmp = strrchr(parent_sysfs_dir, '-'))) {
        dev->port_number = atoi(tmp + 1);
        *tmp             = '\0';
    }
    else {
        usbi_warn(ctx, "Can not parse sysfs_dir: %s, no parent info", parent_sysfs_dir);
        free(parent_sysfs_dir);
        return LIBUSB_SUCCESS;
    }

    /* is the parent a root hub? */
    if(!strchr(parent_sysfs_dir, '-')) {
        tmp = parent_sysfs_dir;
        ret = asprintf(&parent_sysfs_dir, "usb%s", tmp);
        free(tmp);
        if(ret < 0)
            return LIBUSB_ERROR_NO_MEM;
    }

retry:
    /* find the parent in the context */
    usbi_mutex_lock(&ctx->usb_devs_lock);
    for_each_device(ctx, it) {
        struct linux_device_priv *priv = usbi_get_device_priv(it);

        if(priv->sysfs_dir) {
            if(!strcmp(priv->sysfs_dir, parent_sysfs_dir)) {
                dev->parent_dev = libusb_ref_device(it);
                break;
            }
        }
    }
    usbi_mutex_unlock(&ctx->usb_devs_lock);

    if(!dev->parent_dev && add_parent) {
        usbi_dbg(ctx, "parent_dev %s not enumerated yet, enumerating now", parent_sysfs_dir);
        sysfs_scan_device(ctx, parent_sysfs_dir);
        add_parent = 0;
        goto retry;
    }

    usbi_dbg(ctx, "dev %p (%s) has parent %p (%s) port %u", (void *)dev, sysfs_dir, (void *)dev->parent_dev, parent_sysfs_dir, dev->port_number);

    free(parent_sysfs_dir);

    return LIBUSB_SUCCESS;
}

int linux_enumerate_device(struct libusb_context *ctx, uint8_t busnum, uint8_t devaddr, const char *sysfs_dir) {
    unsigned long         session_id;
    struct libusb_device *dev;
    int                   r;

    /* FIXME: session ID is not guaranteed unique as addresses can wrap and
     * will be reused. instead we should add a simple sysfs attribute with
     * a session ID. */
    session_id = busnum << 8 | devaddr;
    usbi_dbg(ctx, "busnum %u devaddr %u session_id %lu", busnum, devaddr, session_id);

    dev = usbi_get_device_by_session_id(ctx, session_id);
    if(dev) {
        /* device already exists in the context */
        usbi_dbg(ctx, "session_id %lu already exists", session_id);
        libusb_unref_device(dev);
        return LIBUSB_SUCCESS;
    }

    usbi_dbg(ctx, "allocating new device for %u/%u (session %lu)", busnum, devaddr, session_id);
    dev = usbi_alloc_device(ctx, session_id);
    if(!dev)
        return LIBUSB_ERROR_NO_MEM;

    r = initialize_device(dev, busnum, devaddr, sysfs_dir, -1);
    if(r < 0)
        goto out;
    r = usbi_sanitize_device(dev);
    if(r < 0)
        goto out;

    r = linux_get_parent_info(dev, sysfs_dir);
    if(r < 0)
        goto out;
out:
    if(r < 0)
        libusb_unref_device(dev);
    else
        usbi_connect_device(dev);

    return r;
}

void linux_hotplug_enumerate(uint8_t busnum, uint8_t devaddr, const char *sys_name) {
    struct libusb_context *ctx;

    usbi_mutex_static_lock(&active_contexts_lock);
    for_each_context(ctx) {
        linux_enumerate_device(ctx, busnum, devaddr, sys_name);
    }
    usbi_mutex_static_unlock(&active_contexts_lock);
}

void linux_device_disconnected(uint8_t busnum, uint8_t devaddr) {
    struct libusb_context *ctx;
    struct libusb_device  *dev;
    unsigned long          session_id = busnum << 8 | devaddr;

    usbi_mutex_static_lock(&active_contexts_lock);
    for_each_context(ctx) {
        dev = usbi_get_device_by_session_id(ctx, session_id);
        if(dev) {
            usbi_disconnect_device(dev);
            libusb_unref_device(dev);
        }
        else {
            usbi_dbg(ctx, "device not found for session %lx", session_id);
        }
    }
    usbi_mutex_static_unlock(&active_contexts_lock);
}

#if !defined(HAVE_LIBUDEV)
static int parse_u8(const char *str, uint8_t *val_p) {
    char *endptr;
    long  num;

    errno = 0;
    num   = strtol(str, &endptr, 10);
    if(num < 0 || num > UINT8_MAX || errno)
        return 0;
    if(endptr == str || *endptr != '\0')
        return 0;

    *val_p = (uint8_t)num;
    return 1;
}

/* open a bus directory and adds all discovered devices to the context */
static int usbfs_scan_busdir(struct libusb_context *ctx, uint8_t busnum) {
    DIR           *dir;
    char           dirpath[20];
    struct dirent *entry;
    int            r = LIBUSB_ERROR_IO;

    snprintf(dirpath, sizeof(dirpath), USB_DEVTMPFS_PATH "/%03u", busnum);
    usbi_dbg(ctx, "%s", dirpath);
    dir = opendir(dirpath);
    if(!dir) {
        usbi_err(ctx, "opendir '%s' failed, errno=%d", dirpath, errno);
        /* FIXME: should handle valid race conditions like hub unplugged
         * during directory iteration - this is not an error */
        return r;
    }

    while((entry = readdir(dir))) {
        uint8_t devaddr;

        if(entry->d_name[0] == '.')
            continue;

        if(!parse_u8(entry->d_name, &devaddr)) {
            usbi_dbg(ctx, "unknown dir entry %s", entry->d_name);
            continue;
        }

        if(linux_enumerate_device(ctx, busnum, devaddr, NULL)) {
            usbi_dbg(ctx, "failed to enumerate dir entry %s", entry->d_name);
            continue;
        }

        r = 0;
    }

    closedir(dir);
    return r;
}

static int usbfs_get_device_list(struct libusb_context *ctx) {
    struct dirent *entry;
    DIR           *buses;
    uint8_t        busnum, devaddr;
    int            r = 0;

    if(usbdev_names)
        buses = opendir(USBDEV_PATH);
    else
        buses = opendir(USB_DEVTMPFS_PATH);

    if(!buses) {
        usbi_err(ctx, "opendir buses failed, errno=%d", errno);
        return LIBUSB_ERROR_IO;
    }

    while((entry = readdir(buses))) {
        if(entry->d_name[0] == '.')
            continue;

        if(usbdev_names) {
            if(!is_usbdev_entry(entry->d_name, &busnum, &devaddr))
                continue;

            r = linux_enumerate_device(ctx, busnum, devaddr, NULL);
            if(r < 0) {
                usbi_dbg(ctx, "failed to enumerate dir entry %s", entry->d_name);
                continue;
            }
        }
        else {
            if(!parse_u8(entry->d_name, &busnum)) {
                usbi_dbg(ctx, "unknown dir entry %s", entry->d_name);
                continue;
            }

            r = usbfs_scan_busdir(ctx, busnum);
            if(r < 0)
                break;
        }
    }

    closedir(buses);
    return r;
}

static int sysfs_get_device_list(struct libusb_context *ctx) {
    DIR           *devices = opendir(SYSFS_DEVICE_PATH);
    struct dirent *entry;
    int            num_devices    = 0;
    int            num_enumerated = 0;

    if(!devices) {
        usbi_err(ctx, "opendir devices failed, errno=%d", errno);
        return LIBUSB_ERROR_IO;
    }

    while((entry = readdir(devices))) {
        if((!isdigit(entry->d_name[0]) && strncmp(entry->d_name, "usb", 3)) || strchr(entry->d_name, ':'))
            continue;

        num_devices++;

        if(sysfs_scan_device(ctx, entry->d_name)) {
            usbi_dbg(ctx, "failed to enumerate dir entry %s", entry->d_name);
            continue;
        }

        num_enumerated++;
    }

    closedir(devices);

    /* successful if at least one device was enumerated or no devices were found */
    if(num_enumerated || !num_devices)
        return LIBUSB_SUCCESS;
    else
        return LIBUSB_ERROR_IO;
}

static int linux_default_scan_devices(struct libusb_context *ctx) {
    /* we can retrieve device list and descriptors from sysfs or usbfs.
     * sysfs is preferable, because if we use usbfs we end up resuming
     * any autosuspended USB devices. however, sysfs is not available
     * everywhere, so we need a usbfs fallback too.
     */
    if(sysfs_available)
        return sysfs_get_device_list(ctx);
    else
        return usbfs_get_device_list(ctx);
}
#endif

static int initialize_handle(struct libusb_device_handle *handle, int fd) {
    struct linux_device_handle_priv *hpriv = usbi_get_device_handle_priv(handle);
    int                              r;

    hpriv->fd = fd;

    r = ioctl(fd, IOCTL_USBFS_GET_CAPABILITIES, &hpriv->caps);
    if(r < 0) {
        if(errno == ENOTTY)
            usbi_dbg(HANDLE_CTX(handle), "getcap not available");
        else
            usbi_err(HANDLE_CTX(handle), "getcap failed, errno=%d", errno);
        hpriv->caps = USBFS_CAP_BULK_CONTINUATION;
    }

    return usbi_add_event_source(HANDLE_CTX(handle), hpriv->fd, POLLOUT);
}

static int op_wrap_sys_device(struct libusb_context *ctx, struct libusb_device_handle *handle, intptr_t sys_dev) {
    struct linux_device_handle_priv *hpriv = usbi_get_device_handle_priv(handle);
    int                              fd    = (int)sys_dev;
    uint8_t                          busnum, devaddr;
    struct usbfs_connectinfo         ci;
    struct libusb_device            *dev;
    int                              r;

    r = linux_get_device_address(ctx, 1, &busnum, &devaddr, NULL, NULL, fd);
    if(r < 0) {
        r = ioctl(fd, IOCTL_USBFS_CONNECTINFO, &ci);
        if(r < 0) {
            usbi_err(ctx, "connectinfo failed, errno=%d", errno);
            return LIBUSB_ERROR_IO;
        }
        /* There is no ioctl to get the bus number. We choose 0 here
         * as linux starts numbering buses from 1. */
        busnum  = 0;
        devaddr = ci.devnum;
    }

    /* Session id is unused as we do not add the device to the list of
     * connected devices. */
    usbi_dbg(ctx, "allocating new device for fd %d", fd);
    dev = usbi_alloc_device(ctx, 0);
    if(!dev)
        return LIBUSB_ERROR_NO_MEM;

    r = initialize_device(dev, busnum, devaddr, NULL, fd);
    if(r < 0)
        goto out;
    r = usbi_sanitize_device(dev);
    if(r < 0)
        goto out;
    /* Consider the device as connected, but do not add it to the managed
     * device list. */
    usbi_atomic_store(&dev->attached, 1);
    handle->dev = dev;

    r              = initialize_handle(handle, fd);
    hpriv->fd_keep = 1;

out:
    if(r < 0)
        libusb_unref_device(dev);
    return r;
}

static int op_open(struct libusb_device_handle *handle) {
    int fd, r;

    fd = get_usbfs_fd(handle->dev, O_RDWR, 0);
    if(fd < 0) {
        if(fd == LIBUSB_ERROR_NO_DEVICE) {
            /* device will still be marked as attached if hotplug monitor thread
             * hasn't processed remove event yet */
            usbi_mutex_static_lock(&linux_hotplug_lock);
            if(usbi_atomic_load(&handle->dev->attached)) {
                usbi_dbg(HANDLE_CTX(handle), "open failed with no device, but device still attached");
                linux_device_disconnected(handle->dev->bus_number, handle->dev->device_address);
            }
            usbi_mutex_static_unlock(&linux_hotplug_lock);
        }
        return fd;
    }

    r = initialize_handle(handle, fd);
    if(r < 0)
        close(fd);

    return r;
}

static void op_close(struct libusb_device_handle *dev_handle) {
    struct linux_device_handle_priv *hpriv = usbi_get_device_handle_priv(dev_handle);

    /* fd may have already been removed by POLLERR condition in op_handle_events() */
    if(!hpriv->fd_removed)
        usbi_remove_event_source(HANDLE_CTX(dev_handle), hpriv->fd);
    if(!hpriv->fd_keep)
        close(hpriv->fd);
}

static int op_get_configuration(struct libusb_device_handle *handle, uint8_t *config) {
    struct linux_device_priv *priv          = usbi_get_device_priv(handle->dev);
    int                       active_config = -1; /* to please compiler */
    int                       r;

    if(priv->sysfs_dir) {
        r = sysfs_get_active_config(handle->dev, &active_config);
    }
    else {
        struct linux_device_handle_priv *hpriv = usbi_get_device_handle_priv(handle);

        r = usbfs_get_active_config(handle->dev, hpriv->fd);
        if(r == LIBUSB_SUCCESS)
            active_config = priv->active_config;
    }
    if(r < 0)
        return r;

    if(active_config == -1) {
        usbi_warn(HANDLE_CTX(handle), "device unconfigured");
        active_config = 0;
    }

    *config = (uint8_t)active_config;

    return 0;
}

static int op_set_configuration(struct libusb_device_handle *handle, int config) {
    struct linux_device_priv        *priv  = usbi_get_device_priv(handle->dev);
    struct linux_device_handle_priv *hpriv = usbi_get_device_handle_priv(handle);
    int                              fd    = hpriv->fd;
    int                              r     = ioctl(fd, IOCTL_USBFS_SETCONFIGURATION, &config);

    if(r < 0) {
        if(errno == EINVAL)
            return LIBUSB_ERROR_NOT_FOUND;
        else if(errno == EBUSY)
            return LIBUSB_ERROR_BUSY;
        else if(errno == ENODEV)
            return LIBUSB_ERROR_NO_DEVICE;

        usbi_err(HANDLE_CTX(handle), "set configuration failed, errno=%d", errno);
        return LIBUSB_ERROR_OTHER;
    }

    /* if necessary, update our cached active config descriptor */
    if(!priv->sysfs_dir) {
        if(config == 0 && !dev_has_config0(handle->dev))
            config = -1;

        priv->active_config = config;
    }

    return LIBUSB_SUCCESS;
}

static int claim_interface(struct libusb_device_handle *handle, unsigned int iface) {
    struct linux_device_handle_priv *hpriv = usbi_get_device_handle_priv(handle);
    int                              fd    = hpriv->fd;
    int                              r     = ioctl(fd, IOCTL_USBFS_CLAIMINTERFACE, &iface);

    if(r < 0) {
        if(errno == ENOENT)
            return LIBUSB_ERROR_NOT_FOUND;
        else if(errno == EBUSY)
            return LIBUSB_ERROR_BUSY;
        else if(errno == ENODEV)
            return LIBUSB_ERROR_NO_DEVICE;

        usbi_err(HANDLE_CTX(handle), "claim interface failed, errno=%d", errno);
        return LIBUSB_ERROR_OTHER;
    }
    return 0;
}

static int release_interface(struct libusb_device_handle *handle, unsigned int iface) {
    struct linux_device_handle_priv *hpriv = usbi_get_device_handle_priv(handle);
    int                              fd    = hpriv->fd;
    int                              r     = ioctl(fd, IOCTL_USBFS_RELEASEINTERFACE, &iface);

    if(r < 0) {
        if(errno == ENODEV)
            return LIBUSB_ERROR_NO_DEVICE;

        usbi_err(HANDLE_CTX(handle), "release interface failed, errno=%d", errno);
        return LIBUSB_ERROR_OTHER;
    }
    return 0;
}

static int op_set_interface(struct libusb_device_handle *handle, uint8_t interface, uint8_t altsetting) {
    struct linux_device_handle_priv *hpriv = usbi_get_device_handle_priv(handle);
    int                              fd    = hpriv->fd;
    struct usbfs_setinterface        setintf;
    int                              r;

    setintf.interface  = interface;
    setintf.altsetting = altsetting;
    r                  = ioctl(fd, IOCTL_USBFS_SETINTERFACE, &setintf);
    if(r < 0) {
        if(errno == EINVAL)
            return LIBUSB_ERROR_NOT_FOUND;
        else if(errno == ENODEV)
            return LIBUSB_ERROR_NO_DEVICE;

        usbi_err(HANDLE_CTX(handle), "set interface failed, errno=%d", errno);
        return LIBUSB_ERROR_OTHER;
    }

    return 0;
}

static int op_clear_halt(struct libusb_device_handle *handle, unsigned char endpoint) {
    struct linux_device_handle_priv *hpriv     = usbi_get_device_handle_priv(handle);
    int                              fd        = hpriv->fd;
    unsigned int                     _endpoint = endpoint;
    int                              r         = ioctl(fd, IOCTL_USBFS_CLEAR_HALT, &_endpoint);

    if(r < 0) {
        if(errno == ENOENT)
            return LIBUSB_ERROR_NOT_FOUND;
        else if(errno == ENODEV)
            return LIBUSB_ERROR_NO_DEVICE;

        usbi_err(HANDLE_CTX(handle), "clear halt failed, errno=%d", errno);
        return LIBUSB_ERROR_OTHER;
    }

    return 0;
}

static int op_reset_device(struct libusb_device_handle *handle) {
    struct linux_device_handle_priv *hpriv = usbi_get_device_handle_priv(handle);
    int                              fd    = hpriv->fd;
    int                              r, ret = 0;
    uint8_t                          i;

    /* Doing a device reset will cause the usbfs driver to get unbound
     * from any interfaces it is bound to. By voluntarily unbinding
     * the usbfs driver ourself, we stop the kernel from rebinding
     * the interface after reset (which would end up with the interface
     * getting bound to the in kernel driver if any). */
    for(i = 0; i < USB_MAXINTERFACES; i++) {
        if(handle->claimed_interfaces & (1UL << i))
            release_interface(handle, i);
    }

    usbi_mutex_lock(&handle->lock);
    r = ioctl(fd, IOCTL_USBFS_RESET, NULL);
    if(r < 0) {
        if(errno == ENODEV) {
            ret = LIBUSB_ERROR_NOT_FOUND;
            goto out;
        }

        usbi_err(HANDLE_CTX(handle), "reset failed, errno=%d", errno);
        ret = LIBUSB_ERROR_OTHER;
        goto out;
    }

    /* And re-claim any interfaces which were claimed before the reset */
    for(i = 0; i < USB_MAXINTERFACES; i++) {
        if(!(handle->claimed_interfaces & (1UL << i)))
            continue;
        /*
         * A driver may have completed modprobing during
         * IOCTL_USBFS_RESET, and bound itself as soon as
         * IOCTL_USBFS_RESET released the device lock
         */
        r = detach_kernel_driver_and_claim(handle, i);
        if(r) {
            usbi_warn(HANDLE_CTX(handle), "failed to re-claim interface %u after reset: %s", i, libusb_error_name(r));
            handle->claimed_interfaces &= ~(1UL << i);
            ret = LIBUSB_ERROR_NOT_FOUND;
        }
    }
out:
    usbi_mutex_unlock(&handle->lock);
    return ret;
}

static int do_streams_ioctl(struct libusb_device_handle *handle, long req, uint32_t num_streams, unsigned char *endpoints, int num_endpoints) {
    struct linux_device_handle_priv *hpriv = usbi_get_device_handle_priv(handle);
    int                              r, fd = hpriv->fd;
    struct usbfs_streams            *streams;

    if(num_endpoints > 30) /* Max 15 in + 15 out eps */
        return LIBUSB_ERROR_INVALID_PARAM;

    streams = malloc(sizeof(*streams) + num_endpoints);
    if(!streams)
        return LIBUSB_ERROR_NO_MEM;

    streams->num_streams = num_streams;
    streams->num_eps     = num_endpoints;
    memcpy(streams->eps, endpoints, num_endpoints);

    r = ioctl(fd, req, streams);

    free(streams);

    if(r < 0) {
        if(errno == ENOTTY)
            return LIBUSB_ERROR_NOT_SUPPORTED;
        else if(errno == EINVAL)
            return LIBUSB_ERROR_INVALID_PARAM;
        else if(errno == ENODEV)
            return LIBUSB_ERROR_NO_DEVICE;

        usbi_err(HANDLE_CTX(handle), "streams-ioctl failed, errno=%d", errno);
        return LIBUSB_ERROR_OTHER;
    }
    return r;
}

static int op_alloc_streams(struct libusb_device_handle *handle, uint32_t num_streams, unsigned char *endpoints, int num_endpoints) {
    return do_streams_ioctl(handle, IOCTL_USBFS_ALLOC_STREAMS, num_streams, endpoints, num_endpoints);
}

static int op_free_streams(struct libusb_device_handle *handle, unsigned char *endpoints, int num_endpoints) {
    return do_streams_ioctl(handle, IOCTL_USBFS_FREE_STREAMS, 0, endpoints, num_endpoints);
}

static void *op_dev_mem_alloc(struct libusb_device_handle *handle, size_t len) {
    struct linux_device_handle_priv *hpriv = usbi_get_device_handle_priv(handle);
    void                            *buffer;

    buffer = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, hpriv->fd, 0);
    if(buffer == MAP_FAILED) {
        usbi_err(HANDLE_CTX(handle), "alloc dev mem failed, errno=%d", errno);
        return NULL;
    }
    return buffer;
}

static int op_dev_mem_free(struct libusb_device_handle *handle, void *buffer, size_t len) {
    if(munmap(buffer, len) != 0) {
        usbi_err(HANDLE_CTX(handle), "free dev mem failed, errno=%d", errno);
        return LIBUSB_ERROR_OTHER;
    }
    else {
        return LIBUSB_SUCCESS;
    }
}

static int op_kernel_driver_active(struct libusb_device_handle *handle, uint8_t interface) {
    struct linux_device_handle_priv *hpriv = usbi_get_device_handle_priv(handle);
    int                              fd    = hpriv->fd;
    struct usbfs_getdriver           getdrv;
    int                              r;

    getdrv.interface = interface;
    r                = ioctl(fd, IOCTL_USBFS_GETDRIVER, &getdrv);
    if(r < 0) {
        if(errno == ENODATA)
            return 0;
        else if(errno == ENODEV)
            return LIBUSB_ERROR_NO_DEVICE;

        usbi_err(HANDLE_CTX(handle), "get driver failed, errno=%d", errno);
        return LIBUSB_ERROR_OTHER;
    }

    return strcmp(getdrv.driver, "usbfs") != 0;
}

static int op_detach_kernel_driver(struct libusb_device_handle *handle, uint8_t interface) {
    struct linux_device_handle_priv *hpriv = usbi_get_device_handle_priv(handle);
    int                              fd    = hpriv->fd;
    struct usbfs_ioctl               command;
    struct usbfs_getdriver           getdrv;
    int                              r;

    command.ifno       = interface;
    command.ioctl_code = IOCTL_USBFS_DISCONNECT;
    command.data       = NULL;

    getdrv.interface = interface;
    r                = ioctl(fd, IOCTL_USBFS_GETDRIVER, &getdrv);
    if(r == 0 && !strcmp(getdrv.driver, "usbfs"))
        return LIBUSB_ERROR_NOT_FOUND;

    r = ioctl(fd, IOCTL_USBFS_IOCTL, &command);
    if(r < 0) {
        if(errno == ENODATA)
            return LIBUSB_ERROR_NOT_FOUND;
        else if(errno == EINVAL)
            return LIBUSB_ERROR_INVALID_PARAM;
        else if(errno == ENODEV)
            return LIBUSB_ERROR_NO_DEVICE;

        usbi_err(HANDLE_CTX(handle), "detach failed, errno=%d", errno);
        return LIBUSB_ERROR_OTHER;
    }

    return 0;
}

static int op_attach_kernel_driver(struct libusb_device_handle *handle, uint8_t interface) {
    struct linux_device_handle_priv *hpriv = usbi_get_device_handle_priv(handle);
    int                              fd    = hpriv->fd;
    struct usbfs_ioctl               command;
    int                              r;

    command.ifno       = interface;
    command.ioctl_code = IOCTL_USBFS_CONNECT;
    command.data       = NULL;

    r = ioctl(fd, IOCTL_USBFS_IOCTL, &command);
    if(r < 0) {
        if(errno == ENODATA)
            return LIBUSB_ERROR_NOT_FOUND;
        else if(errno == EINVAL)
            return LIBUSB_ERROR_INVALID_PARAM;
        else if(errno == ENODEV)
            return LIBUSB_ERROR_NO_DEVICE;
        else if(errno == EBUSY)
            return LIBUSB_ERROR_BUSY;

        usbi_err(HANDLE_CTX(handle), "attach failed, errno=%d", errno);
        return LIBUSB_ERROR_OTHER;
    }
    else if(r == 0) {
        return LIBUSB_ERROR_NOT_FOUND;
    }

    return 0;
}

static int detach_kernel_driver_and_claim(struct libusb_device_handle *handle, uint8_t interface) {
    struct linux_device_handle_priv *hpriv = usbi_get_device_handle_priv(handle);
    struct usbfs_disconnect_claim    dc;
    int                              r, fd = hpriv->fd;

    dc.interface = interface;
    strcpy(dc.driver, "usbfs");
    dc.flags = USBFS_DISCONNECT_CLAIM_EXCEPT_DRIVER;
    r        = ioctl(fd, IOCTL_USBFS_DISCONNECT_CLAIM, &dc);
    if(r == 0)
        return 0;
    switch(errno) {
    case ENOTTY:
        break;
    case EBUSY:
        return LIBUSB_ERROR_BUSY;
    case EINVAL:
        return LIBUSB_ERROR_INVALID_PARAM;
    case ENODEV:
        return LIBUSB_ERROR_NO_DEVICE;
    default:
        usbi_err(HANDLE_CTX(handle), "disconnect-and-claim failed, errno=%d", errno);
        return LIBUSB_ERROR_OTHER;
    }

    /* Fallback code for kernels which don't support the
       disconnect-and-claim ioctl */
    r = op_detach_kernel_driver(handle, interface);
    if(r != 0 && r != LIBUSB_ERROR_NOT_FOUND)
        return r;

    return claim_interface(handle, interface);
}

static int op_claim_interface(struct libusb_device_handle *handle, uint8_t interface) {
    if(handle->auto_detach_kernel_driver)
        return detach_kernel_driver_and_claim(handle, interface);
    else
        return claim_interface(handle, interface);
}

static int op_release_interface(struct libusb_device_handle *handle, uint8_t interface) {
    int r;

    r = release_interface(handle, interface);
    if(r)
        return r;

    if(handle->auto_detach_kernel_driver)
        op_attach_kernel_driver(handle, interface);

    return 0;
}

static void op_destroy_device(struct libusb_device *dev) {
    struct linux_device_priv *priv = usbi_get_device_priv(dev);

    free(priv->config_descriptors);
    free(priv->descriptors);
    free(priv->sysfs_dir);
}

/* URBs are discarded in reverse order of submission to avoid races. */
static int discard_urbs(struct usbi_transfer *itransfer, int first, int last_plus_one) {
    struct libusb_transfer          *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    struct linux_transfer_priv      *tpriv    = usbi_get_transfer_priv(itransfer);
    struct linux_device_handle_priv *hpriv    = usbi_get_device_handle_priv(transfer->dev_handle);
    int                              i, ret = 0;
    struct usbfs_urb                *urb;

    for(i = last_plus_one - 1; i >= first; i--) {
        if(transfer->type == LIBUSB_TRANSFER_TYPE_ISOCHRONOUS)
            urb = tpriv->iso_urbs[i];
        else
            urb = &tpriv->urbs[i];

        if(ioctl(hpriv->fd, IOCTL_USBFS_DISCARDURB, urb) == 0)
            continue;

        if(errno == EINVAL) {
            usbi_dbg(TRANSFER_CTX(transfer), "URB not found --> assuming ready to be reaped");
            if(i == (last_plus_one - 1))
                ret = LIBUSB_ERROR_NOT_FOUND;
        }
        else if(errno == ENODEV) {
            usbi_dbg(TRANSFER_CTX(transfer), "Device not found for URB --> assuming ready to be reaped");
            ret = LIBUSB_ERROR_NO_DEVICE;
        }
        else {
            usbi_warn(TRANSFER_CTX(transfer), "unrecognised discard errno %d", errno);
            ret = LIBUSB_ERROR_OTHER;
        }
    }
    return ret;
}

static void free_iso_urbs(struct linux_transfer_priv *tpriv) {
    int i;

    for(i = 0; i < tpriv->num_urbs; i++) {
        struct usbfs_urb *urb = tpriv->iso_urbs[i];

        if(!urb)
            break;
        free(urb);
    }

    free(tpriv->iso_urbs);
    tpriv->iso_urbs = NULL;
}

static int submit_bulk_transfer(struct usbi_transfer *itransfer) {
    struct libusb_transfer          *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    struct linux_transfer_priv      *tpriv    = usbi_get_transfer_priv(itransfer);
    struct linux_device_handle_priv *hpriv    = usbi_get_device_handle_priv(transfer->dev_handle);
    struct usbfs_urb                *urbs;
    int                              is_out = IS_XFEROUT(transfer);
    int                              bulk_buffer_len, use_bulk_continuation;
    int                              num_urbs;
    int                              last_urb_partial = 0;
    int                              r;
    int                              i;

    /*
     * Older versions of usbfs place a 16kb limit on bulk URBs. We work
     * around this by splitting large transfers into 16k blocks, and then
     * submit all urbs at once. it would be simpler to submit one urb at
     * a time, but there is a big performance gain doing it this way.
     *
     * Newer versions lift the 16k limit (USBFS_CAP_NO_PACKET_SIZE_LIM),
     * using arbitrary large transfers can still be a bad idea though, as
     * the kernel needs to allocate physical contiguous memory for this,
     * which may fail for large buffers.
     *
     * The kernel solves this problem by splitting the transfer into
     * blocks itself when the host-controller is scatter-gather capable
     * (USBFS_CAP_BULK_SCATTER_GATHER), which most controllers are.
     *
     * Last, there is the issue of short-transfers when splitting, for
     * short split-transfers to work reliable USBFS_CAP_BULK_CONTINUATION
     * is needed, but this is not always available.
     */
    if(hpriv->caps & USBFS_CAP_BULK_SCATTER_GATHER) {
        /* Good! Just submit everything in one go */
        bulk_buffer_len       = transfer->length ? transfer->length : 1;
        use_bulk_continuation = 0;
    }
    else if(hpriv->caps & USBFS_CAP_BULK_CONTINUATION) {
        /* Split the transfers and use bulk-continuation to
           avoid issues with short-transfers */
        bulk_buffer_len       = MAX_BULK_BUFFER_LENGTH;
        use_bulk_continuation = 1;
    }
    else if(hpriv->caps & USBFS_CAP_NO_PACKET_SIZE_LIM) {
        /* Don't split, assume the kernel can alloc the buffer
           (otherwise the submit will fail with -ENOMEM) */
        bulk_buffer_len       = transfer->length ? transfer->length : 1;
        use_bulk_continuation = 0;
    }
    else {
        /* Bad, splitting without bulk-continuation, short transfers
           which end before the last urb will not work reliable! */
        /* Note we don't warn here as this is "normal" on kernels <
           2.6.32 and not a problem for most applications */
        bulk_buffer_len       = MAX_BULK_BUFFER_LENGTH;
        use_bulk_continuation = 0;
    }

    num_urbs = transfer->length / bulk_buffer_len;

    if(transfer->length == 0) {
        num_urbs = 1;
    }
    else if((transfer->length % bulk_buffer_len) > 0) {
        last_urb_partial = 1;
        num_urbs++;
    }
    usbi_dbg(TRANSFER_CTX(transfer), "need %d urbs for new transfer with length %d", num_urbs, transfer->length);
    urbs = calloc(num_urbs, sizeof(*urbs));
    if(!urbs)
        return LIBUSB_ERROR_NO_MEM;
    tpriv->urbs        = urbs;
    tpriv->num_urbs    = num_urbs;
    tpriv->num_retired = 0;
    tpriv->reap_action = NORMAL;
    tpriv->reap_status = LIBUSB_TRANSFER_COMPLETED;

    for(i = 0; i < num_urbs; i++) {
        struct usbfs_urb *urb = &urbs[i];

        urb->usercontext = itransfer;
        switch(transfer->type) {
        case LIBUSB_TRANSFER_TYPE_BULK:
            urb->type      = USBFS_URB_TYPE_BULK;
            urb->stream_id = 0;
            break;
        case LIBUSB_TRANSFER_TYPE_BULK_STREAM:
            urb->type      = USBFS_URB_TYPE_BULK;
            urb->stream_id = itransfer->stream_id;
            break;
        case LIBUSB_TRANSFER_TYPE_INTERRUPT:
            urb->type = USBFS_URB_TYPE_INTERRUPT;
            break;
        }
        urb->endpoint = transfer->endpoint;
        urb->buffer   = transfer->buffer + (i * bulk_buffer_len);

        /* don't set the short not ok flag for the last URB */
        if(use_bulk_continuation && !is_out && (i < num_urbs - 1))
            urb->flags = USBFS_URB_SHORT_NOT_OK;

        if(i == num_urbs - 1 && last_urb_partial)
            urb->buffer_length = transfer->length % bulk_buffer_len;
        else if(transfer->length == 0)
            urb->buffer_length = 0;
        else
            urb->buffer_length = bulk_buffer_len;

        if(i > 0 && use_bulk_continuation)
            urb->flags |= USBFS_URB_BULK_CONTINUATION;

        /* we have already checked that the flag is supported */
        if(is_out && i == num_urbs - 1 && (transfer->flags & LIBUSB_TRANSFER_ADD_ZERO_PACKET))
            urb->flags |= USBFS_URB_ZERO_PACKET;

        r = ioctl(hpriv->fd, IOCTL_USBFS_SUBMITURB, urb);
        if(r == 0)
            continue;

        if(errno == ENODEV) {
            r = LIBUSB_ERROR_NO_DEVICE;
        }
        else if(errno == ENOMEM) {
            r = LIBUSB_ERROR_NO_MEM;
        }
        else {
            usbi_err(TRANSFER_CTX(transfer), "submiturb failed, errno=%d", errno);
            r = LIBUSB_ERROR_IO;
        }

        /* if the first URB submission fails, we can simply free up and
         * return failure immediately. */
        if(i == 0) {
            usbi_dbg(TRANSFER_CTX(transfer), "first URB failed, easy peasy");
            free(urbs);
            tpriv->urbs = NULL;
            return r;
        }

        /* if it's not the first URB that failed, the situation is a bit
         * tricky. we may need to discard all previous URBs. there are
         * complications:
         *  - discarding is asynchronous - discarded urbs will be reaped
         *    later. the user must not have freed the transfer when the
         *    discarded URBs are reaped, otherwise libusb will be using
         *    freed memory.
         *  - the earlier URBs may have completed successfully and we do
         *    not want to throw away any data.
         *  - this URB failing may be no error; EREMOTEIO means that
         *    this transfer simply didn't need all the URBs we submitted
         * so, we report that the transfer was submitted successfully and
         * in case of error we discard all previous URBs. later when
         * the final reap completes we can report error to the user,
         * or success if an earlier URB was completed successfully.
         */
        tpriv->reap_action = errno == EREMOTEIO ? COMPLETED_EARLY : SUBMIT_FAILED;

        /* The URBs we haven't submitted yet we count as already
         * retired. */
        tpriv->num_retired += num_urbs - i;

        /* If we completed short then don't try to discard. */
        if(tpriv->reap_action == COMPLETED_EARLY)
            return 0;

        discard_urbs(itransfer, 0, i);

        usbi_dbg(TRANSFER_CTX(transfer),
                 "reporting successful submission but waiting for %d "
                 "discards before reporting error",
                 i);
        return 0;
    }

    return 0;
}

static int submit_iso_transfer(struct usbi_transfer *itransfer) {
    struct libusb_transfer          *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    struct linux_transfer_priv      *tpriv    = usbi_get_transfer_priv(itransfer);
    struct linux_device_handle_priv *hpriv    = usbi_get_device_handle_priv(transfer->dev_handle);
    struct usbfs_urb               **urbs;
    int                              num_packets = transfer->num_iso_packets;
    int                              num_packets_remaining;
    int                              i, j;
    int                              num_urbs;
    unsigned int                     packet_len;
    unsigned int                     total_len  = 0;
    unsigned char                   *urb_buffer = transfer->buffer;

    if(num_packets < 1)
        return LIBUSB_ERROR_INVALID_PARAM;

    /* usbfs places arbitrary limits on iso URBs. this limit has changed
     * at least three times, but we attempt to detect this limit during
     * init and check it here. if the kernel rejects the request due to
     * its size, we return an error indicating such to the user.
     */
    for(i = 0; i < num_packets; i++) {
        packet_len = transfer->iso_packet_desc[i].length;

        if(packet_len > max_iso_packet_len) {
            usbi_warn(TRANSFER_CTX(transfer), "iso packet length of %u bytes exceeds maximum of %u bytes", packet_len, max_iso_packet_len);
            return LIBUSB_ERROR_INVALID_PARAM;
        }

        total_len += packet_len;
    }

    if(transfer->length < (int)total_len)
        return LIBUSB_ERROR_INVALID_PARAM;

    /* usbfs limits the number of iso packets per URB */
    num_urbs = (num_packets + (MAX_ISO_PACKETS_PER_URB - 1)) / MAX_ISO_PACKETS_PER_URB;

    usbi_dbg(TRANSFER_CTX(transfer), "need %d urbs for new transfer with length %d", num_urbs, transfer->length);

    urbs = calloc(num_urbs, sizeof(*urbs));
    if(!urbs)
        return LIBUSB_ERROR_NO_MEM;

    tpriv->iso_urbs          = urbs;
    tpriv->num_urbs          = num_urbs;
    tpriv->num_retired       = 0;
    tpriv->reap_action       = NORMAL;
    tpriv->iso_packet_offset = 0;

    /* allocate + initialize each URB with the correct number of packets */
    num_packets_remaining = num_packets;
    for(i = 0, j = 0; i < num_urbs; i++) {
        int               num_packets_in_urb = MIN(num_packets_remaining, MAX_ISO_PACKETS_PER_URB);
        struct usbfs_urb *urb;
        size_t            alloc_size;
        int               k;

        alloc_size = sizeof(*urb) + (num_packets_in_urb * sizeof(struct usbfs_iso_packet_desc));
        urb        = calloc(1, alloc_size);
        if(!urb) {
            free_iso_urbs(tpriv);
            return LIBUSB_ERROR_NO_MEM;
        }
        urbs[i] = urb;

        /* populate packet lengths */
        for(k = 0; k < num_packets_in_urb; j++, k++) {
            packet_len = transfer->iso_packet_desc[j].length;
            urb->buffer_length += packet_len;
            urb->iso_frame_desc[k].length = packet_len;
        }

        urb->usercontext = itransfer;
        urb->type        = USBFS_URB_TYPE_ISO;
        /* FIXME: interface for non-ASAP data? */
        urb->flags             = USBFS_URB_ISO_ASAP;
        urb->endpoint          = transfer->endpoint;
        urb->number_of_packets = num_packets_in_urb;
        urb->buffer            = urb_buffer;

        urb_buffer += urb->buffer_length;
        num_packets_remaining -= num_packets_in_urb;
    }

    /* submit URBs */
    for(i = 0; i < num_urbs; i++) {
        int r = ioctl(hpriv->fd, IOCTL_USBFS_SUBMITURB, urbs[i]);

        if(r == 0)
            continue;

        if(errno == ENODEV) {
            r = LIBUSB_ERROR_NO_DEVICE;
        }
        else if(errno == EINVAL) {
            usbi_warn(TRANSFER_CTX(transfer), "submiturb failed, transfer too large");
            r = LIBUSB_ERROR_INVALID_PARAM;
        }
        else if(errno == EMSGSIZE) {
            usbi_warn(TRANSFER_CTX(transfer), "submiturb failed, iso packet length too large");
            r = LIBUSB_ERROR_INVALID_PARAM;
        }
        else {
            usbi_err(TRANSFER_CTX(transfer), "submiturb failed, errno=%d", errno);
            r = LIBUSB_ERROR_IO;
        }

        /* if the first URB submission fails, we can simply free up and
         * return failure immediately. */
        if(i == 0) {
            usbi_dbg(TRANSFER_CTX(transfer), "first URB failed, easy peasy");
            free_iso_urbs(tpriv);
            return r;
        }

        /* if it's not the first URB that failed, the situation is a bit
         * tricky. we must discard all previous URBs. there are
         * complications:
         *  - discarding is asynchronous - discarded urbs will be reaped
         *    later. the user must not have freed the transfer when the
         *    discarded URBs are reaped, otherwise libusb will be using
         *    freed memory.
         *  - the earlier URBs may have completed successfully and we do
         *    not want to throw away any data.
         * so, in this case we discard all the previous URBs BUT we report
         * that the transfer was submitted successfully. then later when
         * the final discard completes we can report error to the user.
         */
        tpriv->reap_action = SUBMIT_FAILED;

        /* The URBs we haven't submitted yet we count as already
         * retired. */
        tpriv->num_retired = num_urbs - i;
        discard_urbs(itransfer, 0, i);

        usbi_dbg(TRANSFER_CTX(transfer),
                 "reporting successful submission but waiting for %d "
                 "discards before reporting error",
                 i);
        return 0;
    }

    return 0;
}

static int submit_control_transfer(struct usbi_transfer *itransfer) {
    struct linux_transfer_priv      *tpriv    = usbi_get_transfer_priv(itransfer);
    struct libusb_transfer          *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    struct linux_device_handle_priv *hpriv    = usbi_get_device_handle_priv(transfer->dev_handle);
    struct usbfs_urb                *urb;
    int                              r;

    if(transfer->length - LIBUSB_CONTROL_SETUP_SIZE > MAX_CTRL_BUFFER_LENGTH)
        return LIBUSB_ERROR_INVALID_PARAM;

    urb = calloc(1, sizeof(*urb));
    if(!urb)
        return LIBUSB_ERROR_NO_MEM;
    tpriv->urbs        = urb;
    tpriv->num_urbs    = 1;
    tpriv->reap_action = NORMAL;

    urb->usercontext   = itransfer;
    urb->type          = USBFS_URB_TYPE_CONTROL;
    urb->endpoint      = transfer->endpoint;
    urb->buffer        = transfer->buffer;
    urb->buffer_length = transfer->length;

    r = ioctl(hpriv->fd, IOCTL_USBFS_SUBMITURB, urb);
    if(r < 0) {
        free(urb);
        tpriv->urbs = NULL;
        if(errno == ENODEV)
            return LIBUSB_ERROR_NO_DEVICE;

        usbi_err(TRANSFER_CTX(transfer), "submiturb failed, errno=%d", errno);
        return LIBUSB_ERROR_IO;
    }
    return 0;
}

static int op_submit_transfer(struct usbi_transfer *itransfer) {
    struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);

    switch(transfer->type) {
    case LIBUSB_TRANSFER_TYPE_CONTROL:
        return submit_control_transfer(itransfer);
    case LIBUSB_TRANSFER_TYPE_BULK:
    case LIBUSB_TRANSFER_TYPE_BULK_STREAM:
        return submit_bulk_transfer(itransfer);
    case LIBUSB_TRANSFER_TYPE_INTERRUPT:
        return submit_bulk_transfer(itransfer);
    case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
        return submit_iso_transfer(itransfer);
    default:
        usbi_err(TRANSFER_CTX(transfer), "unknown transfer type %u", transfer->type);
        return LIBUSB_ERROR_INVALID_PARAM;
    }
}

static int op_cancel_transfer(struct usbi_transfer *itransfer) {
    struct linux_transfer_priv *tpriv    = usbi_get_transfer_priv(itransfer);
    struct libusb_transfer     *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    int                         r;

    if(!tpriv->urbs)
        return LIBUSB_ERROR_NOT_FOUND;

    r = discard_urbs(itransfer, 0, tpriv->num_urbs);
    if(r != 0)
        return r;

    switch(transfer->type) {
    case LIBUSB_TRANSFER_TYPE_BULK:
    case LIBUSB_TRANSFER_TYPE_BULK_STREAM:
        if(tpriv->reap_action == ERROR)
            break;
        /* else, fall through */
    default:
        tpriv->reap_action = CANCELLED;
    }

    return 0;
}

static void op_clear_transfer_priv(struct usbi_transfer *itransfer) {
    struct libusb_transfer     *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    struct linux_transfer_priv *tpriv    = usbi_get_transfer_priv(itransfer);

    switch(transfer->type) {
    case LIBUSB_TRANSFER_TYPE_CONTROL:
    case LIBUSB_TRANSFER_TYPE_BULK:
    case LIBUSB_TRANSFER_TYPE_BULK_STREAM:
    case LIBUSB_TRANSFER_TYPE_INTERRUPT:
        if(tpriv->urbs) {
            free(tpriv->urbs);
            tpriv->urbs = NULL;
        }
        break;
    case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
        if(tpriv->iso_urbs) {
            free_iso_urbs(tpriv);
            tpriv->iso_urbs = NULL;
        }
        break;
    default:
        usbi_err(TRANSFER_CTX(transfer), "unknown transfer type %u", transfer->type);
    }
}

static int handle_bulk_completion(struct usbi_transfer *itransfer, struct usbfs_urb *urb) {
    struct linux_transfer_priv *tpriv    = usbi_get_transfer_priv(itransfer);
    struct libusb_transfer     *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    int                         urb_idx  = urb - tpriv->urbs;

    usbi_mutex_lock(&itransfer->lock);
    usbi_dbg(TRANSFER_CTX(transfer), "handling completion status %d of bulk urb %d/%d", urb->status, urb_idx + 1, tpriv->num_urbs);

    tpriv->num_retired++;

    if(tpriv->reap_action != NORMAL) {
        /* cancelled, submit_fail, or completed early */
        usbi_dbg(TRANSFER_CTX(transfer), "abnormal reap: urb status %d", urb->status);

        /* even though we're in the process of cancelling, it's possible that
         * we may receive some data in these URBs that we don't want to lose.
         * examples:
         * 1. while the kernel is cancelling all the packets that make up an
         *    URB, a few of them might complete. so we get back a successful
         *    cancellation *and* some data.
         * 2. we receive a short URB which marks the early completion condition,
         *    so we start cancelling the remaining URBs. however, we're too
         *    slow and another URB completes (or at least completes partially).
         *    (this can't happen since we always use BULK_CONTINUATION.)
         *
         * When this happens, our objectives are not to lose any "surplus" data,
         * and also to stick it at the end of the previously-received data
         * (closing any holes), so that libusb reports the total amount of
         * transferred data and presents it in a contiguous chunk.
         */
        if(urb->actual_length > 0) {
            unsigned char *target = transfer->buffer + itransfer->transferred;

            usbi_dbg(TRANSFER_CTX(transfer), "received %d bytes of surplus data", urb->actual_length);
            if(urb->buffer != target) {
                usbi_dbg(TRANSFER_CTX(transfer), "moving surplus data from offset %zu to offset %zu", (unsigned char *)urb->buffer - transfer->buffer,
                         target - transfer->buffer);
                memmove(target, urb->buffer, urb->actual_length);
            }
            itransfer->transferred += urb->actual_length;
        }

        if(tpriv->num_retired == tpriv->num_urbs) {
            usbi_dbg(TRANSFER_CTX(transfer), "abnormal reap: last URB handled, reporting");
            if(tpriv->reap_action != COMPLETED_EARLY && tpriv->reap_status == LIBUSB_TRANSFER_COMPLETED)
                tpriv->reap_status = LIBUSB_TRANSFER_ERROR;
            goto completed;
        }
        goto out_unlock;
    }

    itransfer->transferred += urb->actual_length;

    /* Many of these errors can occur on *any* urb of a multi-urb
     * transfer.  When they do, we tear down the rest of the transfer.
     */
    switch(urb->status) {
    case 0:
        break;
    case -EREMOTEIO: /* short transfer */
        break;
    case -ENOENT: /* cancelled */
    case -ECONNRESET:
        break;
    case -ENODEV:
    case -ESHUTDOWN:
        usbi_dbg(TRANSFER_CTX(transfer), "device removed");
        tpriv->reap_status = LIBUSB_TRANSFER_NO_DEVICE;
        goto cancel_remaining;
    case -EPIPE:
        usbi_dbg(TRANSFER_CTX(transfer), "detected endpoint stall");
        if(tpriv->reap_status == LIBUSB_TRANSFER_COMPLETED)
            tpriv->reap_status = LIBUSB_TRANSFER_STALL;
        goto cancel_remaining;
    case -EOVERFLOW:
        /* overflow can only ever occur in the last urb */
        usbi_dbg(TRANSFER_CTX(transfer), "overflow, actual_length=%d", urb->actual_length);
        if(tpriv->reap_status == LIBUSB_TRANSFER_COMPLETED)
            tpriv->reap_status = LIBUSB_TRANSFER_OVERFLOW;
        goto completed;
    case -ETIME:
    case -EPROTO:
    case -EILSEQ:
    case -ECOMM:
    case -ENOSR:
        usbi_dbg(TRANSFER_CTX(transfer), "low-level bus error %d", urb->status);
        tpriv->reap_action = ERROR;
        goto cancel_remaining;
    default:
        usbi_warn(ITRANSFER_CTX(itransfer), "unrecognised urb status %d", urb->status);
        tpriv->reap_action = ERROR;
        goto cancel_remaining;
    }

    /* if we've reaped all urbs or we got less data than requested then we're
     * done */
    if(tpriv->num_retired == tpriv->num_urbs) {
        usbi_dbg(TRANSFER_CTX(transfer), "all URBs in transfer reaped --> complete!");
        goto completed;
    }
    else if(urb->actual_length < urb->buffer_length) {
        usbi_dbg(TRANSFER_CTX(transfer), "short transfer %d/%d --> complete!", urb->actual_length, urb->buffer_length);
        if(tpriv->reap_action == NORMAL)
            tpriv->reap_action = COMPLETED_EARLY;
    }
    else {
        goto out_unlock;
    }

cancel_remaining:
    if(tpriv->reap_action == ERROR && tpriv->reap_status == LIBUSB_TRANSFER_COMPLETED)
        tpriv->reap_status = LIBUSB_TRANSFER_ERROR;

    if(tpriv->num_retired == tpriv->num_urbs) /* nothing to cancel */
        goto completed;

    /* cancel remaining urbs and wait for their completion before
     * reporting results */
    discard_urbs(itransfer, urb_idx + 1, tpriv->num_urbs);

out_unlock:
    usbi_mutex_unlock(&itransfer->lock);
    return 0;

completed:
    free(tpriv->urbs);
    tpriv->urbs = NULL;
    usbi_mutex_unlock(&itransfer->lock);
    return tpriv->reap_action == CANCELLED ? usbi_handle_transfer_cancellation(itransfer) : usbi_handle_transfer_completion(itransfer, tpriv->reap_status);
}

static int handle_iso_completion(struct usbi_transfer *itransfer, struct usbfs_urb *urb) {
    struct libusb_transfer     *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    struct linux_transfer_priv *tpriv    = usbi_get_transfer_priv(itransfer);
    int                         num_urbs = tpriv->num_urbs;
    int                         urb_idx  = 0;
    int                         i;
    enum libusb_transfer_status status = LIBUSB_TRANSFER_COMPLETED;

    usbi_mutex_lock(&itransfer->lock);
    for(i = 0; i < num_urbs; i++) {
        if(urb == tpriv->iso_urbs[i]) {
            urb_idx = i + 1;
            break;
        }
    }
    if(urb_idx == 0) {
        usbi_err(TRANSFER_CTX(transfer), "could not locate urb!");
        usbi_mutex_unlock(&itransfer->lock);
        return LIBUSB_ERROR_NOT_FOUND;
    }

    usbi_dbg(TRANSFER_CTX(transfer), "handling completion status %d of iso urb %d/%d", urb->status, urb_idx, num_urbs);

    /* copy isochronous results back in */

    for(i = 0; i < urb->number_of_packets; i++) {
        struct usbfs_iso_packet_desc        *urb_desc = &urb->iso_frame_desc[i];
        struct libusb_iso_packet_descriptor *lib_desc = &transfer->iso_packet_desc[tpriv->iso_packet_offset++];

        lib_desc->status = LIBUSB_TRANSFER_COMPLETED;
        switch(urb_desc->status) {
        case 0:
            break;
        case -ENOENT: /* cancelled */
        case -ECONNRESET:
            break;
        case -ENODEV:
        case -ESHUTDOWN:
            usbi_dbg(TRANSFER_CTX(transfer), "packet %d - device removed", i);
            lib_desc->status = LIBUSB_TRANSFER_NO_DEVICE;
            break;
        case -EPIPE:
            usbi_dbg(TRANSFER_CTX(transfer), "packet %d - detected endpoint stall", i);
            lib_desc->status = LIBUSB_TRANSFER_STALL;
            break;
        case -EOVERFLOW:
            usbi_dbg(TRANSFER_CTX(transfer), "packet %d - overflow error", i);
            lib_desc->status = LIBUSB_TRANSFER_OVERFLOW;
            break;
        case -ETIME:
        case -EPROTO:
        case -EILSEQ:
        case -ECOMM:
        case -ENOSR:
        case -EXDEV:
            usbi_dbg(TRANSFER_CTX(transfer), "packet %d - low-level USB error %d", i, urb_desc->status);
            lib_desc->status = LIBUSB_TRANSFER_ERROR;
            break;
        default:
            usbi_warn(TRANSFER_CTX(transfer), "packet %d - unrecognised urb status %d", i, urb_desc->status);
            lib_desc->status = LIBUSB_TRANSFER_ERROR;
            break;
        }
        lib_desc->actual_length = urb_desc->actual_length;
    }

    tpriv->num_retired++;

    if(tpriv->reap_action != NORMAL) { /* cancelled or submit_fail */
        usbi_dbg(TRANSFER_CTX(transfer), "CANCEL: urb status %d", urb->status);

        if(tpriv->num_retired == num_urbs) {
            usbi_dbg(TRANSFER_CTX(transfer), "CANCEL: last URB handled, reporting");
            free_iso_urbs(tpriv);
            if(tpriv->reap_action == CANCELLED) {
                usbi_mutex_unlock(&itransfer->lock);
                return usbi_handle_transfer_cancellation(itransfer);
            }
            else {
                usbi_mutex_unlock(&itransfer->lock);
                return usbi_handle_transfer_completion(itransfer, LIBUSB_TRANSFER_ERROR);
            }
        }
        goto out;
    }

    switch(urb->status) {
    case 0:
        break;
    case -ENOENT: /* cancelled */
    case -ECONNRESET:
        break;
    case -ESHUTDOWN:
        usbi_dbg(TRANSFER_CTX(transfer), "device removed");
        status = LIBUSB_TRANSFER_NO_DEVICE;
        break;
    default:
        usbi_warn(TRANSFER_CTX(transfer), "unrecognised urb status %d", urb->status);
        status = LIBUSB_TRANSFER_ERROR;
        break;
    }

    /* if we've reaped all urbs then we're done */
    if(tpriv->num_retired == num_urbs) {
        usbi_dbg(TRANSFER_CTX(transfer), "all URBs in transfer reaped --> complete!");
        free_iso_urbs(tpriv);
        usbi_mutex_unlock(&itransfer->lock);
        return usbi_handle_transfer_completion(itransfer, status);
    }

out:
    usbi_mutex_unlock(&itransfer->lock);
    return 0;
}

static int handle_control_completion(struct usbi_transfer *itransfer, struct usbfs_urb *urb) {
    struct linux_transfer_priv *tpriv = usbi_get_transfer_priv(itransfer);
    int                         status;

    usbi_mutex_lock(&itransfer->lock);
    usbi_dbg(ITRANSFER_CTX(itransfer), "handling completion status %d", urb->status);

    itransfer->transferred += urb->actual_length;

    if(tpriv->reap_action == CANCELLED) {
        if(urb->status && urb->status != -ENOENT)
            usbi_warn(ITRANSFER_CTX(itransfer), "cancel: unrecognised urb status %d", urb->status);
        free(tpriv->urbs);
        tpriv->urbs = NULL;
        usbi_mutex_unlock(&itransfer->lock);
        return usbi_handle_transfer_cancellation(itransfer);
    }

    switch(urb->status) {
    case 0:
        status = LIBUSB_TRANSFER_COMPLETED;
        break;
    case -ENOENT: /* cancelled */
        status = LIBUSB_TRANSFER_CANCELLED;
        break;
    case -ENODEV:
    case -ESHUTDOWN:
        usbi_dbg(ITRANSFER_CTX(itransfer), "device removed");
        status = LIBUSB_TRANSFER_NO_DEVICE;
        break;
    case -EPIPE:
        usbi_dbg(ITRANSFER_CTX(itransfer), "unsupported control request");
        status = LIBUSB_TRANSFER_STALL;
        break;
    case -EOVERFLOW:
        usbi_dbg(ITRANSFER_CTX(itransfer), "overflow, actual_length=%d", urb->actual_length);
        status = LIBUSB_TRANSFER_OVERFLOW;
        break;
    case -ETIME:
    case -EPROTO:
    case -EILSEQ:
    case -ECOMM:
    case -ENOSR:
        usbi_dbg(ITRANSFER_CTX(itransfer), "low-level bus error %d", urb->status);
        status = LIBUSB_TRANSFER_ERROR;
        break;
    default:
        usbi_warn(ITRANSFER_CTX(itransfer), "unrecognised urb status %d", urb->status);
        status = LIBUSB_TRANSFER_ERROR;
        break;
    }

    free(tpriv->urbs);
    tpriv->urbs = NULL;
    usbi_mutex_unlock(&itransfer->lock);
    return usbi_handle_transfer_completion(itransfer, status);
}

static int reap_for_handle(struct libusb_device_handle *handle) {
    struct linux_device_handle_priv *hpriv = usbi_get_device_handle_priv(handle);
    int                              r;
    struct usbfs_urb                *urb = NULL;
    struct usbi_transfer            *itransfer;
    struct libusb_transfer          *transfer;

    r = ioctl(hpriv->fd, IOCTL_USBFS_REAPURBNDELAY, &urb);
    if(r < 0) {
        if(errno == EAGAIN)
            return 1;
        if(errno == ENODEV)
            return LIBUSB_ERROR_NO_DEVICE;

        usbi_err(HANDLE_CTX(handle), "reap failed, errno=%d", errno);
        return LIBUSB_ERROR_IO;
    }

    itransfer = urb->usercontext;
    transfer  = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);

    usbi_dbg(HANDLE_CTX(handle), "urb type=%u status=%d transferred=%d", urb->type, urb->status, urb->actual_length);

    switch(transfer->type) {
    case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
        return handle_iso_completion(itransfer, urb);
    case LIBUSB_TRANSFER_TYPE_BULK:
    case LIBUSB_TRANSFER_TYPE_BULK_STREAM:
    case LIBUSB_TRANSFER_TYPE_INTERRUPT:
        return handle_bulk_completion(itransfer, urb);
    case LIBUSB_TRANSFER_TYPE_CONTROL:
        return handle_control_completion(itransfer, urb);
    default:
        usbi_err(HANDLE_CTX(handle), "unrecognised transfer type %u", transfer->type);
        return LIBUSB_ERROR_OTHER;
    }
}

static int op_handle_events(struct libusb_context *ctx, void *event_data, unsigned int count, unsigned int num_ready) {
    struct pollfd *fds = event_data;
    unsigned int   n;
    int            r;

    usbi_mutex_lock(&ctx->open_devs_lock);
    for(n = 0; n < count && num_ready > 0; n++) {
        struct pollfd                   *pollfd = &fds[n];
        struct libusb_device_handle     *handle;
        struct linux_device_handle_priv *hpriv = NULL;
        int                              reap_count;

        if(!pollfd->revents)
            continue;

        num_ready--;
        for_each_open_device(ctx, handle) {
            hpriv = usbi_get_device_handle_priv(handle);
            if(hpriv->fd == pollfd->fd)
                break;
        }

        if(!hpriv || hpriv->fd != pollfd->fd) {
            usbi_err(ctx, "cannot find handle for fd %d", pollfd->fd);
            continue;
        }

        if(pollfd->revents & POLLERR) {
            /* remove the fd from the pollfd set so that it doesn't continuously
             * trigger an event, and flag that it has been removed so op_close()
             * doesn't try to remove it a second time */
            usbi_remove_event_source(HANDLE_CTX(handle), hpriv->fd);
            hpriv->fd_removed = 1;

            /* device will still be marked as attached if hotplug monitor thread
             * hasn't processed remove event yet */
            usbi_mutex_static_lock(&linux_hotplug_lock);
            if(usbi_atomic_load(&handle->dev->attached))
                linux_device_disconnected(handle->dev->bus_number, handle->dev->device_address);
            usbi_mutex_static_unlock(&linux_hotplug_lock);

            if(hpriv->caps & USBFS_CAP_REAP_AFTER_DISCONNECT) {
                do {
                    r = reap_for_handle(handle);
                } while(r == 0);
            }

            usbi_handle_disconnect(handle);
            continue;
        }

        reap_count = 0;
        do {
            r = reap_for_handle(handle);
        } while(r == 0 && ++reap_count <= 25);

        if(r == 1 || r == LIBUSB_ERROR_NO_DEVICE)
            continue;
        else if(r < 0)
            goto out;
    }

    r = 0;
out:
    usbi_mutex_unlock(&ctx->open_devs_lock);
    return r;
}

const struct usbi_os_backend usbi_backend = {
    .name                           = "Linux usbfs",
    .caps                           = USBI_CAP_HAS_HID_ACCESS | USBI_CAP_SUPPORTS_DETACH_KERNEL_DRIVER,
    .init                           = op_init,
    .exit                           = op_exit,
    .set_option                     = op_set_option,
    .hotplug_poll                   = op_hotplug_poll,
    .get_active_config_descriptor   = op_get_active_config_descriptor,
    .get_config_descriptor          = op_get_config_descriptor,
    .get_config_descriptor_by_value = op_get_config_descriptor_by_value,

    .wrap_sys_device   = op_wrap_sys_device,
    .open              = op_open,
    .close             = op_close,
    .get_configuration = op_get_configuration,
    .set_configuration = op_set_configuration,
    .claim_interface   = op_claim_interface,
    .release_interface = op_release_interface,

    .set_interface_altsetting = op_set_interface,
    .clear_halt               = op_clear_halt,
    .reset_device             = op_reset_device,

    .alloc_streams = op_alloc_streams,
    .free_streams  = op_free_streams,

    .dev_mem_alloc = op_dev_mem_alloc,
    .dev_mem_free  = op_dev_mem_free,

    .kernel_driver_active = op_kernel_driver_active,
    .detach_kernel_driver = op_detach_kernel_driver,
    .attach_kernel_driver = op_attach_kernel_driver,

    .destroy_device = op_destroy_device,

    .submit_transfer     = op_submit_transfer,
    .cancel_transfer     = op_cancel_transfer,
    .clear_transfer_priv = op_clear_transfer_priv,

    .handle_events = op_handle_events,

    .context_priv_size       = sizeof(struct linux_context_priv),
    .device_priv_size        = sizeof(struct linux_device_priv),
    .device_handle_priv_size = sizeof(struct linux_device_handle_priv),
    .transfer_priv_size      = sizeof(struct linux_transfer_priv),
};
