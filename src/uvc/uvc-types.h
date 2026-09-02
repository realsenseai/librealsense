// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 RealSense, Inc. All Rights Reserved.

#pragma once

#include "types.h"
#include "../small-heap.h"
#include <src/platform/frame-object.h>
#include <src/core/frame-data-allocator.h>

#include <vector>
#include <unordered_map>

/** Converts an unaligned one-byte integer into an int8 */
#define B_TO_BYTE(p) ((int8_t)(p)[0])

/** Converts an unaligned two-byte little-endian integer into an int16 */
#define SW_TO_SHORT(p) ((uint8_t)(p)[0] | \
                       ((int8_t)(p)[1] << 8))

/** Converts an unaligned four-byte little-endian integer into an int32 */
#define DW_TO_INT(p) ((uint8_t)(p)[0] | \
                     ((uint8_t)(p)[1] << 8) | \
                     ((uint8_t)(p)[2] << 16) | \
                     ((int8_t)(p)[3] << 24))

/** Converts an unaligned eight-byte little-endian integer into an int64 */
#define QW_TO_QUAD(p) (((uint64_t)(p)[0]) | \
                      (((uint64_t)(p)[1]) << 8) | \
                      (((uint64_t)(p)[2]) << 16) | \
                      (((uint64_t)(p)[3]) << 24) | \
                      (((uint64_t)(p)[4]) << 32) | \
                      (((uint64_t)(p)[5]) << 40) | \
                      (((uint64_t)(p)[6]) << 48) | \
                      (((int64_t)(p)[7]) << 56))


/** Converts an int16 into an unaligned two-byte little-endian integer */
#define SHORT_TO_SW(s, p) \
  (p)[0] = (uint8_t)(s); \
  (p)[1] = (uint8_t)((s) >> 8);

/** Converts an int32 into an unaligned four-byte little-endian integer */
#define INT_TO_DW(i, p) \
  (p)[0] = (uint8_t)(i); \
  (p)[1] = (uint8_t)((i) >> 8); \
  (p)[2] = (uint8_t)((i) >> 16); \
  (p)[3] = (uint8_t)((i) >> 24);

/** Converts an int64 into an unaligned eight-byte little-endian integer */
#define QUAD_TO_QW(i, p) \
  (p)[0] = (uint8_t)(i); \
  (p)[1] = (uint8_t)((i) >> 8); \
  (p)[2] = (uint8_t)((i) >> 16); \
  (p)[3] = (uint8_t)((i) >> 24); \
  (p)[4] = (uint8_t)((i) >> 32); \
  (p)[5] = (uint8_t)((i) >> 40); \
  (p)[6] = (uint8_t)((i) >> 48); \
  (p)[7] = (uint8_t)((i) >> 56); \

// convert to standard fourcc codes
const std::unordered_map<uint32_t, uint32_t> fourcc_map = {
        { 0x59382020, 0x47524559 },    /* 'GREY' from 'Y8  ' */
        { 0x52573130, 0x70524141 },    /* 'pRAA' from 'RW10'.*/
        { 0x32000000, 0x47524559 },    /* 'GREY' from 'L8  ' */
        { 0x50000000, 0x5a313620 },    /* 'Z16'  from 'D16 ' */
        { 0x52415738, 0x47524559 },    /* 'GREY' from 'RAW8' */
        { 0x52573136, 0x42595232 }     /* 'RW16' from 'BYR2' */
};

/** Color coding of stream, transport-independent
* @ingroup streaming
*/
enum uvc_frame_format {
    UVC_FRAME_FORMAT_UNKNOWN = 0,
    /** Any supported format */
    UVC_FRAME_FORMAT_ANY = 0,
    UVC_FRAME_FORMAT_UNCOMPRESSED,
    UVC_FRAME_FORMAT_COMPRESSED,
    /** YUYV/YUV2/YUV422: YUV encoding with one luminance value per pixel and
    * one UV (chrominance) pair for every two pixels.
    */
    UVC_FRAME_FORMAT_YUYV,
    UVC_FRAME_FORMAT_UYVY,
    /** 24-bit RGB */
    UVC_FRAME_FORMAT_RGB,
    UVC_FRAME_FORMAT_BGR,
    /** Motion-JPEG (or JPEG) encoded images */
    UVC_FRAME_FORMAT_MJPEG,
    UVC_FRAME_FORMAT_GRAY8,
    UVC_FRAME_FORMAT_BY8,
    /** Number of formats understood */
    UVC_FRAME_FORMAT_COUNT,
};

