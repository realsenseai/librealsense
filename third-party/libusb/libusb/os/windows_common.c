/*
 * windows backend for libusb 1.0
 * Copyright © 2009-2012 Pete Batard <pete@akeo.ie>
 * With contributions from Michael Plante, Orin Eman et al.
 * Parts of this code adapted from libusb-win32-v1 by Stephan Meyer
 * HID Reports IOCTLs inspired from HIDAPI by Alan Ott, Signal 11 Software
 * Hash table functions adapted from glibc, by Ulrich Drepper et al.
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

#include <libusb/config.h>

#include <stdio.h>

#include "libusbi.h"
#include "windows_common.h"

#define EPOCH_TIME UINT64_C(116444736000000000)  // 1970.01.01 00:00:000 in MS Filetime

#define STATUS_SUCCESS ((ULONG_PTR)0UL)

// Public
enum windows_version windows_version = WINDOWS_UNDEFINED;

// Global variables for init/exit
static unsigned int init_count;
static bool         usbdk_available;

/*
 * Converts a windows error to human readable string
 * uses retval as errorcode, or, if 0, use GetLastError()
 */
#if defined(ENABLE_LOGGING)
const char *windows_error_str(DWORD error_code) {
    static char err_string[256];

    DWORD size;
    int   len;

    if(error_code == 0)
        error_code = GetLastError();

    len = sprintf(err_string, "[%lu] ", ULONG_CAST(error_code));

    // Translate codes returned by SetupAPI. The ones we are dealing with are either
    // in 0x0000xxxx or 0xE000xxxx and can be distinguished from standard error codes.
    // See http://msdn.microsoft.com/en-us/library/windows/hardware/ff545011.aspx
    switch(error_code & 0xE0000000) {
    case 0:
        error_code = HRESULT_FROM_WIN32(error_code);  // Still leaves ERROR_SUCCESS unmodified
        break;
    case 0xE0000000:
        error_code = 0x80000000 | (FACILITY_SETUPAPI << 16) | (error_code & 0x0000FFFF);
        break;
    default:
        break;
    }

    size = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                          &err_string[len], sizeof(err_string) - len, NULL);
    if(size == 0) {
        DWORD format_error = GetLastError();
        if(format_error)
            snprintf(err_string, sizeof(err_string), "Windows error code %lu (FormatMessage error code %lu)", ULONG_CAST(error_code), ULONG_CAST(format_error));
        else
            snprintf(err_string, sizeof(err_string), "Unknown error code %lu", ULONG_CAST(error_code));
    }
    else {
        // Remove CRLF from end of message, if present
        size_t pos = len + size - 2;
        if(err_string[pos] == '\r')
            err_string[pos] = '\0';
    }

    return err_string;
}
#endif

/*
 * Dynamically loads a DLL from the Windows system directory.  Unlike the
 * LoadLibraryA() function, this function will not search through any
 * directories to try and find the library.
 */
HMODULE load_system_library(struct libusb_context *ctx, const char *name) {
    char  library_path[MAX_PATH];
    char *filename_start;
    UINT  length;

    length = GetSystemDirectoryA(library_path, sizeof(library_path));
    if((length == 0) || (length >= (UINT)sizeof(library_path))) {
        usbi_err(ctx, "program assertion failed - could not get system directory");
        return NULL;
    }

    filename_start = library_path + length;
    // Append '\' + name + ".dll" + NUL
    length += 1 + (UINT)strlen(name) + 4 + 1;
    if(length >= (UINT)sizeof(library_path)) {
        usbi_err(ctx, "program assertion failed - library path buffer overflow");
        return NULL;
    }

    sprintf(filename_start, "\\%s.dll", name);
    return LoadLibraryA(library_path);
}

/* Hash table functions - modified From glibc 2.3.2:
   [Aho,Sethi,Ullman] Compilers: Principles, Techniques and Tools, 1986
   [Knuth]            The Art of Computer Programming, part 3 (6.4)  */

#define HTAB_SIZE 1021UL  // *MUST* be a prime number!!

typedef struct htab_entry {
    unsigned long used;
    char         *str;
} htab_entry;

static htab_entry   *htab_table;
static usbi_mutex_t  htab_mutex;
static unsigned long htab_filled;

/* Before using the hash table we must allocate memory for it.
   We allocate one element more as the found prime number says.
   This is done for more effective indexing as explained in the
   comment for the hash function.  */
