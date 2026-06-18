// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#pragma once

#include <stdint.h>
#include <sstream>
#include <string>
#include <map>
#include <vector>

#define USB_DT_DEVICE 0x01
#define USB_DT_CONFIG 0x02
#define USB_DT_STRING 0x03
#define USB_DT_INTERFACE 0x04
#define USB_DT_ENDPOINT 0x05
#define USB_DT_DEVICE_QUALIFIER 0x06
#define USB_DT_OTHER_SPEED_CONFIG 0x07
#define USB_DT_INTERFACE_POWER 0x08
#define USB_DT_OTG 0x09
#define USB_DT_DEBUG 0x0a
#define USB_DT_INTERFACE_ASSOCIATION 0x0b
#define USB_DT_SECURITY 0x0c
#define USB_DT_KEY 0x0d
#define USB_DT_ENCRYPTION_TYPE 0x0e
#define USB_DT_BOS 0x0f
#define USB_DT_DEVICE_CAPABILITY 0x10
#define USB_DT_WIRELESS_ENDPOINT_COMP 0x11
#define USB_DT_WIRE_ADAPTER 0x21
#define USB_DT_RPIPE 0x22
#define USB_DT_CS_RADIO_CONTROL 0x23
#define USB_DT_PIPE_USAGE 0x24
#define USB_DT_SS_ENDPOINT_COMP 0x30
#define USB_DT_SSP_ISOC_ENDPOINT_COMP 0x31
#define USB_DT_CS_DEVICE (USB_TYPE_CLASS | USB_DT_DEVICE)
#define USB_DT_CS_CONFIG (USB_TYPE_CLASS | USB_DT_CONFIG)
#define USB_DT_CS_STRING (USB_TYPE_CLASS | USB_DT_STRING)
#define USB_DT_CS_INTERFACE (USB_TYPE_CLASS | USB_DT_INTERFACE)
#define USB_DT_CS_ENDPOINT (USB_TYPE_CLASS | USB_DT_ENDPOINT)

const uint16_t VID_INTEL_CAMERA = 0x8086;
const uint16_t VID_REALSENSE_CAMERA = 0x38E5;

namespace librealsense
{
    namespace platform
    {
        typedef enum _usb_status
        {
            RS2_USB_STATUS_SUCCESS = 0,
            RS2_USB_STATUS_IO = -1,
            RS2_USB_STATUS_INVALID_PARAM = -2,
            RS2_USB_STATUS_ACCESS = -3,
            RS2_USB_STATUS_NO_DEVICE = -4,
            RS2_USB_STATUS_NOT_FOUND = -5,
            RS2_USB_STATUS_BUSY = -6,
            RS2_USB_STATUS_TIMEOUT = -7,
            RS2_USB_STATUS_OVERFLOW = -8,
            RS2_USB_STATUS_PIPE = -9,
            RS2_USB_STATUS_INTERRUPTED = -10,
            RS2_USB_STATUS_NO_MEM = -11,
            RS2_USB_STATUS_NOT_SUPPORTED = -12,
            RS2_USB_STATUS_OTHER = -13
        } usb_status;

        typedef enum _endpoint_direction
        {
            RS2_USB_ENDPOINT_DIRECTION_WRITE = 0,
            RS2_USB_ENDPOINT_DIRECTION_READ = 0x80
        } endpoint_direction;

        typedef enum _endpoint_type
        {
            RS2_USB_ENDPOINT_CONTROL,
            RS2_USB_ENDPOINT_ISOCHRONOUS,
            RS2_USB_ENDPOINT_BULK,
            RS2_USB_ENDPOINT_INTERRUPT
        } endpoint_type;

        //https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/supported-usb-classes#microsoft-provided-usb-device-class-drivers
        typedef enum _usb_class
        {
            RS2_USB_CLASS_UNSPECIFIED = 0x00,
            RS2_USB_CLASS_AUDIO = 0x01,
            RS2_USB_CLASS_COM = 0x02,
            RS2_USB_CLASS_HID = 0x03,
            RS2_USB_CLASS_PID = 0x05,
            RS2_USB_CLASS_IMAGE = 0x06,
            RS2_USB_CLASS_PRINTER = 0x07,
            RS2_USB_CLASS_MASS_STORAGE = 0x08,
            RS2_USB_CLASS_HUB = 0x09,
            RS2_USB_CLASS_CDC_DATA = 0x0A,
            RS2_USB_CLASS_SMART_CARD = 0x0B,
            RS2_USB_CLASS_CONTENT_SECURITY = 0x0D,
            RS2_USB_CLASS_VIDEO = 0x0E,
            RS2_USB_CLASS_PHDC = 0x0F,
            RS2_USB_CLASS_AV = 0x10,
            RS2_USB_CLASS_BILLBOARD = 0x11,
            RS2_USB_CLASS_DIAGNOSTIC_DEVIE = 0xDC,
            RS2_USB_CLASS_WIRELESS_CONTROLLER = 0xE0,
            RS2_USB_CLASS_MISCELLANEOUS = 0xEF,
            RS2_USB_CLASS_APPLICATION_SPECIFIC = 0xFE,
            RS2_USB_CLASS_VENDOR_SPECIFIC = 0xFF
        } usb_class;

        typedef enum _usb_subclass
        {
            RS2_USB_SUBCLASS_VIDEO_CONTROL = 0x01,
            RS2_USB_SUBCLASS_VIDEO_STREAMING = 0x02
        } usb_subclass;

