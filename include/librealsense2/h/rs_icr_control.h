/* License: Apache 2.0. See LICENSE file in root directory.
   Copyright(c) 2026 RealSense, Inc. All Rights Reserved. */

/** \file rs_icr_control.h
* \brief
* Cast-target struct for RS2_COMPOSITE_OPTION_ICR_CONTROL (see rs_composite_option.h). Wire
* layout: dpp_header + 8 int32 param slots, all used (38 bytes, little-endian, pack(1)); ctl_id =
* 0x0008. Fields 1-6 are read/write, 7-8 are FW-computed telemetry, invalid only before the
* first Depth or Infrared stream after boot (kept after a stream stops). Never SET them - a SET carrying an in-range value is silently ignored.
*/

#ifndef LIBREALSENSE_RS2_ICR_CONTROL_H
#define LIBREALSENSE_RS2_ICR_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rs_dpp_header.h"
#include <stdint.h>

#pragma pack(push, 1)

/** ICR mode values (rs2_icr_control::mode) */
#define RS2_ICR_MODE_OFF 0
#define RS2_ICR_MODE_DOWNSCALE_HDR 1
#define RS2_ICR_MODE_HIGH_RES 2

/** Downscale ratio values (rs2_icr_control::downscale_ratio) */
#define RS2_ICR_DOWNSCALE_RATIO_X2 1
#define RS2_ICR_DOWNSCALE_RATIO_X4 2

/** High-Res mode values (rs2_icr_control::high_res_mode) */
#define RS2_ICR_HIGH_RES_MODE_126PX 0
#define RS2_ICR_HIGH_RES_MODE_64PX 1
#define RS2_ICR_HIGH_RES_MODE_MANUAL 2

/** The entire 38-byte HKR Improved Close Range payload, header included. */
typedef struct rs2_icr_control
{
    dpp_header header;

    int32_t  mode;                  /**< 0 = Off, 1 = Downscale HDR, 2 = High-Res. Range [0,2]. Default 0 */
    int32_t  downscale_ratio;       /**< Meaningful when mode == Downscale HDR: 1 = x2, 2 = x4. Default 1.
                                     * 0 is reserved */
    int32_t  high_res_mode;         /**< Meaningful when mode == High-Res: 0 = 126px, 1 = 64px, 2 = Manual
                                     * (see high_res_offset). Range [0,2]. Default 0 */
    int32_t  high_res_offset;       /**< Disparity shift in pixels, used when high_res_mode == Manual.
                                     * Range [0,256]. Default 126 */
    int32_t  smoothing_offset_mm;   /**< Close Range Smoothing: depth range above MinZ, in mm, where secondary-frame
                                     * data overrides the primary. 0 disables it. Meaningful when mode == Downscale
                                     * HDR. Range [0,10000]. Default 0 */
    int32_t  completion_offset_mm;  /**< Close Range Completion: depth range above MinZ, in mm, where holes are filled
                                     * from the secondary frame. 0 disables it. Meaningful when mode != Off.
                                     * Range [0,10000]. Default 0 */
    int32_t  min_z_mm;              /**< Read-only. FW-computed baseline MinZ for the active resolution. Range [0,500] */
    int32_t  improved_min_z_mm;     /**< Read-only. FW-computed MinZ achieved under the applied configuration.
                                     * Range [0,500] */
} rs2_icr_control;

#pragma pack(pop)

/* Fails to compile if padding/field changes push this off the documented 38-byte wire size. */
typedef char rs2_icr_control_wire_size_check[ ( sizeof( rs2_icr_control ) == 38 ) ? 1 : -1 ];

/** {min, max, step, def} bounds for rs2_icr_control, as returned by
* rs2_get_composite_option_range(RS2_COMPOSITE_OPTION_ICR_CONTROL). Read-only - each
* bound already carries its own header.version, so this wrapper has no version field of its own. */
typedef struct rs2_icr_control_range
{
    rs2_icr_control min;
    rs2_icr_control max;
    rs2_icr_control step;
    rs2_icr_control def;
} rs2_icr_control_range;

#ifdef __cplusplus
}
#endif
#endif  // LIBREALSENSE_RS2_ICR_CONTROL_H