static bool htab_create(struct libusb_context *ctx) {
    if(htab_table != NULL) {
        usbi_err(ctx, "program assertion failed - hash table already allocated");
        return true;
    }

    // Create a mutex
    usbi_mutex_init(&htab_mutex);

    usbi_dbg(ctx, "using %lu entries hash table", HTAB_SIZE);
    htab_filled = 0;

    // allocate memory and zero out.
    htab_table = calloc(HTAB_SIZE + 1, sizeof(htab_entry));
    if(htab_table == NULL) {
        usbi_err(ctx, "could not allocate space for hash table");
        return false;
    }

    return true;
}

/* After using the hash table it has to be destroyed.  */
static void htab_destroy(void) {
    unsigned long i;

    if(htab_table == NULL)
        return;

    for(i = 0; i < HTAB_SIZE; i++)
        free(htab_table[i].str);

    safe_free(htab_table);

    usbi_mutex_destroy(&htab_mutex);
}

/* This is the search function. It uses double hashing with open addressing.
   We use a trick to speed up the lookup. The table is created with one
   more element available. This enables us to use the index zero special.
   This index will never be used because we store the first hash index in
   the field used where zero means not used. Every other value means used.
   The used field can be used as a first fast comparison for equality of
   the stored and the parameter value. This helps to prevent unnecessary
   expensive calls of strcmp.  */
unsigned long htab_hash(const char *str) {
    unsigned long hval, hval2;
    unsigned long idx;
    unsigned long r = 5381UL;
    int           c;
    const char   *sz = str;

    if(str == NULL)
        return 0;

    // Compute main hash value (algorithm suggested by Nokia)
    while((c = *sz++) != 0)
        r = ((r << 5) + r) + c;
    if(r == 0)
        ++r;

    // compute table hash: simply take the modulus
    hval = r % HTAB_SIZE;
    if(hval == 0)
        ++hval;

    // Try the first index
    idx = hval;

    // Mutually exclusive access (R/W lock would be better)
    usbi_mutex_lock(&htab_mutex);

    if(htab_table[idx].used) {
        if((htab_table[idx].used == hval) && (strcmp(str, htab_table[idx].str) == 0))
            goto out_unlock;  // existing hash

        usbi_dbg(NULL, "hash collision ('%s' vs '%s')", str, htab_table[idx].str);

        // Second hash function, as suggested in [Knuth]
        hval2 = 1UL + hval % (HTAB_SIZE - 2);

        do {
            // Because size is prime this guarantees to step through all available indexes
            if(idx <= hval2)
                idx = HTAB_SIZE + idx - hval2;
            else
                idx -= hval2;

            // If we visited all entries leave the loop unsuccessfully
            if(idx == hval)
                break;

            // If entry is found use it.
            if((htab_table[idx].used == hval) && (strcmp(str, htab_table[idx].str) == 0))
                goto out_unlock;
        } while(htab_table[idx].used);
    }

    // Not found => New entry

    // If the table is full return an error
    if(htab_filled >= HTAB_SIZE) {
        usbi_err(NULL, "hash table is full (%lu entries)", HTAB_SIZE);
        idx = 0UL;
        goto out_unlock;
    }

    htab_table[idx].str = _strdup(str);
    if(htab_table[idx].str == NULL) {
        usbi_err(NULL, "could not duplicate string for hash table");
        idx = 0UL;
        goto out_unlock;
    }

    htab_table[idx].used = hval;
    ++htab_filled;

out_unlock:
    usbi_mutex_unlock(&htab_mutex);

    return idx;
}

enum libusb_transfer_status usbd_status_to_libusb_transfer_status(USBD_STATUS status) {
    if(USBD_SUCCESS(status))
        return LIBUSB_TRANSFER_COMPLETED;

    switch(status) {
    case USBD_STATUS_TIMEOUT:
        return LIBUSB_TRANSFER_TIMED_OUT;
    case USBD_STATUS_CANCELED:
        return LIBUSB_TRANSFER_CANCELLED;
    case USBD_STATUS_ENDPOINT_HALTED:
    case USBD_STATUS_STALL_PID:
        return LIBUSB_TRANSFER_STALL;
    case USBD_STATUS_DEVICE_GONE:
        return LIBUSB_TRANSFER_NO_DEVICE;
    default:
        usbi_dbg(NULL, "USBD_STATUS 0x%08lx translated to LIBUSB_TRANSFER_ERROR", ULONG_CAST(status));
        return LIBUSB_TRANSFER_ERROR;
    }
}

