// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.
#pragma once

#include "rs-dds-sensor-proxy.h"
#include <src/depth-sensor.h>


namespace librealsense {

class rs_dds_embedded_filter;

// For cases when checking if this is< depth_sensor > or is< depth_stereo_sensor > (like realsense-viewer::subdevice_model and on-chip-calib)
class dds_depth_sensor_proxy
    : public dds_sensor_proxy
    , public depth_stereo_sensor
{
    using super = dds_sensor_proxy;

public:
    dds_depth_sensor_proxy(std::string const& sensor_name,
        software_device* owner,
        std::shared_ptr< realdds::dds_device > const& dev)
        : super(sensor_name, owner, dev)
    {
    }

    // Needed by abstract interfaces
    float get_depth_scale() const override;
    float get_stereo_baseline_mm() const override;

    bool extend_to( rs2_extension, void ** ptr ) override;  // extendable_interface
    embedded_filters get_supported_embedded_filters() const override;
    void add_embedded_filter(std::shared_ptr< embedded_filter_interface > embedded_filter);
    void open( const stream_profiles & profiles ) override;

protected:
    void add_no_metadata( frame *, streaming_impl & ) override;
    void add_frame_metadata( frame *, rsutils::json const & md, streaming_impl & ) override;
    std::map< rs2_embedded_filter_type, std::shared_ptr< embedded_filter_interface > > _embedded_filters;
};

}  // namespace librealsense
