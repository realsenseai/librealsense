/* License: Apache 2.0. See LICENSE file in root directory.
   Copyright(c) 2026 RealSense, Inc. All Rights Reserved. */

/** \file rs_temporal_filter_dpp.h
* \brief
* Cast-target struct for RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP (see rs_composite_option.h).
* Holds only the user-facing settings: the firmware wire layout (header, unused slots, packing) is
* private to the SDK, so firmware changes do not change this struct.
*/

#ifndef LIBREALSENSE_RS2_TEMPORAL_FILTER_DPP_H
#define LIBREALSENSE_RS2_TEMPORAL_FILTER_DPP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** Embedded Temporal filter settings. Read with get_composite_option_as(), change the fields of
* interest and write back with set_composite_option_from(). */
typedef struct rs2_temporal_filter_dpp_config
{
    int32_t enabled;             /**< 0 = Off, 1 = On. Default 0 */
    int32_t smooth_alpha;        /**< Normalized [0,1] scaled into [0,1000] (every setting is an
                                  * int32, not a float). Default 400 (0.4) */
    int32_t smooth_delta;        /**< Range [1,100], step 1. Default 20 */
    int32_t persistency_index;   /**< Range [0,8], step 1. Default 3 */
} rs2_temporal_filter_dpp_config;

/** {min, max, step, def} bounds for rs2_temporal_filter_dpp_config, as returned by
* rs2_get_composite_option_range(RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP). Read-only. */
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