/*
 * Make a transfer complete synchronously
 */
void windows_force_sync_completion(struct usbi_transfer *itransfer, ULONG size) {
    struct libusb_transfer       *transfer      = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    struct windows_context_priv  *priv          = usbi_get_context_priv(TRANSFER_CTX(transfer));
    struct windows_transfer_priv *transfer_priv = usbi_get_transfer_priv(itransfer);
    OVERLAPPED                   *overlapped    = &transfer_priv->overlapped;

    usbi_dbg(TRANSFER_CTX(transfer), "transfer %p, length %lu", transfer, ULONG_CAST(size));

    overlapped->Internal     = (ULONG_PTR)STATUS_SUCCESS;
    overlapped->InternalHigh = (ULONG_PTR)size;

    if(!PostQueuedCompletionStatus(priv->completion_port, (DWORD)size, (ULONG_PTR)transfer->dev_handle, overlapped))
        usbi_err(TRANSFER_CTX(transfer), "failed to post I/O completion: %s", windows_error_str(0));
}

/* Windows version detection */
static BOOL is_x64(void) {
    BOOL ret = FALSE;

    // Detect if we're running a 32 or 64 bit system
    if(sizeof(uintptr_t) < 8) {
        IsWow64Process(GetCurrentProcess(), &ret);
    }
    else {
        ret = TRUE;
    }

    return ret;
}

static enum windows_version get_windows_version(void) {
    enum windows_version winver;
    OSVERSIONINFOEXA     vi, vi2;
    unsigned             major, minor, version;
    ULONGLONG            major_equal, minor_equal;
    const char          *w, *arch;
    bool                 ws;

#ifndef ENABLE_LOGGING
    UNUSED(w);
    UNUSED(arch);
#endif
    memset(&vi, 0, sizeof(vi));
    vi.dwOSVersionInfoSize = sizeof(vi);
    if(!GetVersionExA((OSVERSIONINFOA *)&vi)) {
        memset(&vi, 0, sizeof(vi));
        vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
        if(!GetVersionExA((OSVERSIONINFOA *)&vi))
            return WINDOWS_UNDEFINED;
    }

    if(vi.dwPlatformId != VER_PLATFORM_WIN32_NT)
        return WINDOWS_UNDEFINED;

    if((vi.dwMajorVersion > 6) || ((vi.dwMajorVersion == 6) && (vi.dwMinorVersion >= 2))) {
        // Starting with Windows 8.1 Preview, GetVersionEx() does no longer report the actual OS version
        // See: http://msdn.microsoft.com/en-us/library/windows/desktop/dn302074.aspx
        // And starting with Windows 10 Preview 2, Windows enforces the use of the application/supportedOS
        // manifest in order for VerSetConditionMask() to report the ACTUAL OS major and minor...

        major_equal = VerSetConditionMask(0, VER_MAJORVERSION, VER_EQUAL);
        for(major = vi.dwMajorVersion; major <= 9; major++) {
            memset(&vi2, 0, sizeof(vi2));
            vi2.dwOSVersionInfoSize = sizeof(vi2);
            vi2.dwMajorVersion      = major;
            if(!VerifyVersionInfoA(&vi2, VER_MAJORVERSION, major_equal))
                continue;

            if(vi.dwMajorVersion < major) {
                vi.dwMajorVersion = major;
                vi.dwMinorVersion = 0;
            }

            minor_equal = VerSetConditionMask(0, VER_MINORVERSION, VER_EQUAL);
            for(minor = vi.dwMinorVersion; minor <= 9; minor++) {
                memset(&vi2, 0, sizeof(vi2));
                vi2.dwOSVersionInfoSize = sizeof(vi2);
                vi2.dwMinorVersion      = minor;
                if(!VerifyVersionInfoA(&vi2, VER_MINORVERSION, minor_equal))
                    continue;

                vi.dwMinorVersion = minor;
                break;
            }

            break;
        }
    }

    if((vi.dwMajorVersion > 0xf) || (vi.dwMinorVersion > 0xf))
        return WINDOWS_UNDEFINED;

    ws      = (vi.wProductType <= VER_NT_WORKSTATION);
    version = vi.dwMajorVersion << 4 | vi.dwMinorVersion;

