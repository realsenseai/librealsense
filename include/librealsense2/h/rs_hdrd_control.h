/* License: Apache 2.0. See LICENSE file in root directory.
   Copyright(c) 2026 RealSense, Inc. All Rights Reserved. */

/** \file rs_hdrd_control.h
* \brief
* Cast-target struct for RS2_COMPOSITE_OPTION_HDRD_CONTROL (see rs_composite_option.h). Wire
* layout: dpp_header + 8 int32 param slots, 7 used (38 bytes, little-endian, pack(1)); ctl_id =
* 0x0008. Only some Depth profiles are supported; others are rejected when enabling or opening the stream.
*/

#ifndef LIBREALSENSE_RS2_HDRD_CONTROL_H
#define LIBREALSENSE_RS2_HDRD_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rs_dpp_header.h"
#include <stdint.h>

#pragma pack(push, 1)

/** The entire 38-byte HKR Improved Close Range payload, header included. */
typedef struct rs2_hdrd_control
{
    dpp_header header;

    int32_t  enable;                /**< 0 = Off, 1 = On. Default 0 */
    int32_t  filter_type;           /**< 0 = Downscale, 1 = Lookup shift - selects which of the
                                     * next two field-pairs is meaningful */
    int32_t  downscale_ratio;       /**< Meaningful when filter_type == Downscale: 1 = x2, 2 = x4.
                                     * Default 1. 0 is reserved */
    int32_t  shift_mode;            /**< Meaningful when filter_type == Lookup shift: 0 = 126px,
                                     * 1 = 64px, 2 = manual (see shift_pixels). Default 0 */
    int32_t  shift_pixels;          /**< Used when shift_mode == Manual. Range [0,256]. Default 126 */
    int32_t  threshold_mode;        /**< 0 = Zero range, 1 = MinZ range (FW-computed, not exposed
                                     * to the host), 2 = Manual (see threshold_mm). Default 1 */
    int32_t  threshold_mm;          /**< Used when threshold_mode == Manual. Range [0,65535].
                                     * Default 0 */

    int32_t  reserved[1];           /**< MUST be zero on SET */
} rs2_hdrd_control;

#pragma pack(pop)

/* Fails to compile if padding/field changes push this off the documented 38-byte wire size. */
typedef char rs2_hdrd_control_wire_size_check[ ( sizeof( rs2_hdrd_control ) == 38 ) ? 1 : -1 ];

/** {min, max, step, def} bounds for rs2_hdrd_control, as returned by
* rs2_get_composite_option_range(RS2_COMPOSITE_OPTION_HDRD_CONTROL). Read-only - each
* bound already carries its own header.version, so this wrapper has no version field of its own. */
typedef struct rs2_hdrd_control_range
{
    rs2_hdrd_control min;
    rs2_hdrd_control max;
    rs2_hdrd_control step;
    rs2_hdrd_control def;
} rs2_hdrd_control_range;

#ifdef __cplusplus
}
#endif
#endif  // LIBREALSENSE_RS2_HDRD_CONTROL_H
