// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#pragma once

#include "dds-stream-base.h"
#include "dds-trinsics.h"
#include "dds-sample.h"

#include <string>
#include <vector>
#include <set>
#include <functional>

namespace realdds {

namespace topics {
class imu_msg;
class string_msg;
class flexible_msg;
}  // namespace topics

class dds_subscriber;
class dds_topic_reader_thread;

// Represents a stream of information (images, motion data, etc..) from a single source received via the DDS system.
// A stream can have several profiles, i.e different data frequency, image resolution, etc..
// 
// This is a base class: you need to specify the type of stream via the instantiation of a video_stream, etc.
//
// Streaming is over a single topic that remains closed until open() is called. Opening a stream signals the
// server that frames are requested. In order to actually receive callbacks for those frames, use start_streaming().
//
class dds_stream : public dds_stream_base
{
    typedef dds_stream_base super;

protected:
    dds_stream( std::string const & stream_name, std::string const & sensor_name );

public:
    ~dds_stream();

    bool is_open() const override { return !! _reader; }
    virtual void open( std::string const & topic_name, std::shared_ptr< dds_subscriber > const & ) = 0;
    virtual void close();

    bool is_streaming() const override { return _streaming; }
    void start_streaming();
    void stop_streaming();

    std::shared_ptr< dds_topic > const & get_topic() const override;

protected:
    virtual void handle_data() = 0;
    virtual bool can_start_streaming() const = 0;

    std::shared_ptr< dds_topic_reader_thread > _reader;
    bool _streaming = false;
};

class dds_video_stream : public dds_stream
{
    typedef dds_stream super;

public:
    dds_video_stream( std::string const & stream_name, std::string const & sensor_name );

    void open( std::string const & topic_name, std::shared_ptr< dds_subscriber > const & ) override;

    typedef std::function< void( std::vector< uint8_t > &&, dds_time &&, dds_sample && ) > on_data_available_callback;
    void on_data_available( on_data_available_callback cb ) { _on_data_available = cb; }

    void set_intrinsics( std::set< video_intrinsics > intrinsics ) { _intrinsics = std::move( intrinsics ); }
    const std::set< video_intrinsics > & get_intrinsics() const { return _intrinsics; }

protected:
    void handle_data() override;
    bool can_start_streaming() const override { return _on_data_available != nullptr; }

    template< typename frame_type >
    void handle_image()
    {
        frame_type frame;
        dds_sample sample;
        while( _reader && frame_type::take_next( *_reader, &frame, &sample ) )
        {
            if( ! frame.is_valid() )
                continue;

            if( is_streaming() && _on_data_available )
                _on_data_available( std::move( frame.raw().data() ), frame.timestamp(), std::move( sample ) );
        }
    }

    std::set< video_intrinsics > _intrinsics;
    on_data_available_callback _on_data_available = nullptr;
};

class dds_depth_stream : public dds_video_stream
{
    typedef dds_video_stream super;

public:
    dds_depth_stream( std::string const & stream_name, std::string const & sensor_name );

    char const * type_string() const override { return "depth"; }
};

class dds_ir_stream : public dds_video_stream
{
    typedef dds_video_stream super;

public:
    dds_ir_stream( std::string const & stream_name, std::string const & sensor_name );

    char const * type_string() const override { return "ir"; }
};

class dds_color_stream : public dds_video_stream
{
    typedef dds_video_stream super;

public:
    dds_color_stream( std::string const & stream_name, std::string const & sensor_name );

    char const * type_string() const override { return "color"; }
};

class dds_confidence_stream : public dds_video_stream
{
    typedef dds_video_stream super;

public:
    dds_confidence_stream( std::string const & stream_name, std::string const & sensor_name );

    char const * type_string() const override { return "confidence"; }
};

class dds_motion_stream : public dds_stream
{
    typedef dds_stream super;

public:
    dds_motion_stream( std::string const & stream_name, std::string const & sensor_name );

    char const * type_string() const override { return "motion"; }

    void open( std::string const & topic_name, std::shared_ptr< dds_subscriber > const & ) override;

    typedef std::function< void( topics::imu_msg &&, dds_sample && ) > on_data_available_callback;
    void on_data_available( on_data_available_callback cb ) { _on_data_available = cb; }

    void set_accel_intrinsics( const motion_intrinsics & intrinsics ) { _accel_intrinsics = intrinsics; }
    void set_gyro_intrinsics( const motion_intrinsics & intrinsics ) { _gyro_intrinsics = intrinsics; }
    const motion_intrinsics & get_accel_intrinsics() const { return _accel_intrinsics; }
    const motion_intrinsics & get_gyro_intrinsics() const { return _gyro_intrinsics; }

protected:
    void handle_data() override;
    bool can_start_streaming() const override { return _on_data_available != nullptr; }

    motion_intrinsics _accel_intrinsics;
    motion_intrinsics _gyro_intrinsics;
    on_data_available_callback _on_data_available = nullptr;
};

class dds_inference_stream : public dds_stream
{
    typedef dds_stream super;

public:
    dds_inference_stream( std::string const & stream_name, std::string const & sensor_name );

    char const * type_string() const override { return "inference"; }

    void open( std::string const & topic_name, std::shared_ptr< dds_subscriber > const & ) override;

    typedef std::function< void( topics::string_msg &&, dds_sample && ) > on_data_available_callback;
    void on_data_available( on_data_available_callback cb ) { _on_data_available = cb; }

protected:
    void handle_data() override;
    bool can_start_streaming() const override { return _on_data_available != nullptr; }

    on_data_available_callback _on_data_available = nullptr;
};

class dds_object_detection_stream : public dds_inference_stream
{
    typedef dds_inference_stream super;

public:
    dds_object_detection_stream( std::string const & stream_name, std::string const & sensor_name );

    char const * type_string() const override { return "object_detection"; }
};

typedef std::vector< std::shared_ptr< dds_stream > > dds_streams;


}  // namespace realdds
