// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/feature-interface.h>
#include <src/embedded-filter-interface.h>

#include <memory>


namespace librealsense {

class d500_device;
class d500_depth_sensor;
class device;
class sensor_interface;
class composite_option_interface;

// Wires the HKR Improved Close Range (MinZ) Control composite-option embedded filter (hdrd_embedded_filter) onto a
// depth sensor, and rejects enabling it on Depth profiles the FW cannot process.
class hdrd_filter_feature : public feature_interface
{
public:
    static const feature_id ID;

    // USB/MIPI: the control is the HKR XU, and the filter is added to depth_sensor
    hdrd_filter_feature( d500_device & owner, d500_depth_sensor & depth_sensor, bool is_mipi );
    // Any transport (e.g. DDS) providing the rs2_hdrd_control payload; the caller adds get_filter() to its depth sensor
    hdrd_filter_feature( device & owner, const sensor_interface & depth_sensor,
                         std::shared_ptr< composite_option_interface > control );

    feature_id get_id() const override;
    std::shared_ptr< embedded_filter_interface > get_filter() const { return _filter; }

private:
    std::shared_ptr< embedded_filter_interface > _filter;
};

}  // namespace librealsense
