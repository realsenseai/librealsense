/*
 * Windows backend common header for libusb 1.0
 *
 * This file brings together header code common between
 * the desktop Windows backends.
 * Copyright © 2012-2013 RealVNC Ltd.
 * Copyright © 2009-2012 Pete Batard <pete@akeo.ie>
 * Copyright © 2014-2020 Chris Dickens <christopher.a.dickens@gmail.com>
 * With contributions from Michael Plante, Orin Eman et al.
 * Parts of this code adapted from libusb-win32-v1 by Stephan Meyer
 * Major code testing contribution by Xiaofan Chen
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

#ifndef LIBUSB_WINDOWS_COMMON_H
#define LIBUSB_WINDOWS_COMMON_H

#include <stdbool.h>

/*
 * Workaround for the mess that exists with the DWORD and ULONG types.
 * Visual Studio unconditionally defines these types as 'unsigned long'
 * and a long is always 32-bits, even on 64-bit builds. GCC on the other
 * hand varies the width of a long, matching it to the build. To make
 * matters worse, the platform headers for these GCC builds define a
 * DWORD/ULONG to be 'unsigned long' on 32-bit builds and 'unsigned int'
 * on 64-bit builds. This creates a great deal of warnings for compilers
 * that support printf format checking since it will never actually be
 * an unsigned long.
 */
#if defined(_MSC_VER)
#define ULONG_CAST(x) (x)
#else
#define ULONG_CAST(x) ((unsigned long)(x))
#endif

#if defined(__CYGWIN__)
#define _stricmp strcasecmp
#define _strdup strdup
// _beginthreadex is MSVCRT => unavailable for cygwin. Fallback to using CreateThread
#define _beginthreadex(a, b, c, d, e, f) CreateThread(a, b, (LPTHREAD_START_ROUTINE)c, d, e, (LPDWORD)f)
#else
#include <process.h>
#endif

#define safe_free(p)         \
    do {                     \
        if(p != NULL) {      \
            free((void *)p); \
            p = NULL;        \
        }                    \
    } while(0)

/*
 * API macros - leveraged from libusb-win32 1.x
 */
#define DLL_STRINGIFY(s) #s

/*
 * Macros for handling DLL themselves
 */
#define DLL_HANDLE_NAME(name) __dll_##name##_handle

#define DLL_DECLARE_HANDLE(name) static HMODULE DLL_HANDLE_NAME(name)

#define DLL_GET_HANDLE(ctx, name)                                              \
    do {                                                                       \
        DLL_HANDLE_NAME(name) = load_system_library(ctx, DLL_STRINGIFY(name)); \
        if(!DLL_HANDLE_NAME(name))                                             \
            return false;                                                      \
    } while(0)

#define DLL_FREE_HANDLE(name)                   \
    do {                                        \
        if(DLL_HANDLE_NAME(name)) {             \
            FreeLibrary(DLL_HANDLE_NAME(name)); \
            DLL_HANDLE_NAME(name) = NULL;       \
        }                                       \
    } while(0)

/*
 * Macros for handling functions within a DLL
 */
#define DLL_FUNC_NAME(name) __dll_##name##_func_t

#define DLL_DECLARE_FUNC_PREFIXNAME(api, ret, prefixname, name, args) \
    typedef ret(api *DLL_FUNC_NAME(name)) args;                       \
    static DLL_FUNC_NAME(name) prefixname

#define DLL_DECLARE_FUNC(api, ret, name, args) DLL_DECLARE_FUNC_PREFIXNAME(api, ret, name, name, args)
#define DLL_DECLARE_FUNC_PREFIXED(api, ret, prefix, name, args) DLL_DECLARE_FUNC_PREFIXNAME(api, ret, prefix##name, name, args)

#define DLL_LOAD_FUNC_PREFIXNAME(dll, prefixname, name, ret_on_failure)                            \
    do {                                                                                           \
        HMODULE h  = DLL_HANDLE_NAME(dll);                                                         \
        prefixname = (DLL_FUNC_NAME(name))GetProcAddress(h, DLL_STRINGIFY(name));                  \
        if(prefixname)                                                                             \
            break;                                                                                 \
        prefixname = (DLL_FUNC_NAME(name))GetProcAddress(h, DLL_STRINGIFY(name) DLL_STRINGIFY(A)); \
        if(prefixname)                                                                             \
            break;                                                                                 \
        prefixname = (DLL_FUNC_NAME(name))GetProcAddress(h, DLL_STRINGIFY(name) DLL_STRINGIFY(W)); \
        if(prefixname)                                                                             \
            break;                                                                                 \
        if(ret_on_failure)                                                                         \
            return false;                                                                          \
    } while(0)

