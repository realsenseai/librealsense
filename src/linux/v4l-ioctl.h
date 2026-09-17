// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "types.h"

#include <linux/videodev2.h>

#include <cstdint>
#include <string>

// Metadata streaming nodes are available with kernels 4.16+
#ifdef V4L2_META_FMT_UVC
constexpr bool metadata_node = true;
#else
#pragma message ( "\nLibrealsense notification: V4L2_META_FMT_UVC was not defined, adding metadata constructs")

constexpr bool metadata_node = false;

// Providing missing parts from videodev2.h
// V4L2_META_FMT_UVC >> V4L2_CAP_META_CAPTURE is also defined, but the opposite does not hold
#define V4L2_META_FMT_UVC    v4l2_fourcc('U', 'V', 'C', 'H') /* UVC Payload Header */

#ifndef V4L2_CAP_META_CAPTURE
#define V4L2_CAP_META_CAPTURE    0x00800000  /* Specified in kernel header v4.16 */
#endif // V4L2_CAP_META_CAPTURE

#endif // V4L2_META_FMT_UVC

#ifndef V4L2_META_FMT_D4XX
#define V4L2_META_FMT_D4XX      v4l2_fourcc('D', '4', 'X', 'X') /* D400 Payload Header metadata */
#endif

#undef DEBUG_V4L
#ifdef DEBUG_V4L
#define LOG_DEBUG_V4L(...)   do { CLOG(DEBUG   ,LIBREALSENSE_ELPP_ID) << __VA_ARGS__; } while(false)
#else
#define LOG_DEBUG_V4L(...)
#endif //DEBUG_V4L

// Use local definition of buf type to resolve for kernel versions
constexpr auto LOCAL_V4L2_BUF_TYPE_META_CAPTURE = (v4l2_buf_type)(13);

#pragma pack(push, 1)
// The struct definition is identical to uvc_meta_buf defined uvcvideo.h/ kernel 4.16 headers, and is provided to allow for cross-kernel compilation
struct uvc_meta_buffer {
    __u64 ns;               // system timestamp of the payload in nanoseconds
    __u16 sof;              // USB Frame Number
    __u8 length;            // length of the payload metadata header
    __u8 flags;             // payload header flags
    __u8* buf;              //device-specific metadata payload data
};
#pragma pack(pop)

namespace librealsense
{
    namespace platform
    {
        // Low-level V4L2 ioctl wrapper (retries on EINTR). Used by low level types (buffer, kernel_buf_guard)
        int xioctl( int fh, unsigned long request, void * arg );

        // Open a V4L2 node. Retries while udev still has events queued for it. Negative on failure, errno set.
        int open_v4l_node( const std::string & name );

        // Retrieve device video capabilities to discriminate video capturing and metadata nodes
        v4l2_capability get_dev_capabilities( const std::string dev_name );

        std::string fourcc_to_string( uint32_t id );

        void stream_ctl_on( int fd, v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE );
        void stream_off( int fd, v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE );
        void req_io_buff( int fd, uint32_t count, std::string dev_name, v4l2_memory mem_type, v4l2_buf_type type );
    }  // namespace platform
}  // namespace librealsense
