/*
 * Copyright © 2011 Martin Pieuchot <mpi@openbsd.org>
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

#include <sys/time.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <dev/usb/usb.h>

#include "libusbi.h"

struct device_priv {
    char devnode[16];
    int  fd;

    usb_config_descriptor_t *cdesc; /* active config descriptor */
};

struct handle_priv {
    int endpoints[USB_MAX_ENDPOINTS];
};

/*
 * Backend functions
 */
static int  netbsd_get_device_list(struct libusb_context *, struct discovered_devs **);
static int  netbsd_open(struct libusb_device_handle *);
static void netbsd_close(struct libusb_device_handle *);

static int netbsd_get_active_config_descriptor(struct libusb_device *, void *, size_t);
static int netbsd_get_config_descriptor(struct libusb_device *, uint8_t, void *, size_t);

static int netbsd_get_configuration(struct libusb_device_handle *, uint8_t *);
static int netbsd_set_configuration(struct libusb_device_handle *, int);

static int netbsd_claim_interface(struct libusb_device_handle *, uint8_t);
static int netbsd_release_interface(struct libusb_device_handle *, uint8_t);

static int  netbsd_set_interface_altsetting(struct libusb_device_handle *, uint8_t, uint8_t);
static int  netbsd_clear_halt(struct libusb_device_handle *, unsigned char);
static void netbsd_destroy_device(struct libusb_device *);

static int netbsd_submit_transfer(struct usbi_transfer *);
static int netbsd_cancel_transfer(struct usbi_transfer *);
static int netbsd_handle_transfer_completion(struct usbi_transfer *);

/*
 * Private functions
 */
static int _errno_to_libusb(int);
static int _cache_active_config_descriptor(struct libusb_device *, int);
static int _sync_control_transfer(struct usbi_transfer *);
static int _sync_gen_transfer(struct usbi_transfer *);
static int _access_endpoint(struct libusb_transfer *);

const struct usbi_os_backend usbi_backend = {
    .name            = "Synchronous NetBSD backend",
    .caps            = 0,
    .get_device_list = netbsd_get_device_list,
    .open            = netbsd_open,
    .close           = netbsd_close,

    .get_active_config_descriptor = netbsd_get_active_config_descriptor,
    .get_config_descriptor        = netbsd_get_config_descriptor,

    .get_configuration = netbsd_get_configuration,
    .set_configuration = netbsd_set_configuration,

    .claim_interface   = netbsd_claim_interface,
    .release_interface = netbsd_release_interface,

    .set_interface_altsetting = netbsd_set_interface_altsetting,
    .clear_halt               = netbsd_clear_halt,

    .destroy_device = netbsd_destroy_device,

    .submit_transfer = netbsd_submit_transfer,
    .cancel_transfer = netbsd_cancel_transfer,

    .handle_transfer_completion = netbsd_handle_transfer_completion,

    .device_priv_size        = sizeof(struct device_priv),
    .device_handle_priv_size = sizeof(struct handle_priv),
};

