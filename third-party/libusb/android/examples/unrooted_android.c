/*
 *  libusb example program for reading out USB descriptors on unrooted Android
 *  (based on testlibusb.c)
 *
 *  Copyright 2020-2021 Peter Stoiber
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  Please contact the author if you need another license.
 *  This Repository is provided "as is", without warranties of any kind.
 */

/*
 * This example creates a shared object which can be accessed over JNA or JNI from Java or Kotlin in Android.
 * Hint: If you are using Android Studio, set the "Debug type" to "Java Only" to receive debug messages.
 */

/*
 * Usage:
 * First, you have to connect your USB device from the Java side.
 * Use the android.hardware.usb class to find the USB device, claim the interfaces, and open the usb_device_connection
 * Obtain the native File Descriptor --> usb_device_connection.getFileDescriptor()
 * Pass the received int value to the unrooted_usb_description method of this code (over JNA)
 */

/*
 * libusb can only be included in Android projects using NDK for now. (CMake is not supported at the moment)
 * Clone the libusb git repo into your Android project and include the Android.mk file in your build.gradle.
 */

/*
 Example JNA Approach:
    public interface unrooted_sample extends Library {
        public static final unrooted_sample INSTANCE = Native.load("unrooted_android", unrooted_sample.class);
        public int unrooted_usb_description (int fileDescriptor);
    }
    unrooted_sample.INSTANCE.unrooted_usb_description( usbDeviceConnection.getFileDescriptor());
 */

#include <jni.h>
#include <string.h>
#include "unrooted_android.h"
#include "libusb.h"
#include <android/log.h>
#define LOG_TAG "LibUsb"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

int verbose = 0;

static void print_endpoint_comp(const struct libusb_ss_endpoint_companion_descriptor *ep_comp) {
    LOGD("      USB 3.0 Endpoint Companion:\n");
    LOGD("        bMaxBurst:           %u\n", ep_comp->bMaxBurst);
    LOGD("        bmAttributes:        %02xh\n", ep_comp->bmAttributes);
    LOGD("        wBytesPerInterval:   %u\n", ep_comp->wBytesPerInterval);
}

static void print_endpoint(const struct libusb_endpoint_descriptor *endpoint) {
    int i, ret;

    LOGD("      Endpoint:\n");
    LOGD("        bEndpointAddress:    %02xh\n", endpoint->bEndpointAddress);
    LOGD("        bmAttributes:        %02xh\n", endpoint->bmAttributes);
    LOGD("        wMaxPacketSize:      %u\n", endpoint->wMaxPacketSize);
    LOGD("        bInterval:           %u\n", endpoint->bInterval);
    LOGD("        bRefresh:            %u\n", endpoint->bRefresh);
    LOGD("        bSynchAddress:       %u\n", endpoint->bSynchAddress);

    for(i = 0; i < endpoint->extra_length;) {
        if(LIBUSB_DT_SS_ENDPOINT_COMPANION == endpoint->extra[i + 1]) {
            struct libusb_ss_endpoint_companion_descriptor *ep_comp;

            ret = libusb_get_ss_endpoint_companion_descriptor(NULL, endpoint, &ep_comp);
            if(LIBUSB_SUCCESS != ret)
                continue;

            print_endpoint_comp(ep_comp);

            libusb_free_ss_endpoint_companion_descriptor(ep_comp);
        }

        i += endpoint->extra[i];
    }
}

static void print_altsetting(const struct libusb_interface_descriptor *interface) {
    uint8_t i;

    LOGD("    Interface:\n");
    LOGD("      bInterfaceNumber:      %u\n", interface->bInterfaceNumber);
    LOGD("      bAlternateSetting:     %u\n", interface->bAlternateSetting);
    LOGD("      bNumEndpoints:         %u\n", interface->bNumEndpoints);
    LOGD("      bInterfaceClass:       %u\n", interface->bInterfaceClass);
    LOGD("      bInterfaceSubClass:    %u\n", interface->bInterfaceSubClass);
    LOGD("      bInterfaceProtocol:    %u\n", interface->bInterfaceProtocol);
    LOGD("      iInterface:            %u\n", interface->iInterface);

    for(i = 0; i < interface->bNumEndpoints; i++)
        print_endpoint(&interface->endpoint[i]);
}

