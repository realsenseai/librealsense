// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#pragma once

#include "d500-device.h"
#include <src/depth-mapping-sensor.h>
#include "core/video.h"
#include <rsutils/lazy.h>

namespace librealsense
{
    class d500_depth_mapping_sensor;

    class d500_depth_mapping : public virtual d500_device
    {
    public:
        std::shared_ptr<synthetic_sensor> create_depth_mapping_device(std::shared_ptr<context> ctx,
            const std::vector<platform::uvc_device_info>& mapping_devices_info);

        d500_depth_mapping( std::shared_ptr< const d500_info > const &,
                            bool versioned_mapping = false );

        bool has_depth_mapping() const { return _has_depth_mapping; }

    private:

        friend class d500_depth_mapping_sensor;

        void register_extrinsics();

        void register_options(std::shared_ptr<d500_depth_mapping_sensor> mapping_ep, std::shared_ptr<uvc_sensor> raw_mapping_sensor);
        void register_metadata(std::shared_ptr<uvc_sensor> mapping_ep);
        void register_processing_blocks(std::shared_ptr<d500_depth_mapping_sensor> mapping_ep);
        
        void register_occupancy_metadata(std::shared_ptr<uvc_sensor> raw_mapping_ep);
        void register_point_cloud_metadata(std::shared_ptr<uvc_sensor> raw_mapping_ep);

    protected:
        std::shared_ptr<stream_interface> _occupancy_stream;
        std::shared_ptr<stream_interface> _point_cloud_stream;
        uint8_t _depth_mapping_device_idx;
        std::shared_ptr<rsutils::lazy<rs2_extrinsics>> _depth_to_depth_mapping_extrinsics;
        bool _versioned_mapping = false;
        bool _has_depth_mapping = false;
    };

    class d500_depth_mapping_sensor : public synthetic_sensor,
                              public video_sensor_interface,
                              public depth_mapping_sensor
    {
    public:
        explicit d500_depth_mapping_sensor(d500_depth_mapping* owner,
            std::shared_ptr<uvc_sensor> uvc_sensor,
            std::map<uint32_t, rs2_format> mapping_fourcc_to_rs2_format,
            std::map<uint32_t, rs2_stream> mapping_fourcc_to_rs2_stream)
            : synthetic_sensor("Depth Mapping Camera", uvc_sensor, owner, mapping_fourcc_to_rs2_format, mapping_fourcc_to_rs2_stream),
            _owner(owner)
        {}

        rs2_intrinsics get_intrinsics(const stream_profile& profile) const override;
        stream_profiles init_stream_profiles() override;

    protected:
        const d500_depth_mapping* _owner;
    };
}
