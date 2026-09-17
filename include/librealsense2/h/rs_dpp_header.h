// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

/** \file rs_dpp_header.h
* \brief
* Wire header shared by every control in the HKR Depth Post-Processing (DPP) composite-option
* family: this header + a fixed 8-slot 32-bit param block; param_count says how many are active,
* the rest MUST be zero on SET. See rs_hdrd_control.h for a control that uses it.
*/

#ifndef LIBREALSENSE_RS2_DPP_HEADER_H
#define LIBREALSENSE_RS2_DPP_HEADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** The only dpp_header wire version this SDK build knows how to interpret. A struct read back
* with a different `header.version` (including 0, "never populated") is rejected - see
* rs_options.hpp's get_composite_option_as() - rather than silently misread. */
#define DPP_HEADER_CURRENT_VERSION 1

#pragma pack(push, 1)

typedef struct dpp_header
{
    uint8_t  version;       /**< Wire struct version - see DPP_HEADER_CURRENT_VERSION */
    uint8_t  flags;         /**< Bitwise control-status mask (active/read-only) */
    uint16_t ctl_id;        /**< dpp_ctrl_list entry identifying this control */
    uint8_t  param_count;   /**< Active param slots out of the fixed 8 that follow */
    uint8_t  param_type;    /**< Per-param int/float bitmap. 0x00 = all-integer; a float slot is IEEE-754 float32 */
} dpp_header;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif
#endif  // LIBREALSENSE_RS2_DPP_HEADER_H