    switch(version) {
    case 0x50:
        winver = WINDOWS_2000;
        w      = "2000";
        break;
    case 0x51:
        winver = WINDOWS_XP;
        w      = "XP";
        break;
    case 0x52:
        winver = WINDOWS_2003;
        w      = "2003";
        break;
    case 0x60:
        winver = WINDOWS_VISTA;
        w      = (ws ? "Vista" : "2008");
        break;
    case 0x61:
        winver = WINDOWS_7;
        w      = (ws ? "7" : "2008_R2");
        break;
    case 0x62:
        winver = WINDOWS_8;
        w      = (ws ? "8" : "2012");
        break;
    case 0x63:
        winver = WINDOWS_8_1;
        w      = (ws ? "8.1" : "2012_R2");
        break;
    case 0x64:  // Early Windows 10 Insider Previews and Windows Server 2017 Technical Preview 1 used version 6.4
    case 0xA0:
        winver = WINDOWS_10;
        w      = (ws ? "10" : "2016");
        if(vi.dwBuildNumber < 20000)
            break;
        // fallthrough
    case 0xB0:
        winver = WINDOWS_11;
        w      = (ws ? "11" : "2022");
        break;
    default:
        if(version < 0x50)
            return WINDOWS_UNDEFINED;
        winver = WINDOWS_12_OR_LATER;
        w      = "12 or later";
    }

    // We cannot tell if we are on 8, 10, or 11 without "app manifest"
    if(version == 0x62 && vi.dwBuildNumber == 9200)
        w = "8 (or later)";

    arch = is_x64() ? "64-bit" : "32-bit";

    if(vi.wServicePackMinor)
        usbi_dbg(NULL, "Windows %s SP%u.%u %s", w, vi.wServicePackMajor, vi.wServicePackMinor, arch);
    else if(vi.wServicePackMajor)
        usbi_dbg(NULL, "Windows %s SP%u %s", w, vi.wServicePackMajor, arch);
    else
        usbi_dbg(NULL, "Windows %s %s", w, arch);

    return winver;
}

static unsigned __stdcall windows_iocp_thread(void *arg) {
    struct libusb_context             *ctx  = arg;
    struct windows_context_priv       *priv = usbi_get_context_priv(ctx);
    HANDLE                             iocp = priv->completion_port;
    DWORD                              num_bytes;
    ULONG_PTR                          completion_key;
    OVERLAPPED                        *overlapped;
    struct libusb_device_handle       *dev_handle;
    struct libusb_device_handle       *opened_device_handle;
    struct windows_device_handle_priv *handle_priv;
    struct windows_transfer_priv      *transfer_priv;
    struct usbi_transfer              *itransfer;
    bool                               found;

    usbi_dbg(ctx, "I/O completion thread started");

    while(true) {
        overlapped = NULL;
        if(!GetQueuedCompletionStatus(iocp, &num_bytes, &completion_key, &overlapped, INFINITE) && (overlapped == NULL)) {
            usbi_err(ctx, "GetQueuedCompletionStatus failed: %s", windows_error_str(0));
            break;
        }

        if(overlapped == NULL) {
            // Signal to quit
            if(completion_key != (ULONG_PTR)ctx)
                usbi_err(ctx, "program assertion failed - overlapped is NULL");
            break;
        }

        // Find the transfer associated with the OVERLAPPED that just completed.
        // If we cannot find a match, the I/O operation originated from outside of libusb
        // (e.g. within libusbK) and we need to ignore it.
        dev_handle = (struct libusb_device_handle *)completion_key;

        found         = false;
        transfer_priv = NULL;

        // Issue 912: lock opened device handles in context to search the current device handle
        // to avoid accessing unallocated memory after device has been closed
        usbi_mutex_lock(&ctx->open_devs_lock);
        for_each_open_device(ctx, opened_device_handle) {
            if(dev_handle == opened_device_handle) {
                handle_priv = usbi_get_device_handle_priv(dev_handle);

                usbi_mutex_lock(&dev_handle->lock);
                list_for_each_entry(transfer_priv, &handle_priv->active_transfers, list, struct windows_transfer_priv) {
                    if(overlapped == &transfer_priv->overlapped) {
                        // This OVERLAPPED belongs to us, remove the transfer from the device handle's list
                        list_del(&transfer_priv->list);
                        found = true;
                        break;
                    }
                }
                usbi_mutex_unlock(&dev_handle->lock);
            }
        }
        usbi_mutex_unlock(&ctx->open_devs_lock);

        if(!found) {
            usbi_dbg(ctx, "ignoring overlapped %p for handle %p", overlapped, dev_handle);
            continue;
        }

        itransfer = (struct usbi_transfer *)((unsigned char *)transfer_priv + PTR_ALIGN(sizeof(*transfer_priv)));
        usbi_dbg(ctx, "transfer %p completed, length %lu", USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer), ULONG_CAST(num_bytes));
        usbi_signal_transfer_completion(itransfer);
    }

    usbi_dbg(ctx, "I/O completion thread exiting");

    return 0;
}

