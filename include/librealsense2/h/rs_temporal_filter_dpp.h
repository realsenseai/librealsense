/* License: Apache 2.0. See LICENSE file in root directory.
   Copyright(c) 2026 RealSense, Inc. All Rights Reserved. */

/** \file rs_temporal_filter_dpp.h
* \brief
* Cast-target struct for RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP (see rs_composite_option.h).
* Same shared HKR DPP wire layout (see rs_hdrd_control.h): dpp_header + 8 32-bit param slots,
* 4 used (38 bytes, little-endian, pack(1)); ctl_id = 0x0002.
*/

#ifndef LIBREALSENSE_RS2_TEMPORAL_FILTER_DPP_H
#define LIBREALSENSE_RS2_TEMPORAL_FILTER_DPP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rs_dpp_header.h"
#include <stdint.h>

#pragma pack(push, 1)

/** The entire 38-byte Temporal Filter DPP payload, header included. */
typedef struct rs2_temporal_filter_dpp_config
{
    dpp_header header;

    int32_t enabled;             /**< 0 = Off, 1 = On. Default 0 */
    float smooth_alpha;          /**< IEEE-754 float32, normalized [0,1]. Default 0.4 */
    int32_t smooth_delta;        /**< Range [1,100], step 1. Default 20 */
    int32_t persistency_index;   /**< Range [0,8], step 1. Default 3 */

    int32_t reserved[4];         /**< MUST be zero on SET */
} rs2_temporal_filter_dpp_config;

#pragma pack(pop)

/* Fails to compile if padding/field changes push this off the documented 38-byte wire size. */
typedef char rs2_temporal_filter_dpp_config_wire_size_check[ ( sizeof( rs2_temporal_filter_dpp_config ) == 38 ) ? 1 : -1 ];

/** {min, max, step, def} bounds for rs2_temporal_filter_dpp_config, as returned by
* rs2_get_composite_option_range(RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP). Read-only - each
* bound already carries its own header.version, so this wrapper has no version field of its own. */
typedef struct rs2_temporal_filter_dpp_range
{
    rs2_temporal_filter_dpp_config min;
    rs2_temporal_filter_dpp_config max;
    rs2_temporal_filter_dpp_config step;
    rs2_temporal_filter_dpp_config def;
} rs2_temporal_filter_dpp_range;

#ifdef __cplusplus
}
#endif
#endif  // LIBREALSENSE_RS2_TEMPORAL_FILTER_DPP_H
