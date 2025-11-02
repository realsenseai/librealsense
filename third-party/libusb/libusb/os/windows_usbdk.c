/*
 * windows UsbDk backend for libusb 1.0
 * Copyright © 2014 Red Hat, Inc.

 * Authors:
 * Dmitry Fleytman <dmitry@daynix.com>
 * Pavel Gurvich <pavel@daynix.com>
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

#include <windows.h>
#include <stdio.h>

#include "libusbi.h"
#include "windows_usbdk.h"

#if !defined(STATUS_SUCCESS)
typedef LONG NTSTATUS;
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

#if !defined(STATUS_CANCELLED)
#define STATUS_CANCELLED ((NTSTATUS)0xC0000120L)
#endif

#if !defined(STATUS_REQUEST_CANCELED)
#define STATUS_REQUEST_CANCELED ((NTSTATUS)0xC0000703L)
#endif

static struct {
    HMODULE module;

    USBDK_GET_DEVICES_LIST                 GetDevicesList;
    USBDK_RELEASE_DEVICES_LIST             ReleaseDevicesList;
    USBDK_START_REDIRECT                   StartRedirect;
    USBDK_STOP_REDIRECT                    StopRedirect;
    USBDK_GET_CONFIGURATION_DESCRIPTOR     GetConfigurationDescriptor;
    USBDK_RELEASE_CONFIGURATION_DESCRIPTOR ReleaseConfigurationDescriptor;
    USBDK_READ_PIPE                        ReadPipe;
    USBDK_WRITE_PIPE                       WritePipe;
    USBDK_ABORT_PIPE                       AbortPipe;
    USBDK_RESET_PIPE                       ResetPipe;
    USBDK_SET_ALTSETTING                   SetAltsetting;
    USBDK_RESET_DEVICE                     ResetDevice;
    USBDK_GET_REDIRECTOR_SYSTEM_HANDLE     GetRedirectorSystemHandle;
} usbdk_helper;

static FARPROC get_usbdk_proc_addr(struct libusb_context *ctx, LPCSTR api_name) {
    FARPROC api_ptr = GetProcAddress(usbdk_helper.module, api_name);

    if(api_ptr == NULL)
        usbi_err(ctx, "UsbDkHelper API %s not found: %s", api_name, windows_error_str(0));

    return api_ptr;
}

static void unload_usbdk_helper_dll(void) {
    if(usbdk_helper.module != NULL) {
        FreeLibrary(usbdk_helper.module);
        usbdk_helper.module = NULL;
    }
}

static int load_usbdk_helper_dll(struct libusb_context *ctx) {
    usbdk_helper.module = load_system_library(ctx, "UsbDkHelper");
    if(usbdk_helper.module == NULL) {
        usbi_err(ctx, "Failed to load UsbDkHelper.dll: %s", windows_error_str(0));
        return LIBUSB_ERROR_NOT_FOUND;
    }

    usbdk_helper.GetDevicesList = (USBDK_GET_DEVICES_LIST)get_usbdk_proc_addr(ctx, "UsbDk_GetDevicesList");
    if(usbdk_helper.GetDevicesList == NULL)
        goto error_unload;

    usbdk_helper.ReleaseDevicesList = (USBDK_RELEASE_DEVICES_LIST)get_usbdk_proc_addr(ctx, "UsbDk_ReleaseDevicesList");
    if(usbdk_helper.ReleaseDevicesList == NULL)
        goto error_unload;

    usbdk_helper.StartRedirect = (USBDK_START_REDIRECT)get_usbdk_proc_addr(ctx, "UsbDk_StartRedirect");
    if(usbdk_helper.StartRedirect == NULL)
        goto error_unload;

    usbdk_helper.StopRedirect = (USBDK_STOP_REDIRECT)get_usbdk_proc_addr(ctx, "UsbDk_StopRedirect");
    if(usbdk_helper.StopRedirect == NULL)
        goto error_unload;

    usbdk_helper.GetConfigurationDescriptor = (USBDK_GET_CONFIGURATION_DESCRIPTOR)get_usbdk_proc_addr(ctx, "UsbDk_GetConfigurationDescriptor");
    if(usbdk_helper.GetConfigurationDescriptor == NULL)
        goto error_unload;

    usbdk_helper.ReleaseConfigurationDescriptor = (USBDK_RELEASE_CONFIGURATION_DESCRIPTOR)get_usbdk_proc_addr(ctx, "UsbDk_ReleaseConfigurationDescriptor");
    if(usbdk_helper.ReleaseConfigurationDescriptor == NULL)
        goto error_unload;

    usbdk_helper.ReadPipe = (USBDK_READ_PIPE)get_usbdk_proc_addr(ctx, "UsbDk_ReadPipe");
    if(usbdk_helper.ReadPipe == NULL)
        goto error_unload;

    usbdk_helper.WritePipe = (USBDK_WRITE_PIPE)get_usbdk_proc_addr(ctx, "UsbDk_WritePipe");
    if(usbdk_helper.WritePipe == NULL)
        goto error_unload;

    usbdk_helper.AbortPipe = (USBDK_ABORT_PIPE)get_usbdk_proc_addr(ctx, "UsbDk_AbortPipe");
    if(usbdk_helper.AbortPipe == NULL)
        goto error_unload;

    usbdk_helper.ResetPipe = (USBDK_RESET_PIPE)get_usbdk_proc_addr(ctx, "UsbDk_ResetPipe");
    if(usbdk_helper.ResetPipe == NULL)
        goto error_unload;

    usbdk_helper.SetAltsetting = (USBDK_SET_ALTSETTING)get_usbdk_proc_addr(ctx, "UsbDk_SetAltsetting");
    if(usbdk_helper.SetAltsetting == NULL)
        goto error_unload;

    usbdk_helper.ResetDevice = (USBDK_RESET_DEVICE)get_usbdk_proc_addr(ctx, "UsbDk_ResetDevice");
    if(usbdk_helper.ResetDevice == NULL)
        goto error_unload;

    usbdk_helper.GetRedirectorSystemHandle = (USBDK_GET_REDIRECTOR_SYSTEM_HANDLE)get_usbdk_proc_addr(ctx, "UsbDk_GetRedirectorSystemHandle");
    if(usbdk_helper.GetRedirectorSystemHandle == NULL)
        goto error_unload;

    return LIBUSB_SUCCESS;

error_unload:
    FreeLibrary(usbdk_helper.module);
    usbdk_helper.module = NULL;
    return LIBUSB_ERROR_NOT_FOUND;
}

typedef SC_HANDLE(WINAPI *POPENSCMANAGERA)(LPCSTR, LPCSTR, DWORD);
typedef SC_HANDLE(WINAPI *POPENSERVICEA)(SC_HANDLE, LPCSTR, DWORD);
typedef BOOL(WINAPI *PCLOSESERVICEHANDLE)(SC_HANDLE);

static int usbdk_init(struct libusb_context *ctx) {
    POPENSCMANAGERA     pOpenSCManagerA;
    POPENSERVICEA       pOpenServiceA;
    PCLOSESERVICEHANDLE pCloseServiceHandle;
    SC_HANDLE           managerHandle;
    SC_HANDLE           serviceHandle;
    HMODULE             h;

    h = load_system_library(ctx, "Advapi32");
    if(h == NULL) {
        usbi_warn(ctx, "failed to open Advapi32\n");
        return LIBUSB_ERROR_OTHER;
    }

    pOpenSCManagerA = (POPENSCMANAGERA)GetProcAddress(h, "OpenSCManagerA");
    if(pOpenSCManagerA == NULL) {
        usbi_warn(ctx, "failed to find %s in Advapi32\n", "OpenSCManagerA");
        goto error_free_library;
    }
    pOpenServiceA = (POPENSERVICEA)GetProcAddress(h, "OpenServiceA");
    if(pOpenServiceA == NULL) {
        usbi_warn(ctx, "failed to find %s in Advapi32\n", "OpenServiceA");
        goto error_free_library;
    }
    pCloseServiceHandle = (PCLOSESERVICEHANDLE)GetProcAddress(h, "CloseServiceHandle");
    if(pCloseServiceHandle == NULL) {
        usbi_warn(ctx, "failed to find %s in Advapi32\n", "CloseServiceHandle");
        goto error_free_library;
    }

    managerHandle = pOpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
    if(managerHandle == NULL) {
        usbi_warn(ctx, "failed to open service control manager: %s", windows_error_str(0));
        goto error_free_library;
    }

    serviceHandle = pOpenServiceA(managerHandle, "UsbDk", GENERIC_READ);
    pCloseServiceHandle(managerHandle);

    if(serviceHandle == NULL) {
        if(GetLastError() != ERROR_SERVICE_DOES_NOT_EXIST)
            usbi_warn(ctx, "failed to open UsbDk service: %s", windows_error_str(0));
        FreeLibrary(h);
        return LIBUSB_ERROR_NOT_FOUND;
    }

    pCloseServiceHandle(serviceHandle);
    FreeLibrary(h);

    return load_usbdk_helper_dll(ctx);

error_free_library:
    FreeLibrary(h);
    return LIBUSB_ERROR_OTHER;
}

static void usbdk_exit(struct libusb_context *ctx) {
    UNUSED(ctx);
    unload_usbdk_helper_dll();
}

static int usbdk_get_session_id_for_device(struct libusb_context *ctx, PUSB_DK_DEVICE_ID id, unsigned long *session_id) {
    char dev_identity[ARRAYSIZE(id->DeviceID) + ARRAYSIZE(id->InstanceID) + 1];

    if(snprintf(dev_identity, sizeof(dev_identity), "%S%S", id->DeviceID, id->InstanceID) == -1) {
        usbi_warn(ctx, "cannot form device identity");
        return LIBUSB_ERROR_NOT_SUPPORTED;
    }

    *session_id = htab_hash(dev_identity);

    return LIBUSB_SUCCESS;
}

static void usbdk_release_config_descriptors(struct usbdk_device_priv *priv, uint8_t count) {
    uint8_t i;

    for(i = 0; i < count; i++)
        usbdk_helper.ReleaseConfigurationDescriptor(priv->config_descriptors[i]);

    free(priv->config_descriptors);
    priv->config_descriptors = NULL;
}

static int usbdk_cache_config_descriptors(struct libusb_context *ctx, struct usbdk_device_priv *priv, PUSB_DK_DEVICE_INFO info) {
    uint8_t                          i;
    USB_DK_CONFIG_DESCRIPTOR_REQUEST Request;
    Request.ID = info->ID;

    priv->config_descriptors = calloc(info->DeviceDescriptor.bNumConfigurations, sizeof(PUSB_CONFIGURATION_DESCRIPTOR));
    if(priv->config_descriptors == NULL) {
        usbi_err(ctx, "failed to allocate configuration descriptors holder");
        return LIBUSB_ERROR_NO_MEM;
    }

    for(i = 0; i < info->DeviceDescriptor.bNumConfigurations; i++) {
        ULONG Length;

        Request.Index = i;
        if(!usbdk_helper.GetConfigurationDescriptor(&Request, &priv->config_descriptors[i], &Length)) {
            usbi_err(ctx, "failed to retrieve configuration descriptors");
            usbdk_release_config_descriptors(priv, i);
            return LIBUSB_ERROR_OTHER;
        }
    }

    return LIBUSB_SUCCESS;
}

static inline int usbdk_device_priv_init(struct libusb_context *ctx, struct libusb_device *dev, PUSB_DK_DEVICE_INFO info) {
    struct usbdk_device_priv *priv = usbi_get_device_priv(dev);

    priv->ID                   = info->ID;
    priv->active_configuration = 0;

    return usbdk_cache_config_descriptors(ctx, priv, info);
}

static void usbdk_device_init(struct libusb_device *dev, PUSB_DK_DEVICE_INFO info) {
    dev->bus_number  = (uint8_t)info->FilterID;
    dev->port_number = (uint8_t)info->Port;
    dev->parent_dev  = NULL;

    // Addresses in libusb are 1-based
    dev->device_address = (uint8_t)(info->Port + 1);

    static_assert(sizeof(dev->device_descriptor) == sizeof(info->DeviceDescriptor), "mismatch between libusb and OS device descriptor sizes");
    memcpy(&dev->device_descriptor, &info->DeviceDescriptor, LIBUSB_DT_DEVICE_SIZE);
    usbi_localize_device_descriptor(&dev->device_descriptor);

    switch(info->Speed) {
    case LowSpeed:
        dev->speed = LIBUSB_SPEED_LOW;
        break;
    case FullSpeed:
        dev->speed = LIBUSB_SPEED_FULL;
        break;
    case HighSpeed:
        dev->speed = LIBUSB_SPEED_HIGH;
        break;
    case SuperSpeed:
        dev->speed = LIBUSB_SPEED_SUPER;
        break;
    case NoSpeed:
    default:
        dev->speed = LIBUSB_SPEED_UNKNOWN;
        break;
    }
}

static int usbdk_get_device_list(struct libusb_context *ctx, struct discovered_devs **_discdevs) {
    int                     r = LIBUSB_SUCCESS;
    ULONG                   i;
    struct discovered_devs *discdevs = NULL;
    ULONG                   dev_number;
    PUSB_DK_DEVICE_INFO     devices;

    if(!usbdk_helper.GetDevicesList(&devices, &dev_number))
        return LIBUSB_ERROR_OTHER;

    for(i = 0; i < dev_number; i++) {
        unsigned long         session_id;
        struct libusb_device *dev = NULL;

        if(usbdk_get_session_id_for_device(ctx, &devices[i].ID, &session_id))
            continue;

        dev = usbi_get_device_by_session_id(ctx, session_id);
        if(dev == NULL) {
            dev = usbi_alloc_device(ctx, session_id);
            if(dev == NULL) {
                usbi_err(ctx, "failed to allocate a new device structure");
                continue;
            }

            usbdk_device_init(dev, &devices[i]);
            if(usbdk_device_priv_init(ctx, dev, &devices[i]) != LIBUSB_SUCCESS) {
                libusb_unref_device(dev);
                continue;
            }
        }

        discdevs = discovered_devs_append(*_discdevs, dev);
        libusb_unref_device(dev);
        if(!discdevs) {
            usbi_err(ctx, "cannot append new device to list");
            r = LIBUSB_ERROR_NO_MEM;
            goto func_exit;
        }

        *_discdevs = discdevs;
    }

func_exit:
    usbdk_helper.ReleaseDevicesList(devices);
    return r;
}

static int usbdk_get_config_descriptor(struct libusb_device *dev, uint8_t config_index, void *buffer, size_t len) {
    struct usbdk_device_priv     *priv = usbi_get_device_priv(dev);
    PUSB_CONFIGURATION_DESCRIPTOR config_header;
    size_t                        size;

    config_header = (PUSB_CONFIGURATION_DESCRIPTOR)priv->config_descriptors[config_index];

    size = min(config_header->wTotalLength, len);
    memcpy(buffer, config_header, size);
    return (int)size;
}

static int usbdk_get_config_descriptor_by_value(struct libusb_device *dev, uint8_t bConfigurationValue, void **buffer) {
    struct usbdk_device_priv     *priv = usbi_get_device_priv(dev);
    PUSB_CONFIGURATION_DESCRIPTOR config_header;
    uint8_t                       index;

    for(index = 0; index < dev->device_descriptor.bNumConfigurations; index++) {
        config_header = priv->config_descriptors[index];
        if(config_header->bConfigurationValue == bConfigurationValue) {
            *buffer = priv->config_descriptors[index];
            return (int)config_header->wTotalLength;
        }
    }

    return LIBUSB_ERROR_NOT_FOUND;
}

static int usbdk_get_active_config_descriptor(struct libusb_device *dev, void *buffer, size_t len) {
    struct usbdk_device_priv *priv = usbi_get_device_priv(dev);

    return usbdk_get_config_descriptor(dev, priv->active_configuration, buffer, len);
}

static int usbdk_open(struct libusb_device_handle *dev_handle) {
    struct libusb_device        *dev         = dev_handle->dev;
    struct libusb_context       *ctx         = DEVICE_CTX(dev);
    struct windows_context_priv *priv        = usbi_get_context_priv(ctx);
    struct usbdk_device_priv    *device_priv = usbi_get_device_priv(dev);

    device_priv->redirector_handle = usbdk_helper.StartRedirect(&device_priv->ID);
    if(device_priv->redirector_handle == INVALID_HANDLE_VALUE) {
        usbi_err(ctx, "Redirector startup failed");
        device_priv->redirector_handle = NULL;
        return LIBUSB_ERROR_OTHER;
    }

    device_priv->system_handle = usbdk_helper.GetRedirectorSystemHandle(device_priv->redirector_handle);

    if(CreateIoCompletionPort(device_priv->system_handle, priv->completion_port, (ULONG_PTR)dev_handle, 0) == NULL) {
        usbi_err(ctx, "failed to associate handle to I/O completion port: %s", windows_error_str(0));
        usbdk_helper.StopRedirect(device_priv->redirector_handle);
        device_priv->system_handle     = NULL;
        device_priv->redirector_handle = NULL;
        return LIBUSB_ERROR_OTHER;
    }

    return LIBUSB_SUCCESS;
}

static void usbdk_close(struct libusb_device_handle *dev_handle) {
    struct usbdk_device_priv *priv = usbi_get_device_priv(dev_handle->dev);

    if(!usbdk_helper.StopRedirect(priv->redirector_handle))
        usbi_err(HANDLE_CTX(dev_handle), "Redirector shutdown failed");

    priv->system_handle     = NULL;
    priv->redirector_handle = NULL;
}

static int usbdk_get_configuration(struct libusb_device_handle *dev_handle, uint8_t *config) {
    struct usbdk_device_priv *priv = usbi_get_device_priv(dev_handle->dev);

    *config = priv->active_configuration;

    return LIBUSB_SUCCESS;
}

static int usbdk_set_configuration(struct libusb_device_handle *dev_handle, uint8_t config) {
    UNUSED(dev_handle);
    UNUSED(config);
    return LIBUSB_SUCCESS;
}

static int usbdk_claim_interface(struct libusb_device_handle *dev_handle, uint8_t iface) {
    UNUSED(dev_handle);
    UNUSED(iface);
    return LIBUSB_SUCCESS;
}

static int usbdk_set_interface_altsetting(struct libusb_device_handle *dev_handle, uint8_t iface, uint8_t altsetting) {
    struct usbdk_device_priv *priv = usbi_get_device_priv(dev_handle->dev);

    if(!usbdk_helper.SetAltsetting(priv->redirector_handle, iface, altsetting)) {
        usbi_err(HANDLE_CTX(dev_handle), "SetAltsetting failed: %s", windows_error_str(0));
        return LIBUSB_ERROR_NO_DEVICE;
    }

    return LIBUSB_SUCCESS;
}

static int usbdk_release_interface(struct libusb_device_handle *dev_handle, uint8_t iface) {
    UNUSED(dev_handle);
    UNUSED(iface);
    return LIBUSB_SUCCESS;
}

static int usbdk_clear_halt(struct libusb_device_handle *dev_handle, unsigned char endpoint) {
    struct usbdk_device_priv *priv = usbi_get_device_priv(dev_handle->dev);

    if(!usbdk_helper.ResetPipe(priv->redirector_handle, endpoint)) {
        usbi_err(HANDLE_CTX(dev_handle), "ResetPipe failed: %s", windows_error_str(0));
        return LIBUSB_ERROR_NO_DEVICE;
    }

    return LIBUSB_SUCCESS;
}

static int usbdk_reset_device(struct libusb_device_handle *dev_handle) {
    struct usbdk_device_priv *priv = usbi_get_device_priv(dev_handle->dev);

    if(!usbdk_helper.ResetDevice(priv->redirector_handle)) {
        usbi_err(HANDLE_CTX(dev_handle), "ResetDevice failed: %s", windows_error_str(0));
        return LIBUSB_ERROR_NO_DEVICE;
    }

    return LIBUSB_SUCCESS;
}

static void usbdk_destroy_device(struct libusb_device *dev) {
    struct usbdk_device_priv *priv = usbi_get_device_priv(dev);

    if(priv->config_descriptors != NULL)
        usbdk_release_config_descriptors(priv, dev->device_descriptor.bNumConfigurations);
}

static void usbdk_clear_transfer_priv(struct usbi_transfer *itransfer) {
    struct usbdk_transfer_priv *transfer_priv = get_usbdk_transfer_priv(itransfer);
    struct libusb_transfer     *transfer      = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);

    if(transfer->type == LIBUSB_TRANSFER_TYPE_ISOCHRONOUS) {
        safe_free(transfer_priv->IsochronousPacketsArray);
        safe_free(transfer_priv->IsochronousResultsArray);
    }
}

static int usbdk_do_control_transfer(struct usbi_transfer *itransfer) {
    struct libusb_transfer     *transfer      = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    struct usbdk_device_priv   *priv          = usbi_get_device_priv(transfer->dev_handle->dev);
    struct usbdk_transfer_priv *transfer_priv = get_usbdk_transfer_priv(itransfer);
    OVERLAPPED                 *overlapped    = get_transfer_priv_overlapped(itransfer);
    TransferResult              transResult;

    transfer_priv->request.Buffer       = (PVOID64)transfer->buffer;
    transfer_priv->request.BufferLength = transfer->length;
    transfer_priv->request.TransferType = ControlTransferType;

    set_transfer_priv_handle(itransfer, priv->system_handle);

    if(transfer->buffer[0] & LIBUSB_ENDPOINT_IN)
        transResult = usbdk_helper.ReadPipe(priv->redirector_handle, &transfer_priv->request, overlapped);
    else
        transResult = usbdk_helper.WritePipe(priv->redirector_handle, &transfer_priv->request, overlapped);

    switch(transResult) {
    case TransferSuccess:
        windows_force_sync_completion(itransfer, (ULONG)transfer_priv->request.Result.GenResult.BytesTransferred);
        break;
    case TransferSuccessAsync:
        break;
    case TransferFailure:
        usbi_err(TRANSFER_CTX(transfer), "ControlTransfer failed: %s", windows_error_str(0));
        return LIBUSB_ERROR_IO;
    }

    return LIBUSB_SUCCESS;
}

static int usbdk_do_bulk_transfer(struct usbi_transfer *itransfer) {
    struct libusb_transfer     *transfer      = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    struct usbdk_device_priv   *priv          = usbi_get_device_priv(transfer->dev_handle->dev);
    struct usbdk_transfer_priv *transfer_priv = get_usbdk_transfer_priv(itransfer);
    OVERLAPPED                 *overlapped    = get_transfer_priv_overlapped(itransfer);
    TransferResult              transferRes;

    transfer_priv->request.Buffer          = (PVOID64)transfer->buffer;
    transfer_priv->request.BufferLength    = transfer->length;
    transfer_priv->request.EndpointAddress = transfer->endpoint;

    switch(transfer->type) {
    case LIBUSB_TRANSFER_TYPE_BULK:
        transfer_priv->request.TransferType = BulkTransferType;
        break;
    case LIBUSB_TRANSFER_TYPE_INTERRUPT:
        transfer_priv->request.TransferType = InterruptTransferType;
        break;
    }

    set_transfer_priv_handle(itransfer, priv->system_handle);

    if(IS_XFERIN(transfer))
        transferRes = usbdk_helper.ReadPipe(priv->redirector_handle, &transfer_priv->request, overlapped);
    else
        transferRes = usbdk_helper.WritePipe(priv->redirector_handle, &transfer_priv->request, overlapped);

    switch(transferRes) {
    case TransferSuccess:
        windows_force_sync_completion(itransfer, (ULONG)transfer_priv->request.Result.GenResult.BytesTransferred);
        break;
    case TransferSuccessAsync:
        break;
    case TransferFailure:
        usbi_err(TRANSFER_CTX(transfer), "ReadPipe/WritePipe failed: %s", windows_error_str(0));
        return LIBUSB_ERROR_IO;
    }

    return LIBUSB_SUCCESS;
}

static int usbdk_do_iso_transfer(struct usbi_transfer *itransfer) {
    struct libusb_transfer     *transfer      = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    struct usbdk_device_priv   *priv          = usbi_get_device_priv(transfer->dev_handle->dev);
    struct usbdk_transfer_priv *transfer_priv = get_usbdk_transfer_priv(itransfer);
    OVERLAPPED                 *overlapped    = get_transfer_priv_overlapped(itransfer);
    TransferResult              transferRes;
    int                         i;

    transfer_priv->request.Buffer                      = (PVOID64)transfer->buffer;
    transfer_priv->request.BufferLength                = transfer->length;
    transfer_priv->request.EndpointAddress             = transfer->endpoint;
    transfer_priv->request.TransferType                = IsochronousTransferType;
    transfer_priv->request.IsochronousPacketsArraySize = transfer->num_iso_packets;
    transfer_priv->IsochronousPacketsArray             = malloc(transfer->num_iso_packets * sizeof(ULONG64));
    transfer_priv->request.IsochronousPacketsArray     = (PVOID64)transfer_priv->IsochronousPacketsArray;
    if(!transfer_priv->IsochronousPacketsArray) {
        usbi_err(TRANSFER_CTX(transfer), "Allocation of IsochronousPacketsArray failed");
        return LIBUSB_ERROR_NO_MEM;
    }

    transfer_priv->IsochronousResultsArray                = malloc(transfer->num_iso_packets * sizeof(USB_DK_ISO_TRANSFER_RESULT));
    transfer_priv->request.Result.IsochronousResultsArray = (PVOID64)transfer_priv->IsochronousResultsArray;
    if(!transfer_priv->IsochronousResultsArray) {
        usbi_err(TRANSFER_CTX(transfer), "Allocation of isochronousResultsArray failed");
        return LIBUSB_ERROR_NO_MEM;
    }

    for(i = 0; i < transfer->num_iso_packets; i++)
        transfer_priv->IsochronousPacketsArray[i] = transfer->iso_packet_desc[i].length;

    set_transfer_priv_handle(itransfer, priv->system_handle);

    if(IS_XFERIN(transfer))
        transferRes = usbdk_helper.ReadPipe(priv->redirector_handle, &transfer_priv->request, overlapped);
    else
        transferRes = usbdk_helper.WritePipe(priv->redirector_handle, &transfer_priv->request, overlapped);

    switch(transferRes) {
    case TransferSuccess:
        windows_force_sync_completion(itransfer, (ULONG)transfer_priv->request.Result.GenResult.BytesTransferred);
        break;
    case TransferSuccessAsync:
        break;
    case TransferFailure:
        return LIBUSB_ERROR_IO;
    }

    return LIBUSB_SUCCESS;
}

static int usbdk_submit_transfer(struct usbi_transfer *itransfer) {
    struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);

    switch(transfer->type) {
    case LIBUSB_TRANSFER_TYPE_CONTROL:
        return usbdk_do_control_transfer(itransfer);
    case LIBUSB_TRANSFER_TYPE_BULK:
    case LIBUSB_TRANSFER_TYPE_INTERRUPT:
        if(IS_XFEROUT(transfer) && (transfer->flags & LIBUSB_TRANSFER_ADD_ZERO_PACKET))
            return LIBUSB_ERROR_NOT_SUPPORTED;  // TODO: Check whether we can support this in UsbDk
        return usbdk_do_bulk_transfer(itransfer);
    case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
        return usbdk_do_iso_transfer(itransfer);
    default:
        // Should not get here since windows_submit_transfer() validates
        // the transfer->type field
        usbi_err(TRANSFER_CTX(transfer), "unsupported endpoint type %d", transfer->type);
        return LIBUSB_ERROR_NOT_SUPPORTED;
    }
}

static enum libusb_transfer_status usbdk_copy_transfer_data(struct usbi_transfer *itransfer, DWORD length) {
    struct libusb_transfer     *transfer      = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    struct usbdk_transfer_priv *transfer_priv = get_usbdk_transfer_priv(itransfer);

    UNUSED(length);

    if(transfer->type == LIBUSB_TRANSFER_TYPE_ISOCHRONOUS) {
        ULONG64 i;

        for(i = 0; i < transfer_priv->request.IsochronousPacketsArraySize; i++) {
            struct libusb_iso_packet_descriptor *lib_desc = &transfer->iso_packet_desc[i];

            switch(transfer_priv->IsochronousResultsArray[i].TransferResult) {
            case STATUS_SUCCESS:
            case STATUS_CANCELLED:
            case STATUS_REQUEST_CANCELED:
                lib_desc->status = LIBUSB_TRANSFER_COMPLETED;  // == ERROR_SUCCESS
                break;
            default:
                lib_desc->status = LIBUSB_TRANSFER_ERROR;  // ERROR_UNKNOWN_EXCEPTION;
                break;
            }

            lib_desc->actual_length = (unsigned int)transfer_priv->IsochronousResultsArray[i].ActualLength;
        }
    }

    itransfer->transferred += (int)transfer_priv->request.Result.GenResult.BytesTransferred;
    return usbd_status_to_libusb_transfer_status((USBD_STATUS)transfer_priv->request.Result.GenResult.UsbdStatus);
}

const struct windows_backend usbdk_backend = {
    usbdk_init,
    usbdk_exit,
    usbdk_get_device_list,
    usbdk_open,
    usbdk_close,
    usbdk_get_active_config_descriptor,
    usbdk_get_config_descriptor,
    usbdk_get_config_descriptor_by_value,
    usbdk_get_configuration,
    usbdk_set_configuration,
    usbdk_claim_interface,
    usbdk_release_interface,
    usbdk_set_interface_altsetting,
    usbdk_clear_halt,
    usbdk_reset_device,
    usbdk_destroy_device,
    usbdk_submit_transfer,
    NULL, /* cancel_transfer */
    usbdk_clear_transfer_priv,
    usbdk_copy_transfer_data,
    NULL /* usbdk_set_option */,
};
