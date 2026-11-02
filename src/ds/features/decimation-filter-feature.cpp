// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.


#include <src/ds/features/decimation-filter-feature.h>
#include <src/ds/d500/d500-device.h>
#include <src/ds/d500/composite-embedded-filter.h>
#include <src/ds/ds-private.h>
#include <src/ds/composite-xu-option.h>
#include <src/proc/decimation-embedded-filter.h>
#include <src/uvc-sensor.h>

#include <librealsense2/h/rs_composite_option.h>
#include <librealsense2/h/rs_decimation_filter_dpp.h>


namespace librealsense {


// D500_CAMERA_CID_DECIMATION range descriptor - magnitude is currently fixed at 2 on the
// FW side, per rs_decimation_filter_dpp.h.
// GMSL cannot query ranges (V4L2 array controls report one scalar bound and the FW
// registers hold only the current value), so these tables are the only source of the MIPI
// range. USB reads it from FW and ignores this desc.
static constexpr int32_t dec_min[]  = { 0, 2 };
static constexpr int32_t dec_max[]  = { 1, 2 };
static constexpr int32_t dec_step[] = { 1, 1 };
static constexpr int32_t dec_def[]  = { 0, 2 };

static constexpr dpp_control_desc decimation_desc = {
    0x0001,  // ctl_id (D500_DPP_XU_DECIMATION_CONTROL_ID = BIT(0))
    2,       // param_count
    0x00,    // param_type (all-integer)
    dec_min, dec_max, dec_step, dec_def
};


/* static */ const feature_id decimation_filter_feature::ID = "Decimation filter feature";

decimation_filter_feature::decimation_filter_feature( d500_depth_sensor & depth_sensor, bool is_mipi )
{
    // Registers the ONE composite option this filter exposes. No dedicated alias type:
    // decimation_embedded_filter already exists for the DDS path's own scalar-option filter -
    // reused here as-is, since it's just the RS2_EXTENSION_* identity, no DDS-specific state.
    auto raw_depth_ep = std::dynamic_pointer_cast< uvc_sensor >( depth_sensor.get_raw_sensor() );
    if( ! raw_depth_ep )
        throw std::runtime_error( "Decimation Filter DPP requires a UVC depth sensor" );
    auto option = composite_xu_option::create(
        is_mipi,
        raw_depth_ep,
        ds::depth_xu,
        ds::DS5_HKR_DECIMATION_FILTER_DPP,
        static_cast< uint32_t >( sizeof( rs2_decimation_filter_dpp_config ) ),
        "Decimation Filter DPP (prototype) - use rs2_set/get_composite_option, see rs_decimation_filter_dpp.h",
        decimation_desc );
    depth_sensor.add_embedded_filter( std::make_shared<
        composite_embedded_filter< decimation_embedded_filter, RS2_EMBEDDED_FILTER_TYPE_DECIMATION > >(
        std::move( option ), RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP ) );
}

feature_id decimation_filter_feature::get_id() const
{
    return ID;
}


}  // namespace librealsense