#define DLL_LOAD_FUNC(dll, name, ret_on_failure) DLL_LOAD_FUNC_PREFIXNAME(dll, name, name, ret_on_failure)
#define DLL_LOAD_FUNC_PREFIXED(dll, prefix, name, ret_on_failure) DLL_LOAD_FUNC_PREFIXNAME(dll, prefix##name, name, ret_on_failure)

// https://msdn.microsoft.com/en-us/library/windows/hardware/ff539136(v=vs.85).aspx
#if !defined(USBD_SUCCESS)
typedef LONG USBD_STATUS;

#define USBD_SUCCESS(Status) ((USBD_STATUS)(Status) >= 0)

#define USBD_STATUS_STALL_PID ((USBD_STATUS)0xC0000004L)
#define USBD_STATUS_ENDPOINT_HALTED ((USBD_STATUS)0xC0000030L)
#define USBD_STATUS_TIMEOUT ((USBD_STATUS)0xC0006000L)
#define USBD_STATUS_DEVICE_GONE ((USBD_STATUS)0xC0007000L)
#define USBD_STATUS_CANCELED ((USBD_STATUS)0xC0010000L)
#endif

// error code added with Windows SDK 10.0.18362
#ifndef ERROR_NO_SUCH_DEVICE
#define ERROR_NO_SUCH_DEVICE 433L
#endif

/* Windows versions */
enum windows_version {
    WINDOWS_UNDEFINED,
    WINDOWS_2000,
    WINDOWS_XP,
    WINDOWS_2003,  // Also XP x64
    WINDOWS_VISTA,
    WINDOWS_7,
    WINDOWS_8,
    WINDOWS_8_1,
    WINDOWS_10,
    WINDOWS_11,
    WINDOWS_12_OR_LATER
};

extern enum windows_version windows_version;

#include <pshpack1.h>

typedef struct USB_DEVICE_DESCRIPTOR {
    UCHAR  bLength;
    UCHAR  bDescriptorType;
    USHORT bcdUSB;
    UCHAR  bDeviceClass;
    UCHAR  bDeviceSubClass;
    UCHAR  bDeviceProtocol;
    UCHAR  bMaxPacketSize0;
    USHORT idVendor;
    USHORT idProduct;
    USHORT bcdDevice;
    UCHAR  iManufacturer;
    UCHAR  iProduct;
    UCHAR  iSerialNumber;
    UCHAR  bNumConfigurations;
} USB_DEVICE_DESCRIPTOR, *PUSB_DEVICE_DESCRIPTOR;

typedef struct USB_CONFIGURATION_DESCRIPTOR {
    UCHAR  bLength;
    UCHAR  bDescriptorType;
    USHORT wTotalLength;
    UCHAR  bNumInterfaces;
    UCHAR  bConfigurationValue;
    UCHAR  iConfiguration;
    UCHAR  bmAttributes;
    UCHAR  MaxPower;
} USB_CONFIGURATION_DESCRIPTOR, *PUSB_CONFIGURATION_DESCRIPTOR;

#include <poppack.h>

#define MAX_DEVICE_ID_LEN 200

typedef struct USB_DK_DEVICE_ID {
    WCHAR DeviceID[MAX_DEVICE_ID_LEN];
    WCHAR InstanceID[MAX_DEVICE_ID_LEN];
} USB_DK_DEVICE_ID, *PUSB_DK_DEVICE_ID;

typedef struct USB_DK_DEVICE_INFO {
    USB_DK_DEVICE_ID      ID;
    ULONG64               FilterID;
    ULONG64               Port;
    ULONG64               Speed;
    USB_DEVICE_DESCRIPTOR DeviceDescriptor;
} USB_DK_DEVICE_INFO, *PUSB_DK_DEVICE_INFO;

typedef struct USB_DK_ISO_TRANSFER_RESULT {
    ULONG64 ActualLength;
    ULONG64 TransferResult;
} USB_DK_ISO_TRANSFER_RESULT, *PUSB_DK_ISO_TRANSFER_RESULT;

typedef struct USB_DK_GEN_TRANSFER_RESULT {
    ULONG64 BytesTransferred;
    ULONG64 UsbdStatus;  // USBD_STATUS code
} USB_DK_GEN_TRANSFER_RESULT, *PUSB_DK_GEN_TRANSFER_RESULT;

