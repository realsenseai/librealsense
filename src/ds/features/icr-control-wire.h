// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <librealsense2/h/rs_dpp_header.h>
#include <librealsense2/h/rs_icr_control.h>

#include <cstdint>
#include <cstring>
#include <vector>


namespace librealsense {

// HKR Improved Close Range wire payload: dpp_header + 8 int32 param slots, 7 used (38 bytes, little-endian,
// pack(1)), ctl_id 0x0008. Private to the SDK so FW payload changes do not change the public rs2_icr_control.
#pragma pack( push, 1 )
struct icr_control_wire
{
    dpp_header header;
    int32_t enable;
    int32_t filter_type;
    int32_t downscale_ratio;
    int32_t shift_mode;
    int32_t shift_pixels;
    int32_t threshold_mode;
    int32_t threshold_mm;
    int32_t reserved[1];  // MUST be zero on SET
};
#pragma pack( pop )

static_assert( sizeof( icr_control_wire ) == 38, "ICR wire payload must be 38 bytes" );

constexpr uint16_t icr_control_ctl_id = 0x0008;
constexpr uint8_t icr_control_param_count = 7;

inline rs2_icr_control icr_from_wire( const icr_control_wire & w )
{
    rs2_icr_control c;
    c.enable = w.enable;
    c.filter_type = w.filter_type;
    c.downscale_ratio = w.downscale_ratio;
    c.shift_mode = w.shift_mode;
    c.shift_pixels = w.shift_pixels;
    c.threshold_mode = w.threshold_mode;
    c.threshold_mm = w.threshold_mm;
    return c;
}

// `header` is the one FW reported (or a default one), carried back as is; reserved is always zero
inline icr_control_wire icr_to_wire( const rs2_icr_control & c, const dpp_header & header )
{
    icr_control_wire w;
    std::memset( &w, 0, sizeof( w ) );
    w.header = header;
    w.enable = c.enable;
    w.filter_type = c.filter_type;
    w.downscale_ratio = c.downscale_ratio;
    w.shift_mode = c.shift_mode;
    w.shift_pixels = c.shift_pixels;
    w.threshold_mode = c.threshold_mode;
    w.threshold_mm = c.threshold_mm;
    return w;
}

inline dpp_header icr_default_header()
{
    dpp_header h;
    std::memset( &h, 0, sizeof( h ) );
    h.version = DPP_HEADER_CURRENT_VERSION;
    h.ctl_id = icr_control_ctl_id;
    h.param_count = icr_control_param_count;
    return h;
}

}  // namespace librealsense