static int windows_init(struct libusb_context *ctx) {
    struct windows_context_priv *priv                = usbi_get_context_priv(ctx);
    bool                         winusb_backend_init = false;
    int                          r;

    // NB: concurrent usage supposes that init calls are equally balanced with
    // exit calls. If init is called more than exit, we will not exit properly
    if(++init_count == 1) {  // First init?
        windows_version = get_windows_version();
        if(windows_version == WINDOWS_UNDEFINED) {
            usbi_err(ctx, "failed to detect Windows version");
            r = LIBUSB_ERROR_NOT_SUPPORTED;
            goto init_exit;
        }
        else if(windows_version < WINDOWS_VISTA) {
            usbi_err(ctx, "Windows version is too old");
            r = LIBUSB_ERROR_NOT_SUPPORTED;
            goto init_exit;
        }

        if(!htab_create(ctx)) {
            r = LIBUSB_ERROR_NO_MEM;
            goto init_exit;
        }

        r = winusb_backend.init(ctx);
        if(r != LIBUSB_SUCCESS)
            goto init_exit;
        winusb_backend_init = true;

        r = usbdk_backend.init(ctx);
        if(r == LIBUSB_SUCCESS) {
            usbi_dbg(ctx, "UsbDk backend is available");
            usbdk_available = true;
        }
        else {
            usbi_info(ctx, "UsbDk backend is not available");
            // Do not report this as an error
        }
    }

    // By default, new contexts will use the WinUSB backend
    priv->backend = &winusb_backend;

    r = LIBUSB_ERROR_NO_MEM;

    // Use an I/O completion port to manage all transfers for this context
    priv->completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
    if(priv->completion_port == NULL) {
        usbi_err(ctx, "failed to create I/O completion port: %s", windows_error_str(0));
        goto init_exit;
    }

    // And a dedicated thread to wait for I/O completions
    priv->completion_port_thread = (HANDLE)_beginthreadex(NULL, 0, windows_iocp_thread, ctx, 0, NULL);
    if(priv->completion_port_thread == NULL) {
        usbi_err(ctx, "failed to create I/O completion port thread");
        CloseHandle(priv->completion_port);
        goto init_exit;
    }

    r = LIBUSB_SUCCESS;

init_exit:                                            // Holds semaphore here
    if((init_count == 1) && (r != LIBUSB_SUCCESS)) {  // First init failed?
        if(usbdk_available) {
            usbdk_backend.exit(ctx);
            usbdk_available = false;
        }
        if(winusb_backend_init)
            winusb_backend.exit(ctx);
        htab_destroy();
        --init_count;
    }

    return r;
}

static void windows_exit(struct libusb_context *ctx) {
    struct windows_context_priv *priv = usbi_get_context_priv(ctx);

    // A NULL completion status will indicate to the thread that it is time to exit
    if(!PostQueuedCompletionStatus(priv->completion_port, 0, (ULONG_PTR)ctx, NULL))
        usbi_err(ctx, "failed to post I/O completion: %s", windows_error_str(0));

    if(WaitForSingleObject(priv->completion_port_thread, INFINITE) == WAIT_FAILED)
        usbi_err(ctx, "failed to wait for I/O completion port thread: %s", windows_error_str(0));

    CloseHandle(priv->completion_port_thread);
    CloseHandle(priv->completion_port);

    // Only works if exits and inits are balanced exactly
    if(--init_count == 0) {  // Last exit
        if(usbdk_available) {
            usbdk_backend.exit(ctx);
            usbdk_available = false;
        }
        winusb_backend.exit(ctx);
        htab_destroy();
    }
}