typedef struct USB_DK_TRANSFER_RESULT {
    USB_DK_GEN_TRANSFER_RESULT GenResult;
    PVOID64                    IsochronousResultsArray;  // array of USB_DK_ISO_TRANSFER_RESULT
} USB_DK_TRANSFER_RESULT, *PUSB_DK_TRANSFER_RESULT;

typedef struct USB_DK_TRANSFER_REQUEST {
    ULONG64                EndpointAddress;
    PVOID64                Buffer;
    ULONG64                BufferLength;
    ULONG64                TransferType;
    ULONG64                IsochronousPacketsArraySize;
    PVOID64                IsochronousPacketsArray;
    USB_DK_TRANSFER_RESULT Result;
} USB_DK_TRANSFER_REQUEST, *PUSB_DK_TRANSFER_REQUEST;

struct usbdk_device_priv {
    USB_DK_DEVICE_ID               ID;
    PUSB_CONFIGURATION_DESCRIPTOR *config_descriptors;
    HANDLE                         redirector_handle;
    HANDLE                         system_handle;
    uint8_t                        active_configuration;
};

struct winusb_device_priv {
    bool                                  initialized;
    bool                                  root_hub;
    uint8_t                               active_config;
    uint8_t                               depth;  // distance to HCD
    const struct windows_usb_api_backend *apib;
    char                                 *dev_id;
    char                                 *path;     // device interface path
    int                                   sub_api;  // for WinUSB-like APIs
    struct {
        char                                 *path;  // each interface needs a device interface path,
        const struct windows_usb_api_backend *apib;  // an API backend (multiple drivers support),
        int                                   sub_api;
        int8_t                                nb_endpoints;  // and a set of endpoint addresses (USB_MAXENDPOINTS)
        uint8_t                              *endpoint;
        int                                   current_altsetting;
        bool                                  restricted_functionality;  // indicates if the interface functionality is restricted
                                                                         // by Windows (eg. HID keyboards or mice cannot do R/W)
        uint8_t num_associated_interfaces;                               // If non-zero, the interface is part of a grouped
                                                                         // set of associated interfaces (defined by an IAD)
                                                                         // and this is the number of interfaces within the
                                                                         // associated group (bInterfaceCount in IAD).
        uint8_t first_associated_interface;                              // For associated interfaces, this is the index of
                                                                         // the first interface (bFirstInterface in IAD) for
                                                                         // the grouped set of associated interfaces.
    } usb_interface[USB_MAXINTERFACES];
    struct hid_device_priv        *hid;
    PUSB_CONFIGURATION_DESCRIPTOR *config_descriptor;  // list of pointers to the cached config descriptors
    GUID                           class_guid;         // checked for change during re-enumeration
};

struct usbdk_device_handle_priv {
    // Not currently used
    char dummy;
};

enum WINUSB_ZLP { WINUSB_ZLP_UNSET = 0, WINUSB_ZLP_OFF = 1, WINUSB_ZLP_ON = 2 };

struct winusb_device_handle_priv {
    int active_interface;
    struct {
        HANDLE  dev_handle;             // WinUSB needs an extra handle for the file
        HANDLE  api_handle;             // used by the API to communicate with the device
        uint8_t zlp[USB_MAXENDPOINTS];  // Current per-endpoint SHORT_PACKET_TERMINATE status (enum WINUSB_ZLP)
    } interface_handle[USB_MAXINTERFACES];
    int autoclaim_count[USB_MAXINTERFACES];  // For auto-release
};

struct usbdk_transfer_priv {
    USB_DK_TRANSFER_REQUEST     request;
    PULONG64                    IsochronousPacketsArray;
    PUSB_DK_ISO_TRANSFER_RESULT IsochronousResultsArray;
};

struct winusb_transfer_priv {
    uint8_t interface_number;

    uint8_t *hid_buffer;  // 1 byte extended data buffer, required for HID
    uint8_t *hid_dest;    // transfer buffer destination, required for HID
    size_t   hid_expected_size;

    // For isochronous transfers with LibUSBk driver:
    void *iso_context;

    // For isochronous transfers with Microsoft WinUSB driver:
    void *isoch_buffer_handle;                // The isoch_buffer_handle to free at the end of the transfer
    BOOL  iso_break_stream;                   // Whether the isoch. stream was to be continued in the last call of libusb_submit_transfer.
                                              // As we this structure is zeroed out upon initialization, we need to use inverse logic here.
    libusb_transfer_cb_fn iso_user_callback;  // Original transfer callback of the user. Might be used for isochronous transfers.
};

