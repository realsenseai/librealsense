// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023-4 RealSense, Inc. All Rights Reserved.
#pragma once

#include "sid_index.h"
#include <src/frame.h>
#include <src/software-sensor.h>
#include <src/proc/formats-converter.h>
#include <src/core/options-watcher.h>

#include <realdds/dds-defines.h>
#include <realdds/dds-metadata-syncer.h>
#include <realdds/dds-embedded-filter.h>
#include <realdds/dds-stream-profile.h>

#include <rsutils/json-fwd.h>
#include <memory>
#include <map>


namespace realdds {
class dds_device;
class dds_stream;
class dds_option;
class dds_video_stream_profile;
class dds_motion_stream_profile;
namespace topics {
class imu_msg;
}  // namespace topics
}  // namespace realdds


namespace librealsense {

class video_stream_profile;
class dds_device_proxy;
class roi_sensor_interface;


class dds_sensor_proxy : public software_sensor
{
    using super = software_sensor;

    std::shared_ptr< realdds::dds_device > const _dev;
    std::string const _name;
    bool const _md_enabled;
    options_watcher _options_watcher;

    typedef realdds::dds_metadata_syncer syncer_type;
    static void frame_releaser( syncer_type::frame_type * f ) { static_cast< frame * >( f )->release(); }

    std::shared_ptr< roi_sensor_interface > _roi_support;

protected:
    struct streaming_impl
    {
        syncer_type syncer;
        std::atomic< unsigned long long > last_frame_number{ 0 };
        std::atomic< rs2_time_t > last_timestamp;
    };

private:
    std::map< sid_index, std::shared_ptr< realdds::dds_stream > > _streams;
    std::map< std::string, streaming_impl > _streaming_by_name;

    formats_converter _formats_converter;
    stream_profiles _active_converted_profiles;

public:
    dds_sensor_proxy( std::string const & sensor_name,
                      software_device * owner,
                      std::shared_ptr< realdds::dds_device > const & dev );

    bool extend_to( rs2_extension, void ** ptr ) override;  // extendable_interface

    const std::string & get_name() const { return _name; }

    void add_dds_stream( sid_index sidx, std::shared_ptr< realdds::dds_stream > const & stream );

    void open( const stream_profiles & profiles ) override;
    void start( rs2_frame_callback_sptr callback ) override;
    void stop();
    void close() override;

    void add_option( std::shared_ptr< realdds::dds_option > option );

    void add_processing_block( std::string const & filter_name );
    virtual void add_embedded_filter(std::shared_ptr< realdds::dds_embedded_filter > embedded_filter);

    const std::map< sid_index, std::shared_ptr< realdds::dds_stream > > & streams() const { return _streams; }
    void set_frames_callback( rs2_frame_callback_sptr callback ) override;
    rs2_frame_callback_sptr get_frames_callback() const override;

    // sensor_interface
public:
    rsutils::subscription register_options_changed_callback( options_watcher::callback && ) override;
    stream_profiles get_active_streams() const override;

protected:
    void register_converters();
    stream_profiles init_stream_profiles() override;
    void log_bandwidth( const std::shared_ptr< librealsense::video_stream_profile > & vsp ) const;

    std::shared_ptr< realdds::dds_video_stream_profile >
    find_profile( sid_index sidx, realdds::dds_video_stream_profile const & profile ) const;

    std::shared_ptr< realdds::dds_motion_stream_profile >
    find_profile( sid_index sidx, realdds::dds_motion_stream_profile const & profile ) const;

    realdds::dds_stream_profiles find_dds_profiles( const librealsense::stream_profiles & source_profiles ) const;
    
    void handle_video_data( std::vector< uint8_t > &&,
                            realdds::dds_time &&,
                            realdds::dds_sample &&,
                            const std::shared_ptr< stream_profile_interface > &,
                            streaming_impl & streaming );
    void handle_motion_data( realdds::topics::imu_msg &&,
                             realdds::dds_sample &&,
                             const std::shared_ptr< stream_profile_interface > &,
                             streaming_impl & );
    void handle_new_metadata( std::string const & stream_name,
                              std::shared_ptr< const rsutils::json > const & metadata );

    virtual void add_no_metadata( frame *, streaming_impl & );
    virtual void add_frame_metadata( frame *, rsutils::json const & metadata, streaming_impl & );

    void add_processing_block_settings( const std::string & filter_name,
                                        std::shared_ptr< librealsense::processing_block_interface > & ppb ) const;

    friend class dds_device_proxy;  // Currently calls handle_new_metadata
};


}  // namespace librealsense