static int windows_set_option(struct libusb_context *ctx, enum libusb_option option, va_list ap) {
    struct windows_context_priv *priv = usbi_get_context_priv(ctx);

    if(option == LIBUSB_OPTION_USE_USBDK) {
        if(!usbdk_available) {
            usbi_err(ctx, "UsbDk backend not available");
            return LIBUSB_ERROR_NOT_FOUND;
        }
        usbi_dbg(ctx, "switching context %p to use UsbDk backend", ctx);
        priv->backend = &usbdk_backend;
        return LIBUSB_SUCCESS;
    }

    if(priv->backend->set_option) {
        return priv->backend->set_option(ctx, option, ap);
    }

    return LIBUSB_ERROR_NOT_SUPPORTED;
}

static int windows_get_device_list(struct libusb_context *ctx, struct discovered_devs **discdevs) {
    struct windows_context_priv *priv = usbi_get_context_priv(ctx);
    return priv->backend->get_device_list(ctx, discdevs);
}

static int windows_open(struct libusb_device_handle *dev_handle) {
    struct windows_context_priv       *priv        = usbi_get_context_priv(HANDLE_CTX(dev_handle));
    struct windows_device_handle_priv *handle_priv = usbi_get_device_handle_priv(dev_handle);

    list_init(&handle_priv->active_transfers);
    return priv->backend->open(dev_handle);
}

static void windows_close(struct libusb_device_handle *dev_handle) {
    struct windows_context_priv *priv = usbi_get_context_priv(HANDLE_CTX(dev_handle));
    priv->backend->close(dev_handle);
}

static int windows_get_active_config_descriptor(struct libusb_device *dev, void *buffer, size_t len) {
    struct windows_context_priv *priv = usbi_get_context_priv(DEVICE_CTX(dev));
    return priv->backend->get_active_config_descriptor(dev, buffer, len);
}

static int windows_get_config_descriptor(struct libusb_device *dev, uint8_t config_index, void *buffer, size_t len) {
    struct windows_context_priv *priv = usbi_get_context_priv(DEVICE_CTX(dev));
    return priv->backend->get_config_descriptor(dev, config_index, buffer, len);
}

static int windows_get_config_descriptor_by_value(struct libusb_device *dev, uint8_t bConfigurationValue, void **buffer) {
    struct windows_context_priv *priv = usbi_get_context_priv(DEVICE_CTX(dev));
    return priv->backend->get_config_descriptor_by_value(dev, bConfigurationValue, buffer);
}

static int windows_get_configuration(struct libusb_device_handle *dev_handle, uint8_t *config) {
    struct windows_context_priv *priv = usbi_get_context_priv(HANDLE_CTX(dev_handle));
    return priv->backend->get_configuration(dev_handle, config);
}

static int windows_set_configuration(struct libusb_device_handle *dev_handle, int config) {
    struct windows_context_priv *priv = usbi_get_context_priv(HANDLE_CTX(dev_handle));
    if(config == -1)
        config = 0;
    return priv->backend->set_configuration(dev_handle, (uint8_t)config);
}

static int windows_claim_interface(struct libusb_device_handle *dev_handle, uint8_t interface_number) {
    struct windows_context_priv *priv = usbi_get_context_priv(HANDLE_CTX(dev_handle));
    return priv->backend->claim_interface(dev_handle, interface_number);
}

static int windows_release_interface(struct libusb_device_handle *dev_handle, uint8_t interface_number) {
    struct windows_context_priv *priv = usbi_get_context_priv(HANDLE_CTX(dev_handle));
    return priv->backend->release_interface(dev_handle, interface_number);
}

static int windows_set_interface_altsetting(struct libusb_device_handle *dev_handle, uint8_t interface_number, uint8_t altsetting) {
    struct windows_context_priv *priv = usbi_get_context_priv(HANDLE_CTX(dev_handle));
    return priv->backend->set_interface_altsetting(dev_handle, interface_number, altsetting);
}

static int windows_clear_halt(struct libusb_device_handle *dev_handle, unsigned char endpoint) {
    struct windows_context_priv *priv = usbi_get_context_priv(HANDLE_CTX(dev_handle));
    return priv->backend->clear_halt(dev_handle, endpoint);
}

static int windows_reset_device(struct libusb_device_handle *dev_handle) {
    struct windows_context_priv *priv = usbi_get_context_priv(HANDLE_CTX(dev_handle));
    return priv->backend->reset_device(dev_handle);
}

