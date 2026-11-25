// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#pragma once

#include "d500-device.h"
#include <src/depth-mapping-sensor.h>
#include "core/video.h"
#include <src/core/tagged-profile.h>
#include <rsutils/lazy.h>
#include <map>
#include <mutex>

namespace librealsense
{
    class d500_depth_mapping_sensor;

    extern const std::map< uint32_t, rs2_format > mapping_fourcc_to_rs2_format;
    extern const std::map< uint32_t, rs2_stream > mapping_fourcc_to_rs2_stream;

    std::shared_ptr< stream_profile_interface > find_stream( const stream_profiles & profiles, rs2_stream type );
    bool is_pcl_transport( const platform::stream_profile & p );

    class d500_depth_mapping : public virtual d500_device
    {
    public:
        std::shared_ptr<synthetic_sensor> create_depth_mapping_device(std::shared_ptr<context> ctx,
            std::shared_ptr<platform::uvc_device> mapping_device, const std::string& physical_port);

        d500_depth_mapping( std::shared_ptr< const d500_info > const & );

        // False when the device does not expose a Mapping interface.
        bool is_depth_mapping_active() const { return _depth_mapping_active; }

        // Append the occupancy/point-cloud streams (and default profile tag) only when
        // active; go through these instead of the raw members so a call site can't forget.
        void add_streams_if_active( std::vector< std::shared_ptr< stream_interface > > & streams ) const;
        void add_profile_tag_if_active( std::vector< tagged_profile > & tags ) const;

        // Occupancy grid and labeled point cloud params, exposed through the depth mapping sensor
        std::string get_depth_mapping_params() const;
        void set_depth_mapping_params( const std::string & params_json_str ) const;

    protected:

        friend class d500_depth_mapping_sensor;
        friend class d500_perception_sensor;

        void register_extrinsics();

        void register_options(std::shared_ptr<d500_depth_mapping_sensor> mapping_ep, std::shared_ptr<uvc_sensor> raw_mapping_sensor);
        void register_metadata(std::shared_ptr<uvc_sensor> mapping_ep);
        void register_processing_blocks(std::shared_ptr<d500_depth_mapping_sensor> mapping_ep);
        
        void register_occupancy_metadata(std::shared_ptr<uvc_sensor> raw_mapping_ep);
        void register_point_cloud_metadata(std::shared_ptr<uvc_sensor> raw_mapping_ep);

        // Throws if depth sensor colors and Occupancy/LPCL would stream together over USB, in either start order
        void colors_with_mapping_or_throw( const stream_profiles & depth_streams, const stream_profiles & mapping_streams ) const;

        // Selects mapping stream e.g. PCL XYZ vs XYZRGB; FW reads selection at the UVC commit that follows
        void select_mapping_stream( const stream_profiles & requests ) const;

        int _depth_mapping_idx = -1;
        // FW that supports it takes the requested stream and format from the host; other FW picks it by the profile alone
        bool _supports_stream_selection = false;
        bool _supports_pcl_format = false;

    protected:
        std::shared_ptr<stream_interface> _occupancy_stream;
        std::shared_ptr<stream_interface> _point_cloud_stream;
        std::shared_ptr<stream_interface> _pcl_stream;  // device point cloud (RS2_STREAM_POINT_CLOUD)
        std::shared_ptr<uvc_sensor> _raw_depth_mapping_ep;
        bool _depth_mapping_active = false;
        // True for D585S (mapping on MI 13, 2880-wide payloads), false for every other
        // D5xx (mapping on MI 11, OCCG 320x256 and LPCL 640x360).
        bool _is_safety_layout = false;
        bool _is_d585s = false;
        // Serializes get/set of the depth mapping params, so the GET that explains a failed set reads that set's result
        mutable std::mutex _depth_mapping_params_mutex;
        std::shared_ptr<rsutils::lazy<rs2_extrinsics>> _depth_to_depth_mapping_extrinsics;
        // Without the safety layout, the Mapping interface (or its MUX streams) for the Perception sensor
        std::vector< std::shared_ptr< platform::uvc_device > > _mapping_devs;
        std::string _mapping_port;
    };

    class d500_depth_mapping_sensor : public synthetic_sensor,
                              public video_sensor_interface
    {
    public:
        explicit d500_depth_mapping_sensor(d500_depth_mapping* owner,
            std::shared_ptr<uvc_sensor> uvc_sensor,
            std::map<uint32_t, rs2_format> mapping_fourcc_to_rs2_format,
            std::map<uint32_t, rs2_stream> mapping_fourcc_to_rs2_stream,
            std::string const & name = "Depth Mapping Camera")
            : synthetic_sensor(name, uvc_sensor, owner, mapping_fourcc_to_rs2_format, mapping_fourcc_to_rs2_stream),
            _owner(owner)
        {}

        rs2_intrinsics get_intrinsics(const stream_profile& profile) const override;
        stream_profiles init_stream_profiles() override;
        // EP12 carries one Mapping stream at a time (PCL, LPCL, or OG)
        void open(const stream_profiles& requests) override;

    protected:
        d500_depth_mapping* _owner;
    };

    // D585S's and D585 legacy's own sensor for the Mapping streams
    class d500_depth_mapping_camera : public d500_depth_mapping_sensor, public depth_mapping_sensor
    {
    public:
        using d500_depth_mapping_sensor::d500_depth_mapping_sensor;

        std::string get_depth_mapping_params() const override { return _owner->get_depth_mapping_params(); }
        void set_depth_mapping_params( const std::string & params_json_str ) const override
        {
            _owner->set_depth_mapping_params( params_json_str );
        }
    };
}
