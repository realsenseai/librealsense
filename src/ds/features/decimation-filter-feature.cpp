// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.


#include <src/ds/features/decimation-filter-feature.h>
#include <src/ds/d500/d500-device.h>
#include <src/ds/d500/composite-embedded-filter.h>
#include <src/ds/ds-private.h>
#include <src/proc/decimation-embedded-filter.h>
#include <src/uvc-sensor.h>

#include <librealsense2/h/rs_composite_option.h>
#include <librealsense2/h/rs_decimation_filter_dpp.h>


namespace librealsense {


/* static */ const feature_id decimation_filter_feature::ID = "Decimation filter feature";

decimation_filter_feature::decimation_filter_feature( d500_depth_sensor & depth_sensor )
{
    // Registers the ONE composite option this filter exposes. No dedicated alias type:
    // decimation_embedded_filter already exists for the DDS path's own scalar-option filter -
    // reused here as-is, since it's just the RS2_EXTENSION_* identity, no DDS-specific state.
    auto raw_depth_ep = std::dynamic_pointer_cast< uvc_sensor >( depth_sensor.get_raw_sensor() );
    if( ! raw_depth_ep )
        throw std::runtime_error( "Decimation Filter DPP requires a UVC depth sensor" );
    depth_sensor.add_embedded_filter( std::make_shared<
        composite_embedded_filter< decimation_embedded_filter, RS2_EMBEDDED_FILTER_TYPE_DECIMATION > >(
        raw_depth_ep,
        ds::DS5_HKR_DECIMATION_FILTER_DPP,
        static_cast< uint32_t >( sizeof( rs2_decimation_filter_dpp_config ) ),
        RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP,
        "Decimation Filter DPP (prototype) - use rs2_set/get_composite_option, see rs_decimation_filter_dpp.h" ) );
}

feature_id decimation_filter_feature::get_id() const
{
    return ID;
}


}  // namespace librealsense