static void windows_destroy_device(struct libusb_device *dev) {
    struct windows_context_priv *priv = usbi_get_context_priv(DEVICE_CTX(dev));
    priv->backend->destroy_device(dev);
}

static int windows_submit_transfer(struct usbi_transfer *itransfer) {
    struct libusb_transfer            *transfer      = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    struct libusb_device_handle       *dev_handle    = transfer->dev_handle;
    struct libusb_context             *ctx           = HANDLE_CTX(dev_handle);
    struct windows_context_priv       *priv          = usbi_get_context_priv(ctx);
    struct windows_device_handle_priv *handle_priv   = usbi_get_device_handle_priv(dev_handle);
    struct windows_transfer_priv      *transfer_priv = usbi_get_transfer_priv(itransfer);
    int                                r;

    switch(transfer->type) {
    case LIBUSB_TRANSFER_TYPE_CONTROL:
    case LIBUSB_TRANSFER_TYPE_BULK:
    case LIBUSB_TRANSFER_TYPE_INTERRUPT:
    case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
        break;
    case LIBUSB_TRANSFER_TYPE_BULK_STREAM:
        usbi_warn(ctx, "bulk stream transfers are not yet supported on this platform");
        return LIBUSB_ERROR_NOT_SUPPORTED;
    default:
        usbi_err(ctx, "unknown endpoint type %d", transfer->type);
        return LIBUSB_ERROR_INVALID_PARAM;
    }

    if(transfer_priv->handle != NULL) {
        usbi_err(ctx, "program assertion failed - transfer HANDLE is not NULL");
        transfer_priv->handle = NULL;
    }

    // Add transfer to the device handle's list
    usbi_mutex_lock(&dev_handle->lock);
    list_add_tail(&transfer_priv->list, &handle_priv->active_transfers);
    usbi_mutex_unlock(&dev_handle->lock);

    r = priv->backend->submit_transfer(itransfer);
    if(r != LIBUSB_SUCCESS) {
        // Remove the unsuccessful transfer from the device handle's list
        usbi_mutex_lock(&dev_handle->lock);
        list_del(&transfer_priv->list);
        usbi_mutex_unlock(&dev_handle->lock);

        // Always call the backend's clear_transfer_priv() function on failure
        priv->backend->clear_transfer_priv(itransfer);
        transfer_priv->handle = NULL;
        return r;
    }

    // The backend should set the HANDLE used for each submitted transfer
    // by calling set_transfer_priv_handle()
    if(transfer_priv->handle == NULL)
        usbi_err(ctx, "program assertion failed - transfer HANDLE is NULL after transfer was submitted");

    return r;
}

static int windows_cancel_transfer(struct usbi_transfer *itransfer) {
    struct windows_context_priv  *priv          = usbi_get_context_priv(ITRANSFER_CTX(itransfer));
    struct windows_transfer_priv *transfer_priv = usbi_get_transfer_priv(itransfer);

    // Try CancelIoEx() on the transfer
    // If that fails, fall back to the backend's cancel_transfer()
    // function if it is available
    if(CancelIoEx(transfer_priv->handle, &transfer_priv->overlapped))
        return LIBUSB_SUCCESS;
    else if(GetLastError() == ERROR_NOT_FOUND)
        return LIBUSB_ERROR_NOT_FOUND;

    if(priv->backend->cancel_transfer)
        return priv->backend->cancel_transfer(itransfer);

    usbi_warn(ITRANSFER_CTX(itransfer), "cancellation not supported for this transfer's driver");
    return LIBUSB_ERROR_NOT_SUPPORTED;
}