static void print_2_0_ext_cap(struct libusb_usb_2_0_extension_descriptor *usb_2_0_ext_cap) {
    LOGD("    USB 2.0 Extension Capabilities:\n");
    LOGD("      bDevCapabilityType:    %u\n", usb_2_0_ext_cap->bDevCapabilityType);
    LOGD("      bmAttributes:          %08xh\n", usb_2_0_ext_cap->bmAttributes);
}

static void print_ss_usb_cap(struct libusb_ss_usb_device_capability_descriptor *ss_usb_cap) {
    LOGD("    USB 3.0 Capabilities:\n");
    LOGD("      bDevCapabilityType:    %u\n", ss_usb_cap->bDevCapabilityType);
    LOGD("      bmAttributes:          %02xh\n", ss_usb_cap->bmAttributes);
    LOGD("      wSpeedSupported:       %u\n", ss_usb_cap->wSpeedSupported);
    LOGD("      bFunctionalitySupport: %u\n", ss_usb_cap->bFunctionalitySupport);
    LOGD("      bU1devExitLat:         %u\n", ss_usb_cap->bU1DevExitLat);
    LOGD("      bU2devExitLat:         %u\n", ss_usb_cap->bU2DevExitLat);
}

static void print_bos(libusb_device_handle *handle) {
    struct libusb_bos_descriptor *bos;
    uint8_t                       i;
    int                           ret;

    ret = libusb_get_bos_descriptor(handle, &bos);
    if(ret < 0)
        return;

    LOGD("  Binary Object Store (BOS):\n");
    LOGD("    wTotalLength:            %u\n", bos->wTotalLength);
    LOGD("    bNumDeviceCaps:          %u\n", bos->bNumDeviceCaps);

    for(i = 0; i < bos->bNumDeviceCaps; i++) {
        struct libusb_bos_dev_capability_descriptor *dev_cap = bos->dev_capability[i];

        if(dev_cap->bDevCapabilityType == LIBUSB_BT_USB_2_0_EXTENSION) {
            struct libusb_usb_2_0_extension_descriptor *usb_2_0_extension;

            ret = libusb_get_usb_2_0_extension_descriptor(NULL, dev_cap, &usb_2_0_extension);
            if(ret < 0)
                return;

            print_2_0_ext_cap(usb_2_0_extension);
            libusb_free_usb_2_0_extension_descriptor(usb_2_0_extension);
        }
        else if(dev_cap->bDevCapabilityType == LIBUSB_BT_SS_USB_DEVICE_CAPABILITY) {
            struct libusb_ss_usb_device_capability_descriptor *ss_dev_cap;

            ret = libusb_get_ss_usb_device_capability_descriptor(NULL, dev_cap, &ss_dev_cap);
            if(ret < 0)
                return;

            print_ss_usb_cap(ss_dev_cap);
            libusb_free_ss_usb_device_capability_descriptor(ss_dev_cap);
        }
    }

    libusb_free_bos_descriptor(bos);
}

static void print_interface(const struct libusb_interface *interface) {
    int i;

    for(i = 0; i < interface->num_altsetting; i++)
        print_altsetting(&interface->altsetting[i]);
}