int netbsd_get_device_list(struct libusb_context *ctx, struct discovered_devs **discdevs) {
    struct libusb_device   *dev;
    struct device_priv     *dpriv;
    struct usb_device_info  di;
    usb_device_descriptor_t ddesc;
    unsigned long           session_id;
    char                    devnode[16];
    int                     fd, err, i;

    usbi_dbg(ctx, " ");

    /* Only ugen(4) is supported */
    for(i = 0; i < USB_MAX_DEVICES; i++) {
        /* Control endpoint is always .00 */
        snprintf(devnode, sizeof(devnode), "/dev/ugen%d.00", i);

        if((fd = open(devnode, O_RDONLY)) < 0) {
            if(errno != ENOENT && errno != ENXIO)
                usbi_err(ctx, "could not open %s", devnode);
            continue;
        }

        if(ioctl(fd, USB_GET_DEVICEINFO, &di) < 0)
            continue;

        session_id = (di.udi_bus << 8 | di.udi_addr);
        dev        = usbi_get_device_by_session_id(ctx, session_id);

        if(dev == NULL) {
            dev = usbi_alloc_device(ctx, session_id);
            if(dev == NULL)
                return (LIBUSB_ERROR_NO_MEM);

            dev->bus_number     = 1 + di.udi_bus;
            dev->device_address = 1 + di.udi_addr;
            dev->speed          = di.udi_speed; /* NetBSD #define's happen to match libusb enum */

            dpriv = usbi_get_device_priv(dev);
            strlcpy(dpriv->devnode, devnode, sizeof(devnode));
            dpriv->fd = -1;

            if(ioctl(fd, USB_GET_DEVICE_DESC, &ddesc) < 0) {
                err = errno;
                goto error;
            }

            static_assert(sizeof(dev->device_descriptor) == sizeof(ddesc), "mismatch between libusb and OS device descriptor sizes");
            memcpy(&dev->device_descriptor, &ddesc, LIBUSB_DT_DEVICE_SIZE);
            usbi_localize_device_descriptor(&dev->device_descriptor);

            if(_cache_active_config_descriptor(dev, fd)) {
                err = errno;
                goto error;
            }

            if((err = usbi_sanitize_device(dev)))
                goto error;
        }
        close(fd);

        if(discovered_devs_append(*discdevs, dev) == NULL)
            return (LIBUSB_ERROR_NO_MEM);

        libusb_unref_device(dev);
    }

    return (LIBUSB_SUCCESS);

error:
    close(fd);
    libusb_unref_device(dev);
    return _errno_to_libusb(err);
}

int netbsd_open(struct libusb_device_handle *handle) {
    struct device_priv *dpriv = usbi_get_device_priv(handle->dev);
    struct handle_priv *hpriv = usbi_get_device_handle_priv(handle);
    int                 i;

    dpriv->fd = open(dpriv->devnode, O_RDWR);
    if(dpriv->fd < 0) {
        dpriv->fd = open(dpriv->devnode, O_RDONLY);
        if(dpriv->fd < 0)
            return _errno_to_libusb(errno);
    }

    for(i = 0; i < USB_MAX_ENDPOINTS; i++)
        hpriv->endpoints[i] = -1;

    usbi_dbg(HANDLE_CTX(handle), "open %s: fd %d", dpriv->devnode, dpriv->fd);

    return (LIBUSB_SUCCESS);
}

void netbsd_close(struct libusb_device_handle *handle) {
    struct device_priv *dpriv = usbi_get_device_priv(handle->dev);

    usbi_dbg(HANDLE_CTX(handle), "close: fd %d", dpriv->fd);

    close(dpriv->fd);
    dpriv->fd = -1;
}

int netbsd_get_active_config_descriptor(struct libusb_device *dev, void *buf, size_t len) {
    struct device_priv *dpriv = usbi_get_device_priv(dev);

    len = MIN(len, (size_t)UGETW(dpriv->cdesc->wTotalLength));

    usbi_dbg(DEVICE_CTX(dev), "len %zu", len);

    memcpy(buf, dpriv->cdesc, len);

    return (int)len;
}

int netbsd_get_config_descriptor(struct libusb_device *dev, uint8_t idx, void *buf, size_t len) {
    struct device_priv  *dpriv = usbi_get_device_priv(dev);
    struct usb_full_desc ufd;
    int                  fd, err;

    usbi_dbg(DEVICE_CTX(dev), "index %u, len %zu", idx, len);

    /* A config descriptor may be requested before opening the device */
    if(dpriv->fd >= 0) {
        fd = dpriv->fd;
    }
    else {
        fd = open(dpriv->devnode, O_RDONLY);
        if(fd < 0)
            return _errno_to_libusb(errno);
    }

    ufd.ufd_config_index = idx;
    ufd.ufd_size         = len;
    ufd.ufd_data         = buf;

    if((ioctl(fd, USB_GET_FULL_DESC, &ufd)) < 0) {
        err = errno;
        if(dpriv->fd < 0)
            close(fd);
        return _errno_to_libusb(err);
    }

    if(dpriv->fd < 0)
        close(fd);

    return (int)len;
}

