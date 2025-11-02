/*
 * Copyright © 2011-2013 Martin Pieuchot <mpi@openbsd.org>
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
    char *devname; /* name of the ugen(4) node */
    int   fd;      /* device file descriptor */

    usb_config_descriptor_t *cdesc; /* active config descriptor */
};

struct handle_priv {
    int endpoints[USB_MAX_ENDPOINTS];
};

/*
 * Backend functions
 */
static int  obsd_get_device_list(struct libusb_context *, struct discovered_devs **);
static int  obsd_open(struct libusb_device_handle *);
static void obsd_close(struct libusb_device_handle *);

static int obsd_get_active_config_descriptor(struct libusb_device *, void *, size_t);
static int obsd_get_config_descriptor(struct libusb_device *, uint8_t, void *, size_t);

static int obsd_get_configuration(struct libusb_device_handle *, uint8_t *);
static int obsd_set_configuration(struct libusb_device_handle *, int);

static int obsd_claim_interface(struct libusb_device_handle *, uint8_t);
static int obsd_release_interface(struct libusb_device_handle *, uint8_t);

static int  obsd_set_interface_altsetting(struct libusb_device_handle *, uint8_t, uint8_t);
static int  obsd_clear_halt(struct libusb_device_handle *, unsigned char);
static void obsd_destroy_device(struct libusb_device *);

static int obsd_submit_transfer(struct usbi_transfer *);
static int obsd_cancel_transfer(struct usbi_transfer *);
static int obsd_handle_transfer_completion(struct usbi_transfer *);

/*
 * Private functions
 */
static int _errno_to_libusb(int);
static int _cache_active_config_descriptor(struct libusb_device *);
static int _sync_control_transfer(struct usbi_transfer *);
static int _sync_gen_transfer(struct usbi_transfer *);
static int _access_endpoint(struct libusb_transfer *);

static int _bus_open(int);

const struct usbi_os_backend usbi_backend = {
    .name            = "Synchronous OpenBSD backend",
    .get_device_list = obsd_get_device_list,
    .open            = obsd_open,
    .close           = obsd_close,

    .get_active_config_descriptor = obsd_get_active_config_descriptor,
    .get_config_descriptor        = obsd_get_config_descriptor,

    .get_configuration = obsd_get_configuration,
    .set_configuration = obsd_set_configuration,

    .claim_interface   = obsd_claim_interface,
    .release_interface = obsd_release_interface,

    .set_interface_altsetting = obsd_set_interface_altsetting,
    .clear_halt               = obsd_clear_halt,
    .destroy_device           = obsd_destroy_device,

    .submit_transfer = obsd_submit_transfer,
    .cancel_transfer = obsd_cancel_transfer,

    .handle_transfer_completion = obsd_handle_transfer_completion,

    .device_priv_size        = sizeof(struct device_priv),
    .device_handle_priv_size = sizeof(struct handle_priv),
};

#define DEVPATH "/dev/"
#define USBDEV DEVPATH "usb"

int obsd_get_device_list(struct libusb_context *ctx, struct discovered_devs **discdevs) {
    struct discovered_devs *ddd;
    struct libusb_device   *dev;
    struct device_priv     *dpriv;
    struct usb_device_info  di;
    struct usb_device_ddesc dd;
    unsigned long           session_id;
    char                    devices[USB_MAX_DEVICES];
    char                    busnode[16];
    char                   *udevname;
    int                     fd, addr, i, j;

    usbi_dbg(ctx, " ");

    for(i = 0; i < 8; i++) {
        snprintf(busnode, sizeof(busnode), USBDEV "%d", i);

        if((fd = open(busnode, O_RDWR)) < 0) {
            if(errno != ENOENT && errno != ENXIO)
                usbi_err(ctx, "could not open %s", busnode);
            continue;
        }

        bzero(devices, sizeof(devices));
        for(addr = 1; addr < USB_MAX_DEVICES; addr++) {
            if(devices[addr])
                continue;

            di.udi_addr = addr;
            if(ioctl(fd, USB_DEVICEINFO, &di) < 0)
                continue;

            /*
             * XXX If ugen(4) is attached to the USB device
             * it will be used.
             */
            udevname = NULL;
            for(j = 0; j < USB_MAX_DEVNAMES; j++)
                if(!strncmp("ugen", di.udi_devnames[j], 4)) {
                    udevname = strdup(di.udi_devnames[j]);
                    break;
                }

            session_id = (di.udi_bus << 8 | di.udi_addr);
            dev        = usbi_get_device_by_session_id(ctx, session_id);

            if(dev == NULL) {
                dev = usbi_alloc_device(ctx, session_id);
                if(dev == NULL) {
                    close(fd);
                    return (LIBUSB_ERROR_NO_MEM);
                }

                dev->bus_number     = di.udi_bus;
                dev->device_address = di.udi_addr;
                dev->speed          = di.udi_speed;
                dev->port_number    = di.udi_port;

                dpriv          = usbi_get_device_priv(dev);
                dpriv->fd      = -1;
                dpriv->devname = udevname;

                dd.udd_bus  = di.udi_bus;
                dd.udd_addr = di.udi_addr;
                if(ioctl(fd, USB_DEVICE_GET_DDESC, &dd) < 0) {
                    libusb_unref_device(dev);
                    continue;
                }

                static_assert(sizeof(dev->device_descriptor) == sizeof(dd.udd_desc), "mismatch between libusb and OS device descriptor sizes");
                memcpy(&dev->device_descriptor, &dd.udd_desc, LIBUSB_DT_DEVICE_SIZE);
                usbi_localize_device_descriptor(&dev->device_descriptor);

                if(_cache_active_config_descriptor(dev)) {
                    libusb_unref_device(dev);
                    continue;
                }

                if(usbi_sanitize_device(dev)) {
                    libusb_unref_device(dev);
                    continue;
                }
            }

            ddd = discovered_devs_append(*discdevs, dev);
            if(ddd == NULL) {
                close(fd);
                return (LIBUSB_ERROR_NO_MEM);
            }
            libusb_unref_device(dev);

            *discdevs     = ddd;
            devices[addr] = 1;
        }

        close(fd);
    }

    return (LIBUSB_SUCCESS);
}

