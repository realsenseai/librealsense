/* License: Apache 2.0. See LICENSE file in root directory.
   Copyright(c) 2026 RealSense, Inc. All Rights Reserved. */

/** \file rs_icr_control.h
* \brief
* Cast-target struct for RS2_COMPOSITE_OPTION_ICR_CONTROL (see rs_composite_option.h). Holds only the
* user-facing settings: the firmware wire layout (header, reserved slots, packing) is private to the
* SDK, so firmware changes do not change this struct. Only some Depth profiles are supported; others
* are rejected when enabling or opening the stream.
*/

#ifndef LIBREALSENSE_RS2_ICR_CONTROL_H
#define LIBREALSENSE_RS2_ICR_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** Improved Close Range (ICR) settings. Read with get_composite_option_as< rs2_icr_control >(),
* modify the fields of interest and write back with set_composite_option_from(), so untouched
* fields keep their value. */
typedef struct rs2_icr_control
{
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
} rs2_icr_control;

/** {min, max, step, def} bounds for rs2_icr_control, as returned by
* rs2_get_composite_option_range(RS2_COMPOSITE_OPTION_ICR_CONTROL). Read-only. */
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