int netbsd_get_configuration(struct libusb_device_handle *handle, uint8_t *config) {
    struct device_priv *dpriv = usbi_get_device_priv(handle->dev);
    int                 tmp;

    usbi_dbg(HANDLE_CTX(handle), " ");

    if(ioctl(dpriv->fd, USB_GET_CONFIG, &tmp) < 0)
        return _errno_to_libusb(errno);

    usbi_dbg(HANDLE_CTX(handle), "configuration %d", tmp);
    *config = (uint8_t)tmp;

    return (LIBUSB_SUCCESS);
}

int netbsd_set_configuration(struct libusb_device_handle *handle, int config) {
    struct device_priv *dpriv = usbi_get_device_priv(handle->dev);

    usbi_dbg(HANDLE_CTX(handle), "configuration %d", config);

    if(ioctl(dpriv->fd, USB_SET_CONFIG, &config) < 0)
        return _errno_to_libusb(errno);

    return _cache_active_config_descriptor(handle->dev, dpriv->fd);
}

int netbsd_claim_interface(struct libusb_device_handle *handle, uint8_t iface) {
    struct handle_priv *hpriv = usbi_get_device_handle_priv(handle);
    int                 i;

    UNUSED(iface);

    for(i = 0; i < USB_MAX_ENDPOINTS; i++)
        hpriv->endpoints[i] = -1;

    return (LIBUSB_SUCCESS);
}

int netbsd_release_interface(struct libusb_device_handle *handle, uint8_t iface) {
    struct handle_priv *hpriv = usbi_get_device_handle_priv(handle);
    int                 i;

    UNUSED(iface);

    for(i = 0; i < USB_MAX_ENDPOINTS; i++)
        if(hpriv->endpoints[i] >= 0)
            close(hpriv->endpoints[i]);

    return (LIBUSB_SUCCESS);
}

int netbsd_set_interface_altsetting(struct libusb_device_handle *handle, uint8_t iface, uint8_t altsetting) {
    struct device_priv      *dpriv = usbi_get_device_priv(handle->dev);
    struct usb_alt_interface intf;

    usbi_dbg(HANDLE_CTX(handle), "iface %u, setting %u", iface, altsetting);

    memset(&intf, 0, sizeof(intf));

    intf.uai_interface_index = iface;
    intf.uai_alt_no          = altsetting;

    if(ioctl(dpriv->fd, USB_SET_ALTINTERFACE, &intf) < 0)
        return _errno_to_libusb(errno);

    return (LIBUSB_SUCCESS);
}

int netbsd_clear_halt(struct libusb_device_handle *handle, unsigned char endpoint) {
    struct device_priv    *dpriv = usbi_get_device_priv(handle->dev);
    struct usb_ctl_request req;

    usbi_dbg(HANDLE_CTX(handle), " ");

    req.ucr_request.bmRequestType = UT_WRITE_ENDPOINT;
    req.ucr_request.bRequest      = UR_CLEAR_FEATURE;
    USETW(req.ucr_request.wValue, UF_ENDPOINT_HALT);
    USETW(req.ucr_request.wIndex, endpoint);
    USETW(req.ucr_request.wLength, 0);

    if(ioctl(dpriv->fd, USB_DO_REQUEST, &req) < 0)
        return _errno_to_libusb(errno);

    return (LIBUSB_SUCCESS);
}

void netbsd_destroy_device(struct libusb_device *dev) {
    struct device_priv *dpriv = usbi_get_device_priv(dev);

    usbi_dbg(DEVICE_CTX(dev), " ");

    free(dpriv->cdesc);
}