        // Binary-coded decimal represent the USB specification to which the UVC device complies
        enum usb_spec : uint16_t {
            usb_undefined = 0,
            usb1_type = 0x0100,
            usb1_1_type = 0x0110,
            usb2_type = 0x0200,
            usb2_01_type = 0x0201,
            usb2_1_type = 0x0210,
            usb3_type = 0x0300,
            usb3_1_type = 0x0310,
            usb3_2_type = 0x0320,
        };

        static const std::map< std::string, usb_spec > usb_name_to_spec = { { "Undefined", usb_undefined },
                                                                            { "1.0", usb1_type },
                                                                            { "1.1", usb1_1_type },
                                                                            { "2.0", usb2_type },
                                                                            { "2.01", usb2_01_type },
                                                                            { "2.1", usb2_1_type },
                                                                            { "3.0", usb3_type },
                                                                            { "3.1", usb3_1_type },
                                                                            { "3.2", usb3_2_type } };

        static const std::map<usb_spec, std::string> usb_spec_names = {
                { usb_undefined,"Undefined" },
                { usb1_type,    "1.0" },
                { usb1_1_type,  "1.1" },
                { usb2_type,    "2.0" },
                { usb2_01_type,  "2.01" },
                { usb2_1_type,  "2.1" },
                { usb3_type,    "3.0" },
                { usb3_1_type,  "3.1" },
                { usb3_2_type,  "3.2" }
        };

        struct usb_device_info
        {
            std::string id;

            uint16_t vid;
            uint16_t pid;
            uint16_t mi;
            std::string unique_id;
            std::string serial;
            usb_spec conn_spec;
            usb_class cls;

            operator std::string()
            {
                std::stringstream s;

                s << "vid- " << std::hex << vid <<
                    "\npid- " << std::hex << pid <<
                    "\nmi- " << mi <<
                    "\nsusb specification- " << std::hex << (uint16_t)conn_spec << std::dec <<
                    "\nunique_id- " << unique_id;

                return s.str();
            }
        };

        inline bool operator==( const usb_device_info & a, const usb_device_info & b )
        {
            return ( a.id == b.id ) && ( a.vid == b.vid ) && ( a.pid == b.pid ) && ( a.mi == b.mi )
                && ( a.unique_id == b.unique_id ) && ( a.conn_spec == b.conn_spec );
        }

        static std::map<usb_status,std::string> usb_status_to_string = 
        {
            {RS2_USB_STATUS_SUCCESS, "RS2_USB_STATUS_SUCCESS"},
            {RS2_USB_STATUS_IO, "RS2_USB_STATUS_IO"},
            {RS2_USB_STATUS_INVALID_PARAM, "RS2_USB_STATUS_INVALID_PARAM"},
            {RS2_USB_STATUS_ACCESS, "RS2_USB_STATUS_ACCESS"},
            {RS2_USB_STATUS_NO_DEVICE, "RS2_USB_STATUS_NO_DEVICE"},
            {RS2_USB_STATUS_NOT_FOUND, "RS2_USB_STATUS_NOT_FOUND"},
            {RS2_USB_STATUS_BUSY, "RS2_USB_STATUS_BUSY"},
            {RS2_USB_STATUS_TIMEOUT, "RS2_USB_STATUS_TIMEOUT"},
            {RS2_USB_STATUS_OVERFLOW, "RS2_USB_STATUS_OVERFLOW"},
            {RS2_USB_STATUS_PIPE, "RS2_USB_STATUS_PIPE"},
            {RS2_USB_STATUS_INTERRUPTED, "RS2_USB_STATUS_INTERRUPTED"},
            {RS2_USB_STATUS_NO_MEM, "RS2_USB_STATUS_NO_MEM"},
            {RS2_USB_STATUS_NOT_SUPPORTED, "RS2_USB_STATUS_NOT_SUPPORTED"},
            {RS2_USB_STATUS_OTHER, "RS2_USB_STATUS_OTHER"}
        };

        struct usb_config_descriptor {
            uint8_t bLength;
            uint8_t bDescriptorType;
            uint16_t wTotalLength;
            uint8_t bNumInterfaces;
            uint8_t bConfigurationValue;
            uint8_t iConfiguration;
            uint8_t bmAttributes;
            uint8_t bMaxPower;
        };

        struct usb_interface_descriptor {
            uint8_t bLength;
            uint8_t bDescriptorType;
            uint8_t bInterfaceNumber;
            uint8_t bAlternateSetting;
            uint8_t bNumEndpoints;
            uint8_t bInterfaceClass;
            uint8_t bInterfaceSubClass;
            uint8_t bInterfaceProtocol;
            uint8_t iInterface;
        };

        struct usb_descriptor
        {
            uint8_t length;
            uint8_t type;
            std::vector<uint8_t> data;
        };

#pragma pack(push, 1)
        struct dfu_header {
            uint32_t dwCRC;
            uint8_t  headerSignature[256];
            uint8_t  cssHeader[128];
            uint32_t magicNumber;
            uint32_t prefixLength;
            uint16_t bcdDFU;
            uint32_t bcdDevice;
            uint8_t  reserve[4];
            uint32_t imageSize;
            uint16_t nofBlocks;
        };
#pragma pack(pop)
    }
}