/** VideoControl interface descriptor subtype (A.5) */
enum uvc_vc_desc_subtype {
    UVC_VC_DESCRIPTOR_UNDEFINED = 0x00,
    UVC_VC_HEADER = 0x01,
    UVC_VC_INPUT_TERMINAL = 0x02,
    UVC_VC_OUTPUT_TERMINAL = 0x03,
    UVC_VC_SELECTOR_UNIT = 0x04,
    UVC_VC_PROCESSING_UNIT = 0x05,
    UVC_VC_EXTENSION_UNIT = 0x06
};

/** VideoStreaming interface descriptor subtype (A.6) */
enum uvc_vs_desc_subtype {
    UVC_VS_UNDEFINED = 0x00,
    UVC_VS_INPUT_HEADER = 0x01,
    UVC_VS_OUTPUT_HEADER = 0x02,
    UVC_VS_STILL_IMAGE_FRAME = 0x03,
    UVC_VS_FORMAT_UNCOMPRESSED = 0x04,
    UVC_VS_FRAME_UNCOMPRESSED = 0x05,
    UVC_VS_FORMAT_MJPEG = 0x06,
    UVC_VS_FRAME_MJPEG = 0x07,
    UVC_VS_FORMAT_MPEG2TS = 0x0a,
    UVC_VS_FORMAT_DV = 0x0c,
    UVC_VS_COLORFORMAT = 0x0d,
    UVC_VS_FORMAT_FRAME_BASED = 0x10,
    UVC_VS_FRAME_FRAME_BASED = 0x11,
    UVC_VS_FORMAT_STREAM_BASED = 0x12
};

/** UVC request code (A.8) */
enum uvc_req_code {
    UVC_RC_UNDEFINED = 0x00,
    UVC_SET_CUR = 0x01,
    UVC_GET_CUR = 0x81,
    UVC_GET_MIN = 0x82,
    UVC_GET_MAX = 0x83,
    UVC_GET_RES = 0x84,
    UVC_GET_LEN = 0x85,
    UVC_GET_INFO = 0x86,
    UVC_GET_DEF = 0x87,
    UVC_REQ_TYPE_GET = 0xa1,
    UVC_REQ_TYPE_SET = 0x21
};

/** VideoControl interface control selector (A.9.1) */
enum uvc_vc_ctrl_selector {
    UVC_VC_CONTROL_UNDEFINED = 0x00,
    UVC_VC_VIDEO_POWER_MODE_CONTROL = 0x01,
    UVC_VC_REQUEST_ERROR_CODE_CONTROL = 0x02
};

/** Camera terminal control selector (A.9.4) */
enum uvc_ct_ctrl_selector {
    UVC_CT_CONTROL_UNDEFINED = 0x00,
    UVC_CT_SCANNING_MODE_CONTROL = 0x01,
    UVC_CT_AE_MODE_CONTROL = 0x02,
    UVC_CT_AE_PRIORITY_CONTROL = 0x03,
    UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL = 0x04,
    UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL = 0x05,
    UVC_CT_FOCUS_ABSOLUTE_CONTROL = 0x06,
    UVC_CT_FOCUS_RELATIVE_CONTROL = 0x07,
    UVC_CT_FOCUS_AUTO_CONTROL = 0x08,
    UVC_CT_IRIS_ABSOLUTE_CONTROL = 0x09,
    UVC_CT_IRIS_RELATIVE_CONTROL = 0x0a,
    UVC_CT_ZOOM_ABSOLUTE_CONTROL = 0x0b,
    UVC_CT_ZOOM_RELATIVE_CONTROL = 0x0c,
    UVC_CT_PANTILT_ABSOLUTE_CONTROL = 0x0d,
    UVC_CT_PANTILT_RELATIVE_CONTROL = 0x0e,
    UVC_CT_ROLL_ABSOLUTE_CONTROL = 0x0f,
    UVC_CT_ROLL_RELATIVE_CONTROL = 0x10,
    UVC_CT_PRIVACY_CONTROL = 0x11,
    UVC_CT_FOCUS_SIMPLE_CONTROL = 0x12,
    UVC_CT_DIGITAL_WINDOW_CONTROL = 0x13,
    UVC_CT_REGION_OF_INTEREST_CONTROL = 0x14
};