int netbsd_submit_transfer(struct usbi_transfer *itransfer) {
    struct libusb_transfer *transfer;
    int                     err = 0;

    usbi_dbg(ITRANSFER_CTX(itransfer), " ");

    transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);

    switch(transfer->type) {
    case LIBUSB_TRANSFER_TYPE_CONTROL:
        err = _sync_control_transfer(itransfer);
        break;
    case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
        if(IS_XFEROUT(transfer)) {
            /* Isochronous write is not supported */
            err = LIBUSB_ERROR_NOT_SUPPORTED;
            break;
        }
        err = _sync_gen_transfer(itransfer);
        break;
    case LIBUSB_TRANSFER_TYPE_BULK:
    case LIBUSB_TRANSFER_TYPE_INTERRUPT:
        if(IS_XFEROUT(transfer) && transfer->flags & LIBUSB_TRANSFER_ADD_ZERO_PACKET) {
            err = LIBUSB_ERROR_NOT_SUPPORTED;
            break;
        }
        err = _sync_gen_transfer(itransfer);
        break;
    case LIBUSB_TRANSFER_TYPE_BULK_STREAM:
        err = LIBUSB_ERROR_NOT_SUPPORTED;
        break;
    }

    if(err)
        return (err);

    usbi_signal_transfer_completion(itransfer);

    return (LIBUSB_SUCCESS);
}

int netbsd_cancel_transfer(struct usbi_transfer *itransfer) {
    UNUSED(itransfer);

    usbi_dbg(ITRANSFER_CTX(itransfer), " ");

    return (LIBUSB_ERROR_NOT_SUPPORTED);
}

int netbsd_handle_transfer_completion(struct usbi_transfer *itransfer) {
    return usbi_handle_transfer_completion(itransfer, LIBUSB_TRANSFER_COMPLETED);
}

int _errno_to_libusb(int err) {
    switch(err) {
    case EIO:
        return (LIBUSB_ERROR_IO);
    case EACCES:
        return (LIBUSB_ERROR_ACCESS);
    case ENOENT:
        return (LIBUSB_ERROR_NO_DEVICE);
    case ENOMEM:
        return (LIBUSB_ERROR_NO_MEM);
    case EWOULDBLOCK:
    case ETIMEDOUT:
        return (LIBUSB_ERROR_TIMEOUT);
    }

    usbi_dbg(NULL, "error: %s", strerror(err));

    return (LIBUSB_ERROR_OTHER);
}

int _cache_active_config_descriptor(struct libusb_device *dev, int fd) {
    struct device_priv    *dpriv = usbi_get_device_priv(dev);
    struct usb_config_desc ucd;
    struct usb_full_desc   ufd;
    void                  *buf;
    int                    len;

    usbi_dbg(DEVICE_CTX(dev), "fd %d", fd);

    ucd.ucd_config_index = USB_CURRENT_CONFIG_INDEX;

    if((ioctl(fd, USB_GET_CONFIG_DESC, &ucd)) < 0)
        return _errno_to_libusb(errno);

    usbi_dbg(DEVICE_CTX(dev), "active bLength %d", ucd.ucd_desc.bLength);

    len = UGETW(ucd.ucd_desc.wTotalLength);
    buf = malloc((size_t)len);
    if(buf == NULL)
        return (LIBUSB_ERROR_NO_MEM);

    ufd.ufd_config_index = ucd.ucd_config_index;
    ufd.ufd_size         = len;
    ufd.ufd_data         = buf;

    usbi_dbg(DEVICE_CTX(dev), "index %d, len %d", ufd.ufd_config_index, len);

    if((ioctl(fd, USB_GET_FULL_DESC, &ufd)) < 0) {
        free(buf);
        return _errno_to_libusb(errno);
    }

    if(dpriv->cdesc)
        free(dpriv->cdesc);
    dpriv->cdesc = buf;

    return (0);
}