static void print_configuration(struct libusb_config_descriptor *config) {
    uint8_t i;

    LOGD("  Configuration:\n");
    LOGD("    wTotalLength:            %u\n", config->wTotalLength);
    LOGD("    bNumInterfaces:          %u\n", config->bNumInterfaces);
    LOGD("    bConfigurationValue:     %u\n", config->bConfigurationValue);
    LOGD("    iConfiguration:          %u\n", config->iConfiguration);
    LOGD("    bmAttributes:            %02xh\n", config->bmAttributes);
    LOGD("    MaxPower:                %u\n", config->MaxPower);

    for(i = 0; i < config->bNumInterfaces; i++)
        print_interface(&config->interface[i]);
}

static void print_device(libusb_device *dev, libusb_device_handle *handle) {
    struct libusb_device_descriptor desc;
    unsigned char                   string[256];
    const char                     *speed;
    int                             ret;
    uint8_t                         i;

    switch(libusb_get_device_speed(dev)) {
    case LIBUSB_SPEED_LOW:
        speed = "1.5M";
        break;
    case LIBUSB_SPEED_FULL:
        speed = "12M";
        break;
    case LIBUSB_SPEED_HIGH:
        speed = "480M";
        break;
    case LIBUSB_SPEED_SUPER:
        speed = "5G";
        break;
    case LIBUSB_SPEED_SUPER_PLUS:
        speed = "10G";
        break;
    default:
        speed = "Unknown";
    }

    ret = libusb_get_device_descriptor(dev, &desc);
    if(ret < 0) {
        LOGD("failed to get device descriptor");
        return;
    }

    LOGD("Dev (bus %u, device %u): %04X - %04X speed: %s\n", libusb_get_bus_number(dev), libusb_get_device_address(dev), desc.idVendor, desc.idProduct, speed);

    if(!handle)
        libusb_open(dev, &handle);

    if(handle) {
        if(desc.iManufacturer) {
            ret = libusb_get_string_descriptor_ascii(handle, desc.iManufacturer, string, sizeof(string));
            if(ret > 0)
                LOGD("  Manufacturer:              %s\n", (char *)string);
        }

        if(desc.iProduct) {
            ret = libusb_get_string_descriptor_ascii(handle, desc.iProduct, string, sizeof(string));
            if(ret > 0)
                LOGD("  Product:                   %s\n", (char *)string);
        }

        if(desc.iSerialNumber && verbose) {
            ret = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, string, sizeof(string));
            if(ret > 0)
                LOGD("  Serial Number:             %s\n", (char *)string);
        }
    }

    if(verbose) {
        for(i = 0; i < desc.bNumConfigurations; i++) {
            struct libusb_config_descriptor *config;

            ret = libusb_get_config_descriptor(dev, i, &config);
            if(LIBUSB_SUCCESS != ret) {
                LOGD("  Couldn't retrieve descriptors\n");
                continue;
            }

            print_configuration(config);

            libusb_free_config_descriptor(config);
        }

        if(handle && desc.bcdUSB >= 0x0201)
            print_bos(handle);
    }

    if(handle)
        libusb_close(handle);
}

/* fileDescriptor = the native file descriptor obtained in Java and transferred to native over JNA, for example */
int unrooted_usb_description(int fileDescriptor) {
    libusb_context       *ctx  = NULL;
    libusb_device_handle *devh = NULL;
    int                   r    = 0;
    verbose                    = 1;
    r                          = libusb_set_option(NULL, LIBUSB_OPTION_NO_DEVICE_DISCOVERY, NULL);
    if(r != LIBUSB_SUCCESS) {
        LOGD("libusb_set_option failed: %d\n", r);
        return -1;
    }
    r = libusb_init(&ctx);
    if(r < 0) {
        LOGD("libusb_init failed: %d\n", r);
        return r;
    }
    r = libusb_wrap_sys_device(ctx, (intptr_t)fileDescriptor, &devh);
    if(r < 0) {
        LOGD("libusb_wrap_sys_device failed: %d\n", r);
        return r;
    }
    else if(devh == NULL) {
        LOGD("libusb_wrap_sys_device returned invalid handle\n");
        return r;
    }
    print_device(libusb_get_device(devh), devh);
    return r;
}