/** Processing unit control selector (A.9.5) */
enum uvc_pu_ctrl_selector {
    UVC_PU_CONTROL_UNDEFINED = 0x00,
    UVC_PU_BACKLIGHT_COMPENSATION_CONTROL = 0x01,
    UVC_PU_BRIGHTNESS_CONTROL = 0x02,
    UVC_PU_CONTRAST_CONTROL = 0x03,
    UVC_PU_GAIN_CONTROL = 0x04,
    UVC_PU_POWER_LINE_FREQUENCY_CONTROL = 0x05,
    UVC_PU_HUE_CONTROL = 0x06,
    UVC_PU_SATURATION_CONTROL = 0x07,
    UVC_PU_SHARPNESS_CONTROL = 0x08,
    UVC_PU_GAMMA_CONTROL = 0x09,
    UVC_PU_WHITE_BALANCE_TEMPERATURE_CONTROL = 0x0a,
    UVC_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL = 0x0b,
    UVC_PU_WHITE_BALANCE_COMPONENT_CONTROL = 0x0c,
    UVC_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL = 0x0d,
    UVC_PU_DIGITAL_MULTIPLIER_CONTROL = 0x0e,
    UVC_PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL = 0x0f,
    UVC_PU_HUE_AUTO_CONTROL = 0x10,
    UVC_PU_ANALOG_VIDEO_STANDARD_CONTROL = 0x11,
    UVC_PU_ANALOG_LOCK_STATUS_CONTROL = 0x12,
    UVC_PU_CONTRAST_AUTO_CONTROL = 0x13
};

/** USB terminal type (B.1) */
enum uvc_term_type {
    UVC_TT_VENDOR_SPECIFIC = 0x0100,
    UVC_TT_STREAMING = 0x0101
};

/** Input terminal type (B.2) */
enum uvc_it_type {
    UVC_ITT_VENDOR_SPECIFIC = 0x0200,
    UVC_ITT_CAMERA = 0x0201,
    UVC_ITT_MEDIA_TRANSPORT_INPUT = 0x0202
};

/** Output terminal type (B.3) */
enum uvc_ot_type {
    UVC_OTT_VENDOR_SPECIFIC = 0x0300,
    UVC_OTT_DISPLAY = 0x0301,
    UVC_OTT_MEDIA_TRANSPORT_OUTPUT = 0x0302
};

/** External terminal type (B.4) */
enum uvc_et_type {
    UVC_EXTERNAL_VENDOR_SPECIFIC = 0x0400,
    UVC_COMPOSITE_CONNECTOR = 0x0401,
    UVC_SVIDEO_CONNECTOR = 0x0402,
    UVC_COMPONENT_CONNECTOR = 0x0403
};

enum uvc_status_class {
    UVC_STATUS_CLASS_CONTROL = 0x10,
    UVC_STATUS_CLASS_CONTROL_CAMERA = 0x11,
    UVC_STATUS_CLASS_CONTROL_PROCESSING = 0x12,
};

enum uvc_status_attribute {
    UVC_STATUS_ATTRIBUTE_VALUE_CHANGE = 0x00,
    UVC_STATUS_ATTRIBUTE_INFO_CHANGE = 0x01,
    UVC_STATUS_ATTRIBUTE_FAILURE_CHANGE = 0x02,
    UVC_STATUS_ATTRIBUTE_UNKNOWN = 0xff
};