int _sync_control_transfer(struct usbi_transfer *itransfer) {
    struct libusb_transfer      *transfer;
    struct libusb_control_setup *setup;
    struct device_priv          *dpriv;
    struct usb_ctl_request       req;

    transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    dpriv    = usbi_get_device_priv(transfer->dev_handle->dev);
    setup    = (struct libusb_control_setup *)transfer->buffer;

    usbi_dbg(ITRANSFER_CTX(itransfer), "type %d request %d value %d index %d length %d timeout %d", setup->bmRequestType, setup->bRequest,
             libusb_le16_to_cpu(setup->wValue), libusb_le16_to_cpu(setup->wIndex), libusb_le16_to_cpu(setup->wLength), transfer->timeout);

    req.ucr_request.bmRequestType = setup->bmRequestType;
    req.ucr_request.bRequest      = setup->bRequest;
    /* Don't use USETW, libusb already deals with the endianness */
    (*(uint16_t *)req.ucr_request.wValue)  = setup->wValue;
    (*(uint16_t *)req.ucr_request.wIndex)  = setup->wIndex;
    (*(uint16_t *)req.ucr_request.wLength) = setup->wLength;
    req.ucr_data                           = transfer->buffer + LIBUSB_CONTROL_SETUP_SIZE;

    if((transfer->flags & LIBUSB_TRANSFER_SHORT_NOT_OK) == 0)
        req.ucr_flags = USBD_SHORT_XFER_OK;

    if((ioctl(dpriv->fd, USB_SET_TIMEOUT, &transfer->timeout)) < 0)
        return _errno_to_libusb(errno);

    if((ioctl(dpriv->fd, USB_DO_REQUEST, &req)) < 0)
        return _errno_to_libusb(errno);

    itransfer->transferred = req.ucr_actlen;

    usbi_dbg(ITRANSFER_CTX(itransfer), "transferred %d", itransfer->transferred);

    return (0);
}

int _access_endpoint(struct libusb_transfer *transfer) {
    struct handle_priv *hpriv;
    struct device_priv *dpriv;
    char               *s, devnode[16];
    int                 fd, endpt;
    mode_t              mode;

    hpriv = usbi_get_device_handle_priv(transfer->dev_handle);
    dpriv = usbi_get_device_priv(transfer->dev_handle->dev);

    endpt = UE_GET_ADDR(transfer->endpoint);
    mode  = IS_XFERIN(transfer) ? O_RDONLY : O_WRONLY;

    usbi_dbg(TRANSFER_CTX(transfer), "endpoint %d mode %d", endpt, mode);

    if(hpriv->endpoints[endpt] < 0) {
        /* Pick the right node given the control one */
        strlcpy(devnode, dpriv->devnode, sizeof(devnode));
        s = strchr(devnode, '.');
        snprintf(s, 4, ".%02d", endpt);

        /* We may need to read/write to the same endpoint later. */
        if(((fd = open(devnode, O_RDWR)) < 0) && (errno == ENXIO))
            if((fd = open(devnode, mode)) < 0)
                return (-1);

        hpriv->endpoints[endpt] = fd;
    }

    return (hpriv->endpoints[endpt]);
}

int _sync_gen_transfer(struct usbi_transfer *itransfer) {
    struct libusb_transfer *transfer;
    int                     fd, nr = 1;

    transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);

    /*
     * Bulk, Interrupt or Isochronous transfer depends on the
     * endpoint and thus the node to open.
     */
    if((fd = _access_endpoint(transfer)) < 0)
        return _errno_to_libusb(errno);

    if((ioctl(fd, USB_SET_TIMEOUT, &transfer->timeout)) < 0)
        return _errno_to_libusb(errno);

    if(IS_XFERIN(transfer)) {
        if((transfer->flags & LIBUSB_TRANSFER_SHORT_NOT_OK) == 0)
            if((ioctl(fd, USB_SET_SHORT_XFER, &nr)) < 0)
                return _errno_to_libusb(errno);

        nr = read(fd, transfer->buffer, transfer->length);
    }
    else {
        nr = write(fd, transfer->buffer, transfer->length);
    }

    if(nr < 0)
        return _errno_to_libusb(errno);

    itransfer->transferred = nr;

    return (0);
}