int obsd_open(struct libusb_device_handle *handle) {
    struct device_priv *dpriv = usbi_get_device_priv(handle->dev);
    char                devnode[16];

    if(dpriv->devname) {
        int fd;
        /*
         * Only open ugen(4) attached devices read-write, all
         * read-only operations are done through the bus node.
         */
        snprintf(devnode, sizeof(devnode), DEVPATH "%s.00", dpriv->devname);
        fd = open(devnode, O_RDWR);
        if(fd < 0)
            return _errno_to_libusb(errno);
        dpriv->fd = fd;

        usbi_dbg(HANDLE_CTX(handle), "open %s: fd %d", devnode, dpriv->fd);
    }

    return (LIBUSB_SUCCESS);
}

void obsd_close(struct libusb_device_handle *handle) {
    struct device_priv *dpriv = usbi_get_device_priv(handle->dev);

    if(dpriv->devname) {
        usbi_dbg(HANDLE_CTX(handle), "close: fd %d", dpriv->fd);

        close(dpriv->fd);
        dpriv->fd = -1;
    }
}

int obsd_get_active_config_descriptor(struct libusb_device *dev, void *buf, size_t len) {
    struct device_priv *dpriv = usbi_get_device_priv(dev);

    len = MIN(len, (size_t)UGETW(dpriv->cdesc->wTotalLength));

    usbi_dbg(DEVICE_CTX(dev), "len %zu", len);

    memcpy(buf, dpriv->cdesc, len);

    return ((int)len);
}

int obsd_get_config_descriptor(struct libusb_device *dev, uint8_t idx, void *buf, size_t len) {
    struct usb_device_fdesc udf;
    int                     fd, err;

    if((fd = _bus_open(dev->bus_number)) < 0)
        return _errno_to_libusb(errno);

    udf.udf_bus          = dev->bus_number;
    udf.udf_addr         = dev->device_address;
    udf.udf_config_index = idx;
    udf.udf_size         = len;
    udf.udf_data         = buf;

    usbi_dbg(DEVICE_CTX(dev), "index %d, len %zu", udf.udf_config_index, len);

    if(ioctl(fd, USB_DEVICE_GET_FDESC, &udf) < 0) {
        err = errno;
        close(fd);
        return _errno_to_libusb(err);
    }
    close(fd);

    return ((int)len);
}

int obsd_get_configuration(struct libusb_device_handle *handle, uint8_t *config) {
    struct device_priv *dpriv = usbi_get_device_priv(handle->dev);

    *config = dpriv->cdesc->bConfigurationValue;

    usbi_dbg(HANDLE_CTX(handle), "bConfigurationValue %u", *config);

    return (LIBUSB_SUCCESS);
}

int obsd_set_configuration(struct libusb_device_handle *handle, int config) {
    struct device_priv *dpriv = usbi_get_device_priv(handle->dev);

    if(dpriv->devname == NULL)
        return (LIBUSB_ERROR_NOT_SUPPORTED);

    usbi_dbg(HANDLE_CTX(handle), "bConfigurationValue %d", config);

    if(ioctl(dpriv->fd, USB_SET_CONFIG, &config) < 0)
        return _errno_to_libusb(errno);

    return _cache_active_config_descriptor(handle->dev);
}