static int windows_handle_transfer_completion(struct usbi_transfer *itransfer) {
    struct libusb_context        *ctx           = ITRANSFER_CTX(itransfer);
    struct windows_context_priv  *priv          = usbi_get_context_priv(ctx);
    const struct windows_backend *backend       = priv->backend;
    struct windows_transfer_priv *transfer_priv = usbi_get_transfer_priv(itransfer);
    enum libusb_transfer_status   status, istatus;
    DWORD                         result, bytes_transferred;

    if(GetOverlappedResult(transfer_priv->handle, &transfer_priv->overlapped, &bytes_transferred, FALSE))
        result = NO_ERROR;
    else
        result = GetLastError();

    usbi_dbg(ctx, "handling transfer %p completion with errcode %lu, length %lu", USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer), ULONG_CAST(result),
             ULONG_CAST(bytes_transferred));

    switch(result) {
    case NO_ERROR:
        status = backend->copy_transfer_data(itransfer, bytes_transferred);
        break;
    case ERROR_GEN_FAILURE:
        usbi_dbg(ctx, "detected endpoint stall");
        status = LIBUSB_TRANSFER_STALL;
        break;
    case ERROR_SEM_TIMEOUT:
        usbi_dbg(ctx, "detected semaphore timeout");
        status = LIBUSB_TRANSFER_TIMED_OUT;
        break;
    case ERROR_OPERATION_ABORTED:
        istatus = backend->copy_transfer_data(itransfer, bytes_transferred);
        if(istatus != LIBUSB_TRANSFER_COMPLETED)
            usbi_dbg(ctx, "failed to copy partial data in aborted operation: %d", (int)istatus);

        usbi_dbg(ctx, "detected operation aborted");
        status = LIBUSB_TRANSFER_CANCELLED;
        break;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_DEVICE_NOT_CONNECTED:
    case ERROR_NO_SUCH_DEVICE:
        usbi_dbg(ctx, "detected device removed");
        status = LIBUSB_TRANSFER_NO_DEVICE;
        break;
    default:
        usbi_err(ctx, "detected I/O error %lu: %s", ULONG_CAST(result), windows_error_str(result));
        status = LIBUSB_TRANSFER_ERROR;
        break;
    }

    transfer_priv->handle = NULL;

    // Backend-specific cleanup
    backend->clear_transfer_priv(itransfer);

    if(status == LIBUSB_TRANSFER_CANCELLED)
        return usbi_handle_transfer_cancellation(itransfer);
    else
        return usbi_handle_transfer_completion(itransfer, status);
}

#ifndef HAVE_CLOCK_GETTIME
void usbi_get_monotonic_time(struct timespec *tp) {
    static LONG     hires_counter_init;
    static uint64_t hires_ticks_to_ps;
    static uint64_t hires_frequency;
    LARGE_INTEGER   hires_counter;

    if(InterlockedExchange(&hires_counter_init, 1L) == 0L) {
        LARGE_INTEGER li_frequency;

        // Microsoft says that the QueryPerformanceFrequency() and
        // QueryPerformanceCounter() functions always succeed on XP and later
        QueryPerformanceFrequency(&li_frequency);

        // The hires frequency can go as high as 4 GHz, so we'll use a conversion
        // to picoseconds to compute the tv_nsecs part
        hires_frequency   = li_frequency.QuadPart;
        hires_ticks_to_ps = UINT64_C(1000000000000) / hires_frequency;
    }

    QueryPerformanceCounter(&hires_counter);
    tp->tv_sec  = (long)(hires_counter.QuadPart / hires_frequency);
    tp->tv_nsec = (long)(((hires_counter.QuadPart % hires_frequency) * hires_ticks_to_ps) / UINT64_C(1000));
}
#endif

// NB: MSVC6 does not support named initializers.
const struct usbi_os_backend usbi_backend = {
    "Windows",
    USBI_CAP_HAS_HID_ACCESS,
    windows_init,
    windows_exit,
    windows_set_option,
    windows_get_device_list,
    NULL, /* hotplug_poll */
    NULL, /* wrap_sys_device */
    windows_open,
    windows_close,
    windows_get_active_config_descriptor,
    windows_get_config_descriptor,
    windows_get_config_descriptor_by_value,
    windows_get_configuration,
    windows_set_configuration,
    windows_claim_interface,
    windows_release_interface,
    windows_set_interface_altsetting,
    windows_clear_halt,
    windows_reset_device,
    NULL, /* alloc_streams */
    NULL, /* free_streams */
    NULL, /* dev_mem_alloc */
    NULL, /* dev_mem_free */
    NULL, /* kernel_driver_active */
    NULL, /* detach_kernel_driver */
    NULL, /* attach_kernel_driver */
    windows_destroy_device,
    windows_submit_transfer,
    windows_cancel_transfer,
    NULL, /* clear_transfer_priv */
    NULL, /* handle_events */
    windows_handle_transfer_completion,
    sizeof(struct windows_context_priv),
    sizeof(union windows_device_priv),
    sizeof(struct windows_device_handle_priv),
    sizeof(struct windows_transfer_priv),
};