enum uvc_vs_ctrl_selector {
    UVC_VS_CONTROL_UNDEFINED = 0x00,
    UVC_VS_PROBE_CONTROL = 0x01,
    UVC_VS_COMMIT_CONTROL = 0x02,
    UVC_VS_STILL_PROBE_CONTROL = 0x03,
    UVC_VS_STILL_COMMIT_CONTROL = 0x04,
    UVC_VS_STILL_IMAGE_TRIGGER_CONTROL = 0x05,
    UVC_VS_STREAM_ERROR_CODE_CONTROL = 0x06,
    UVC_VS_GENERATE_KEY_FRAME_CONTROL = 0x07,
    UVC_VS_UPDATE_FRAME_SEGMENT_CONTROL = 0x08,
    UVC_VS_SYNC_DELAY_CONTROL = 0x09
};

typedef struct uvc_device_descriptor {
    /** Vendor ID */
    uint16_t idVendor;
    /** Product ID */
    uint16_t idProduct;
    /** UVC compliance level, e.g. 0x0100 (1.0), 0x0110 */
    uint16_t bcdUVC;
    /** Serial number (null if unavailable) */
    const char *serialNumber;
    /** Device-reported manufacturer name (or null) */
    const char *manufacturer;
    /** Device-reporter product name (or null) */
    const char *product;
} uvc_device_descriptor_t;

typedef struct uvc_input_terminal {
    /** Index of the terminal within the device */
    uint8_t bTerminalID;
    /** Type of terminal (e.g., camera) */
    enum uvc_it_type wTerminalType;
    uint16_t wObjectiveFocalLengthMin;
    uint16_t wObjectiveFocalLengthMax;
    uint16_t wOcularFocalLength;
    /** Camera controls (meaning of bits given in {uvc_ct_ctrl_selector}) */
    uint64_t bmControls;
} uvc_input_terminal_t;


/** Represents post-capture processing functions */
typedef struct uvc_processing_unit {
    /** Index of the processing unit within the device */
    uint8_t bUnitID;
    /** Index of the terminal from which the device accepts images */
    uint8_t bSourceID;
    /** Processing controls (meaning of bits given in {uvc_pu_ctrl_selector}) */
    uint64_t bmControls;
} uvc_processing_unit_t;

/** Represents selector unit to connect other units */
typedef struct uvc_selector_unit {
    /** Index of the selector unit within the device */
    uint8_t bUnitID;
} uvc_selector_unit_t;

/** Custom processing or camera-control functions */
typedef struct uvc_extension_unit {
    /** Index of the extension unit within the device */
    uint8_t bUnitID;
    /** GUID identifying the extension unit */
    uint8_t guidExtensionCode[16];
    /** Bitmap of available controls (manufacturer-dependent) */
    uint64_t bmControls;
} uvc_extension_unit_t;

typedef struct uvc_stream_ctrl {
    uint16_t bmHint;
    uint8_t bFormatIndex;
    uint8_t bFrameIndex;
    uint32_t dwFrameInterval;
    uint16_t wKeyFrameRate;
    uint16_t wPFrameRate;
    uint16_t wCompQuality;
    uint16_t wCompWindowSize;
    uint16_t wDelay;
    uint32_t dwMaxVideoFrameSize;
    uint32_t dwMaxPayloadTransferSize;
    uint32_t dwClockFrequency;
    uint8_t bmFramingInfo;
    uint8_t bPreferredVersion;
    uint8_t bMinVersion;
    uint8_t bMaxVersion;
    uint8_t bInterfaceNumber;
    uint8_t bUsage;
    uint8_t bBitDepthLuma;
    uint8_t bmSettings;
    uint8_t bMaxNumberOfRefFramesPlus1;
    uint16_t bmRateControlModes;
    uint64_t bmLayoutPerStream;
} uvc_stream_ctrl_t;

typedef struct uvc_format {
    /** Width of image in pixels */
    uint32_t width;
    /** Height of image in pixels */
    uint32_t height;
    /** fourcc format */
    uint32_t fourcc;
    /** frame per second */
    uint32_t fps;
    /** Interface number */
    uint8_t interfaceNumber;
} uvc_format_t;

