/* License: Apache 2.0. See LICENSE file in root directory.
   Copyright(c) 2026 RealSense, Inc. All Rights Reserved. */

/** \file rs_decimation_filter_dpp.h
* \brief
* Cast-target struct for RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP (see rs_composite_option.h).
* Same shared HKR DPP wire layout (see rs_hdrd_control.h): dpp_header + 8 int32 param slots,
* 2 used (38 bytes, little-endian, pack(1)); ctl_id = 0x0001.
*/

#ifndef LIBREALSENSE_RS2_DECIMATION_FILTER_DPP_H
#define LIBREALSENSE_RS2_DECIMATION_FILTER_DPP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rs_dpp_header.h"
#include <stdint.h>

#pragma pack(push, 1)

/** The entire 38-byte Decimation Filter DPP payload, header included. */
typedef struct rs2_decimation_filter_dpp_config
{
    dpp_header header;

    int32_t enabled;    /**< 0 = Off, 1 = On. Default 0. Read/write only while Depth/IR is
                         * idle - the device rejects a SET once streaming starts. */
    int32_t magnitude;  /**< Downscale factor. Currently fixed at 2 (min=max=default=2) - the
                         * FW-defined range this build must not assume stays a single value. */

    int32_t reserved[6];   /**< MUST be zero on SET */
} rs2_decimation_filter_dpp_config;

#pragma pack(pop)

/* Fails to compile if padding/field changes push this off the documented 38-byte wire size. */
typedef char rs2_decimation_filter_dpp_config_wire_size_check[ ( sizeof( rs2_decimation_filter_dpp_config ) == 38 ) ? 1 : -1 ];

/** {min, max, step, def} bounds for rs2_decimation_filter_dpp_config, as returned by
* rs2_get_composite_option_range(RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP). Read-only - each
* bound already carries its own header.version, so this wrapper has no version field of its own. */
typedef struct rs2_decimation_filter_dpp_range
{
    rs2_decimation_filter_dpp_config min;
    rs2_decimation_filter_dpp_config max;
    rs2_decimation_filter_dpp_config step;
    rs2_decimation_filter_dpp_config def;
} rs2_decimation_filter_dpp_range;

#ifdef __cplusplus
}
#endif
#endif  // LIBREALSENSE_RS2_DECIMATION_FILTER_DPP_H
