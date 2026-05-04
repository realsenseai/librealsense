// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.
#pragma once

#include "sensor.h"
#include "core/extension.h"
#include <librealsense2/hpp/rs_types.hpp>
#include <librealsense2/h/rs_internal.h>
#include <rsutils/lazy.h>
#include <embedded-filter-interface.h>

namespace librealsense {


class software_device;
class stream_profile_interface;
class video_stream_profile_interface;
class embedded_filter_interface;


class software_sensor
    : public sensor_base
    , public extendable_interface
{
public:
    software_sensor( std::string const & name, software_device * owner );
    ~software_sensor();

    virtual std::shared_ptr< stream_profile_interface > add_video_stream( rs2_video_stream video_stream,
                                                                          bool is_default = false, std::string name = "" );
    virtual std::shared_ptr< stream_profile_interface > add_motion_stream( rs2_motion_stream motion_stream,
                                                                           bool is_default = false, std::string name = "" );
    virtual std::shared_ptr< stream_profile_interface > add_pose_stream( rs2_pose_stream pose_stream,
                                                                         bool is_default = false, std::string name = "" );
    virtual std::shared_ptr< stream_profile_interface > add_inference_stream( rs2_inference_stream inference_stream,
                                                                              bool is_default = false, std::string name = "" );

    bool extend_to( rs2_extension extension_type, void ** ptr ) override;

    stream_profiles init_stream_profiles() override;
    stream_profiles const & get_raw_stream_profiles() const override { return _sw_profiles; }

    void open( const stream_profiles & requests ) override;
    void close() override;

    using frame_callback_ptr = std::shared_ptr< rs2_frame_callback >;
    void start( frame_callback_ptr callback ) override;
    void stop() override;

    void on_video_frame( rs2_software_video_frame const & );
    void on_motion_frame( rs2_software_motion_frame const & );
    void on_pose_frame( rs2_software_pose_frame const & );
    void on_notification( rs2_software_notification const & );
    void add_read_only_option( rs2_option option, float val );
    void update_read_only_option( rs2_option option, float val );
    void add_option( rs2_option option, option_range range, bool is_writable );
    void set_metadata( rs2_frame_metadata_value key, rs2_metadata_type value );
    void erase_metadata( rs2_frame_metadata_value key );

    // options_container
public:
    std::vector< rs2_option > get_supported_options() const override;

protected:
    frame_interface * allocate_new_frame( rs2_extension, stream_profile_interface *, frame_additional_data && );
    frame_interface * allocate_new_video_frame( video_stream_profile_interface *, int stride, int bpp, frame_additional_data && );
    void invoke_new_frame( frame_holder &&, void const * pixels, std::function< void() > on_release );

    metadata_array _metadata_map;

    processing_blocks get_recommended_processing_blocks() const override { return _pbs; }
    embedded_filters get_supported_embedded_filters() const override { return embedded_filters(); };
    void add_processing_block( std::shared_ptr< processing_block_interface > const & );

    // We build profiles using add_video_stream(), etc., and feed those into init_stream_profiles() which could in
    // theory change them: so these are our "raw" profiles before initialization...
    stream_profiles _sw_profiles;

private:
    friend class software_device;

    class stereo_extension;
    rsutils::lazy< stereo_extension > _stereo_extension;

    processing_blocks _pbs;
};

MAP_EXTENSION( RS2_EXTENSION_SOFTWARE_SENSOR, software_sensor );


}  // namespace librealsense
