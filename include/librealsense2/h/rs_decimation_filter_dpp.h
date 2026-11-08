/* License: Apache 2.0. See LICENSE file in root directory.
   Copyright(c) 2026 RealSense, Inc. All Rights Reserved. */

/** \file rs_decimation_filter_dpp.h
* \brief
* Cast-target struct for RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP (see rs_composite_option.h).
* Holds only the user-facing settings: the firmware wire layout (header, unused slots, packing) is
* private to the SDK, so firmware changes do not change this struct.
*/

#ifndef LIBREALSENSE_RS2_DECIMATION_FILTER_DPP_H
#define LIBREALSENSE_RS2_DECIMATION_FILTER_DPP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** Embedded Decimation filter settings. Read with get_composite_option_as(), change the fields of
* interest and write back with set_composite_option_from(). */
typedef struct rs2_decimation_filter_dpp_config
{
    int32_t enabled;    /**< 0 = Off, 1 = On. Default 0. Read/write only while Depth/IR is
                         * idle - the device rejects a SET once streaming starts. */
    int32_t magnitude;  /**< Downscale factor. Currently fixed at 2 (min=max=default=2) - the
                         * FW-defined range this build must not assume stays a single value. */
} rs2_decimation_filter_dpp_config;

/** {min, max, step, def} bounds for rs2_decimation_filter_dpp_config, as returned by
* rs2_get_composite_option_range(RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP). Read-only. */
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