int obsd_claim_interface(struct libusb_device_handle *handle, uint8_t iface) {
    struct handle_priv *hpriv = usbi_get_device_handle_priv(handle);
    int                 i;

    UNUSED(iface);

    for(i = 0; i < USB_MAX_ENDPOINTS; i++)
        hpriv->endpoints[i] = -1;

    return (LIBUSB_SUCCESS);
}

int obsd_release_interface(struct libusb_device_handle *handle, uint8_t iface) {
    struct handle_priv *hpriv = usbi_get_device_handle_priv(handle);
    int                 i;

    UNUSED(iface);

    for(i = 0; i < USB_MAX_ENDPOINTS; i++)
        if(hpriv->endpoints[i] >= 0)
            close(hpriv->endpoints[i]);

    return (LIBUSB_SUCCESS);
}

int obsd_set_interface_altsetting(struct libusb_device_handle *handle, uint8_t iface, uint8_t altsetting) {
    struct device_priv      *dpriv = usbi_get_device_priv(handle->dev);
    struct usb_alt_interface intf;

    if(dpriv->devname == NULL)
        return (LIBUSB_ERROR_NOT_SUPPORTED);

    usbi_dbg(HANDLE_CTX(handle), "iface %u, setting %u", iface, altsetting);

    memset(&intf, 0, sizeof(intf));

    intf.uai_interface_index = iface;
    intf.uai_alt_no          = altsetting;

    if(ioctl(dpriv->fd, USB_SET_ALTINTERFACE, &intf) < 0)
        return _errno_to_libusb(errno);

    return (LIBUSB_SUCCESS);
}

int obsd_clear_halt(struct libusb_device_handle *handle, unsigned char endpoint) {
    struct usb_ctl_request req;
    int                    fd, err;

    if((fd = _bus_open(handle->dev->bus_number)) < 0)
        return _errno_to_libusb(errno);

    usbi_dbg(HANDLE_CTX(handle), " ");

    req.ucr_addr                  = handle->dev->device_address;
    req.ucr_request.bmRequestType = UT_WRITE_ENDPOINT;
    req.ucr_request.bRequest      = UR_CLEAR_FEATURE;
    USETW(req.ucr_request.wValue, UF_ENDPOINT_HALT);
    USETW(req.ucr_request.wIndex, endpoint);
    USETW(req.ucr_request.wLength, 0);

    if(ioctl(fd, USB_REQUEST, &req) < 0) {
        err = errno;
        close(fd);
        return _errno_to_libusb(err);
    }
    close(fd);

    return (LIBUSB_SUCCESS);
}

void obsd_destroy_device(struct libusb_device *dev) {
    struct device_priv *dpriv = usbi_get_device_priv(dev);

    usbi_dbg(DEVICE_CTX(dev), " ");

    free(dpriv->cdesc);
    free(dpriv->devname);
}