struct windows_backend {
    int (*init)(struct libusb_context *ctx);
    void (*exit)(struct libusb_context *ctx);
    int (*get_device_list)(struct libusb_context *ctx, struct discovered_devs **discdevs);
    int (*open)(struct libusb_device_handle *dev_handle);
    void (*close)(struct libusb_device_handle *dev_handle);
    int (*get_active_config_descriptor)(struct libusb_device *device, void *buffer, size_t len);
    int (*get_config_descriptor)(struct libusb_device *device, uint8_t config_index, void *buffer, size_t len);
    int (*get_config_descriptor_by_value)(struct libusb_device *device, uint8_t bConfigurationValue, void **buffer);
    int (*get_configuration)(struct libusb_device_handle *dev_handle, uint8_t *config);
    int (*set_configuration)(struct libusb_device_handle *dev_handle, uint8_t config);
    int (*claim_interface)(struct libusb_device_handle *dev_handle, uint8_t interface_number);
    int (*release_interface)(struct libusb_device_handle *dev_handle, uint8_t interface_number);
    int (*set_interface_altsetting)(struct libusb_device_handle *dev_handle, uint8_t interface_number, uint8_t altsetting);
    int (*clear_halt)(struct libusb_device_handle *dev_handle, unsigned char endpoint);
    int (*reset_device)(struct libusb_device_handle *dev_handle);
    void (*destroy_device)(struct libusb_device *dev);
    int (*submit_transfer)(struct usbi_transfer *itransfer);
    int (*cancel_transfer)(struct usbi_transfer *itransfer);
    void (*clear_transfer_priv)(struct usbi_transfer *itransfer);
    enum libusb_transfer_status (*copy_transfer_data)(struct usbi_transfer *itransfer, DWORD length);
    int (*set_option)(struct libusb_context *ctx, enum libusb_option option, va_list args);
};

struct windows_context_priv {
    const struct windows_backend *backend;
    HANDLE                        completion_port;
    HANDLE                        completion_port_thread;
};

union windows_device_priv {
    struct usbdk_device_priv  usbdk_priv;
    struct winusb_device_priv winusb_priv;
};

struct windows_device_handle_priv {
    struct list_head active_transfers;
    union {
        struct usbdk_device_handle_priv  usbdk_priv;
        struct winusb_device_handle_priv winusb_priv;
    };
};

struct windows_transfer_priv {
    OVERLAPPED       overlapped;
    HANDLE           handle;
    struct list_head list;
    union {
        struct usbdk_transfer_priv  usbdk_priv;
        struct winusb_transfer_priv winusb_priv;
    };
};

static inline struct usbdk_device_handle_priv *get_usbdk_device_handle_priv(struct libusb_device_handle *dev_handle) {
    struct windows_device_handle_priv *handle_priv = usbi_get_device_handle_priv(dev_handle);
    return &handle_priv->usbdk_priv;
}

static inline struct winusb_device_handle_priv *get_winusb_device_handle_priv(struct libusb_device_handle *dev_handle) {
    struct windows_device_handle_priv *handle_priv = usbi_get_device_handle_priv(dev_handle);
    return &handle_priv->winusb_priv;
}

static inline OVERLAPPED *get_transfer_priv_overlapped(struct usbi_transfer *itransfer) {
    struct windows_transfer_priv *transfer_priv = usbi_get_transfer_priv(itransfer);
    return &transfer_priv->overlapped;
}

static inline void set_transfer_priv_handle(struct usbi_transfer *itransfer, HANDLE handle) {
    struct windows_transfer_priv *transfer_priv = usbi_get_transfer_priv(itransfer);
    transfer_priv->handle                       = handle;
}

static inline struct usbdk_transfer_priv *get_usbdk_transfer_priv(struct usbi_transfer *itransfer) {
    struct windows_transfer_priv *transfer_priv = usbi_get_transfer_priv(itransfer);
    return &transfer_priv->usbdk_priv;
}

static inline struct winusb_transfer_priv *get_winusb_transfer_priv(struct usbi_transfer *itransfer) {
    struct windows_transfer_priv *transfer_priv = usbi_get_transfer_priv(itransfer);
    return &transfer_priv->winusb_priv;
}

extern const struct windows_backend usbdk_backend;
extern const struct windows_backend winusb_backend;

HMODULE                     load_system_library(struct libusb_context *ctx, const char *name);
unsigned long               htab_hash(const char *str);
enum libusb_transfer_status usbd_status_to_libusb_transfer_status(USBD_STATUS status);
void                        windows_force_sync_completion(struct usbi_transfer *itransfer, ULONG size);

#if defined(ENABLE_LOGGING)
const char *windows_error_str(DWORD error_code);
#endif

#endif
