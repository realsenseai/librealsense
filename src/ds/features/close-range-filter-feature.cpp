// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.


#include <src/ds/features/close-range-filter-feature.h>
#include <src/ds/d500/d500-device.h>
#include <src/ds/d500/d500-close-range-embedded-filter.h>
#include <src/uvc-sensor.h>


namespace librealsense {


/* static */ const feature_id close_range_filter_feature::ID = "Close range filter feature";

close_range_filter_feature::close_range_filter_feature( d500_depth_sensor & depth_sensor )
{
    auto raw_depth_ep = std::dynamic_pointer_cast< uvc_sensor >( depth_sensor.get_raw_sensor() );
    depth_sensor.add_embedded_filter( std::make_shared< d500_close_range_embedded_filter >( raw_depth_ep ) );
}

feature_id close_range_filter_feature::get_id() const
{
    return ID;
}


}  // namespace librealsense