int obsd_submit_transfer(struct usbi_transfer *itransfer) {
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

int obsd_cancel_transfer(struct usbi_transfer *itransfer) {
    UNUSED(itransfer);

    usbi_dbg(ITRANSFER_CTX(itransfer), " ");

    return (LIBUSB_ERROR_NOT_SUPPORTED);
}

int obsd_handle_transfer_completion(struct usbi_transfer *itransfer) {
    return usbi_handle_transfer_completion(itransfer, LIBUSB_TRANSFER_COMPLETED);
}

int _errno_to_libusb(int err) {
    usbi_dbg(NULL, "error: %s (%d)", strerror(err), err);

    switch(err) {
    case EIO:
        return (LIBUSB_ERROR_IO);
    case EACCES:
        return (LIBUSB_ERROR_ACCESS);
    case ENOENT:
        return (LIBUSB_ERROR_NO_DEVICE);
    case ENOMEM:
        return (LIBUSB_ERROR_NO_MEM);
    case ETIMEDOUT:
        return (LIBUSB_ERROR_TIMEOUT);
    }

    return (LIBUSB_ERROR_OTHER);
}

int _cache_active_config_descriptor(struct libusb_device *dev) {
    struct device_priv     *dpriv = usbi_get_device_priv(dev);
    struct usb_device_cdesc udc;
    struct usb_device_fdesc udf;
    void                   *buf;
    int                     fd, len, err;

    if((fd = _bus_open(dev->bus_number)) < 0)
        return _errno_to_libusb(errno);

    usbi_dbg(DEVICE_CTX(dev), "fd %d, addr %d", fd, dev->device_address);

    udc.udc_bus          = dev->bus_number;
    udc.udc_addr         = dev->device_address;
    udc.udc_config_index = USB_CURRENT_CONFIG_INDEX;
    if(ioctl(fd, USB_DEVICE_GET_CDESC, &udc) < 0) {
        err = errno;
        close(fd);
        return _errno_to_libusb(errno);
    }

    usbi_dbg(DEVICE_CTX(dev), "active bLength %d", udc.udc_desc.bLength);

    len = UGETW(udc.udc_desc.wTotalLength);
    buf = malloc((size_t)len);
    if(buf == NULL)
        return (LIBUSB_ERROR_NO_MEM);

    udf.udf_bus          = dev->bus_number;
    udf.udf_addr         = dev->device_address;
    udf.udf_config_index = udc.udc_config_index;
    udf.udf_size         = len;
    udf.udf_data         = buf;

    usbi_dbg(DEVICE_CTX(dev), "index %d, len %d", udf.udf_config_index, len);

    if(ioctl(fd, USB_DEVICE_GET_FDESC, &udf) < 0) {
        err = errno;
        close(fd);
        free(buf);
        return _errno_to_libusb(err);
    }
    close(fd);

    if(dpriv->cdesc)
        free(dpriv->cdesc);
    dpriv->cdesc = buf;

    return (LIBUSB_SUCCESS);
}

int _sync_control_transfer(struct usbi_transfer *itransfer) {
    struct libusb_transfer      *transfer;
    struct libusb_control_setup *setup;
    struct device_priv          *dpriv;
    struct usb_ctl_request       req;

    transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    dpriv    = usbi_get_device_priv(transfer->dev_handle->dev);
    setup    = (struct libusb_control_setup *)transfer->buffer;

    usbi_dbg(ITRANSFER_CTX(itransfer), "type 0x%x request 0x%x value 0x%x index %d length %d timeout %d", setup->bmRequestType, setup->bRequest,
             libusb_le16_to_cpu(setup->wValue), libusb_le16_to_cpu(setup->wIndex), libusb_le16_to_cpu(setup->wLength), transfer->timeout);

    req.ucr_addr                  = transfer->dev_handle->dev->device_address;
    req.ucr_request.bmRequestType = setup->bmRequestType;
    req.ucr_request.bRequest      = setup->bRequest;
    /* Don't use USETW, libusb already deals with the endianness */
    (*(uint16_t *)req.ucr_request.wValue)  = setup->wValue;
    (*(uint16_t *)req.ucr_request.wIndex)  = setup->wIndex;
    (*(uint16_t *)req.ucr_request.wLength) = setup->wLength;
    req.ucr_data                           = transfer->buffer + LIBUSB_CONTROL_SETUP_SIZE;

    if((transfer->flags & LIBUSB_TRANSFER_SHORT_NOT_OK) == 0)
        req.ucr_flags = USBD_SHORT_XFER_OK;

    if(dpriv->devname == NULL) {
        /*
         * XXX If the device is not attached to ugen(4) it is
         * XXX still possible to submit a control transfer but
         * XXX with the default timeout only.
         */
        int fd, err;

        if((fd = _bus_open(transfer->dev_handle->dev->bus_number)) < 0)
            return _errno_to_libusb(errno);

        if((ioctl(fd, USB_REQUEST, &req)) < 0) {
            err = errno;
            close(fd);
            return _errno_to_libusb(err);
        }
        close(fd);
    }
    else {
        if((ioctl(dpriv->fd, USB_SET_TIMEOUT, &transfer->timeout)) < 0)
            return _errno_to_libusb(errno);

        if((ioctl(dpriv->fd, USB_DO_REQUEST, &req)) < 0)
            return _errno_to_libusb(errno);
    }

    itransfer->transferred = req.ucr_actlen;

    usbi_dbg(ITRANSFER_CTX(itransfer), "transferred %d", itransfer->transferred);

    return (0);
}

int _access_endpoint(struct libusb_transfer *transfer) {
    struct handle_priv *hpriv;
    struct device_priv *dpriv;
    char                devnode[16];
    int                 fd, endpt;
    mode_t              mode;

    hpriv = usbi_get_device_handle_priv(transfer->dev_handle);
    dpriv = usbi_get_device_priv(transfer->dev_handle->dev);

    endpt = UE_GET_ADDR(transfer->endpoint);
    mode  = IS_XFERIN(transfer) ? O_RDONLY : O_WRONLY;

    usbi_dbg(TRANSFER_CTX(transfer), "endpoint %d mode %d", endpt, mode);

    if(hpriv->endpoints[endpt] < 0) {
        /* Pick the right endpoint node */
        snprintf(devnode, sizeof(devnode), DEVPATH "%s.%02d", dpriv->devname, endpt);

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
    struct device_priv     *dpriv;
    int                     fd, nr = 1;

    transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    dpriv    = usbi_get_device_priv(transfer->dev_handle->dev);

    if(dpriv->devname == NULL)
        return (LIBUSB_ERROR_NOT_SUPPORTED);

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

int _bus_open(int number) {
    char busnode[16];

    snprintf(busnode, sizeof(busnode), USBDEV "%d", number);

    return open(busnode, O_RDWR);
}
