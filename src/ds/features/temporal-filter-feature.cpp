// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.


#include <src/ds/features/temporal-filter-feature.h>
#include <src/ds/d500/d500-device.h>
#include <src/ds/d500/composite-embedded-filter.h>
#include <src/ds/ds-private.h>
#include <src/ds/composite-xu-option.h>
#include <src/proc/temporal-embedded-filter.h>
#include <src/uvc-sensor.h>

#include <librealsense2/h/rs_composite_option.h>
#include <librealsense2/h/rs_temporal_filter_dpp.h>


namespace librealsense {


// D500_CAMERA_CID_TEMPORAL range descriptor - smooth_alpha is a milli-unit-encoded
// fixed-point ([0,1000] step 10, param_type BIT(1)).
// GMSL cannot query ranges (V4L2 array controls report one scalar bound and the FW
// registers hold only the current value), so these tables are the only source of the MIPI
// range. USB reads it from FW and ignores this desc.
static constexpr int32_t tmp_min[]  = { 0,    0,   1, 0 };
static constexpr int32_t tmp_max[]  = { 1, 1000, 100, 8 };
static constexpr int32_t tmp_step[] = { 1,   10,   1, 1 };
static constexpr int32_t tmp_def[]  = { 0,  400,  20, 3 };

static constexpr dpp_control_desc temporal_desc = {
    0x0002,  // ctl_id (D500_DPP_XU_TEMPORAL_CONTROL_ID = BIT(1))
    4,       // param_count
    0x02,    // param_type (D500_DPP_XU_TEMPORAL_PARAM_TYPE = BIT(1), slot 1 is a fixed-point alpha)
    tmp_min, tmp_max, tmp_step, tmp_def
};


/* static */ const feature_id temporal_filter_feature::ID = "Temporal filter feature";

temporal_filter_feature::temporal_filter_feature( d500_depth_sensor & depth_sensor, bool is_mipi )
{
    // Registers the ONE composite option this filter exposes. No dedicated alias type:
    // temporal_embedded_filter is the RS2_EXTENSION_* identity, and this is the only place that
    // ever constructs it (compare hdrd-embedded-filter.h, which needs a named alias).
    auto raw_depth_ep = std::dynamic_pointer_cast< uvc_sensor >( depth_sensor.get_raw_sensor() );
    auto option = composite_xu_option::create(
        is_mipi,
        raw_depth_ep,
        ds::depth_xu,
        ds::DS5_HKR_TEMPORAL_FILTER_DPP,
        static_cast< uint32_t >( sizeof( rs2_temporal_filter_dpp_config ) ),
        "Temporal Filter DPP (prototype) - use rs2_set/get_composite_option, see rs_temporal_filter_dpp.h",
        temporal_desc );
    depth_sensor.add_embedded_filter( std::make_shared<
        composite_embedded_filter< temporal_embedded_filter, RS2_EMBEDDED_FILTER_TYPE_TEMPORAL > >(
        std::move( option ), RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP ) );
}

feature_id temporal_filter_feature::get_id() const
{
    return ID;
}


}  // namespace librealsense