typedef struct uvc_frame_desc {
    /** Type of frame, such as JPEG frame or uncompressed frme */
    uvc_vs_desc_subtype bDescriptorSubtype;
    /** Index of the frame within the list of specs available for this format */
    uint8_t bFrameIndex;
    uint8_t bmCapabilities;
    /** Image width */
    uint16_t wWidth;
    /** Image height */
    uint16_t wHeight;
    /** Bitrate of corresponding stream at minimal frame rate */
    uint32_t dwMinBitRate;
    /** Bitrate of corresponding stream at maximal frame rate */
    uint32_t dwMaxBitRate;
    /** Maximum number of bytes for a video frame */
    uint32_t dwMaxVideoFrameBufferSize;
    /** Default frame interval (in 100ns units) */
    uint32_t dwDefaultFrameInterval;
    /** Minimum frame interval for continuous mode (100ns units) */
    uint32_t dwMinFrameInterval;
    /** Maximum frame interval for continuous mode (100ns units) */
    uint32_t dwMaxFrameInterval;
    /** Granularity of frame interval range for continuous mode (100ns) */
    uint32_t dwFrameIntervalStep;
    /** Frame intervals */
    uint8_t bFrameIntervalType;
    /** number of bytes per line */
    uint32_t dwBytesPerLine;
    /** Available frame rates, zero-terminated (in 100ns units) */
    std::vector<uint32_t> intervals;
} uvc_frame_desc_t;

/** Format descriptor
*
* A "format" determines a stream's image type (e.g., raw YUYV or JPEG)
* and includes many "frame" configurations.
*/
typedef struct uvc_format_desc {
    /** Type of image stream, such as JPEG or uncompressed. */
    enum uvc_vs_desc_subtype bDescriptorSubtype;
    /** Identifier of this format within the VS interface's format list */
    uint8_t bFormatIndex;
    uint8_t bNumFrameDescriptors;
    /** Format specifier */
    union {
        uint8_t guidFormat[16];
        uint8_t fourccFormat[4];
    };
    /** Format-specific data */
    union {
        /** BPP for uncompressed stream */
        uint8_t bBitsPerPixel;
        /** Flags for JPEG stream */
        uint8_t bmFlags;
    };
    /** Default {uvc_frame_desc} to choose given this format */
    uint8_t bDefaultFrameIndex;
    uint8_t bAspectRatioX;
    uint8_t bAspectRatioY;
    uint8_t bmInterlaceFlags;
    uint8_t bCopyProtect;
    uint8_t bVariableSize;
    /** Available frame specifications for this format */
    std::vector<uvc_frame_desc_t> frame_descs;
} uvc_format_desc_t;

template <typename T>
T as(const std::vector<uint8_t>& data, size_t index)
{
    T rv = 0;
    for(int i = 0; i < sizeof(T); i++)
    {
        rv += data[index + i] << (i*8);
    }
    return rv;
}

struct backend_frame;

typedef librealsense::small_heap<backend_frame, 10> backend_frames_archive;

struct backend_frame {
    backend_frame() {}

    // We don't want the frames to be overwritten at any point
    // (deallocate resets item to "default" that we want to avoid)
    backend_frame(const backend_frame &other) {}

    backend_frame(backend_frame &&other) {}

    backend_frame &operator=(const backend_frame &other) { return *this; }

    // A zero-copy captured frame points straight at this buffer, so it must be GPU-visible or the
    // GPU consumer pays a host-to-device upload (V4L2 does the same via rs_v4l2_zc_register).
    std::vector< uint8_t, librealsense::frame_data_allocator > pixels;
    librealsense::platform::frame_object fo;
    backend_frames_archive *owner; // Keep pointer to owner for light-deleter
};

typedef void(*cleanup_ptr)(backend_frame *);

// Unique_ptr is used as the simplest RAII, with static deleter
typedef std::unique_ptr<backend_frame, cleanup_ptr> backend_frame_ptr;
typedef single_consumer_queue<backend_frame_ptr> backend_frames_queue;
