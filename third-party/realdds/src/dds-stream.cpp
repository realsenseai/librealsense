// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#include <realdds/dds-stream.h>

#include <realdds/dds-topic.h>
#include <realdds/dds-topic-reader-thread.h>
#include <realdds/dds-subscriber.h>
#include <realdds/topics/compressed-image-msg.h>
#include <realdds/topics/string-msg.h>
#include <realdds/topics/image-msg.h>
#include <realdds/topics/imu-msg.h>
#include <realdds/topics/flexible-msg.h>
#include <realdds/dds-exceptions.h>
#include <realdds/dds-sample.h>

#include <rsutils/json.h>


namespace realdds {


dds_stream::dds_stream( std::string const & stream_name, std::string const & sensor_name )
    : super( stream_name, sensor_name )
{
}


dds_stream::~dds_stream()
{
    if( _reader )
        _reader->stop();
}


void dds_video_stream::open( std::string const & topic_name, std::shared_ptr< dds_subscriber > const & subscriber )
{
    if( is_open() )
        DDS_THROW( runtime_error, "stream '" + name() + "' is already open" );
    if( profiles().empty() )
        DDS_THROW( runtime_error, "stream '" + name() + "' has no profiles" );

    // Topics with same name and type can be created multiple times (multiple open() calls) without an error.
    std::shared_ptr< dds_topic > topic;
    if( _compressed )
        topic = topics::compressed_image_msg::create_topic( subscriber->get_participant(), topic_name.c_str() );
    else
        topic = topics::image_msg::create_topic( subscriber->get_participant(), topic_name.c_str() );

    // To support automatic streaming (without the need to handle start/stop-streaming commands) the reader is created
    // here and destroyed on close()
    _reader = std::make_shared< dds_topic_reader_thread >( topic, subscriber );
    _reader->on_data_available( [this]() { handle_data(); } );
    _reader->run( dds_topic_reader::qos( eprosima::fastdds::dds::BEST_EFFORT_RELIABILITY_QOS ) );  // no retries
}


void dds_motion_stream::open( std::string const & topic_name, std::shared_ptr< dds_subscriber > const & subscriber )
{
    if( is_open() )
        DDS_THROW( runtime_error, "stream '" + name() + "' is already open" );
    if( profiles().empty() )
        DDS_THROW( runtime_error, "stream '" + name() + "' has no profiles" );

    // Topics with same name and type can be created multiple times (multiple open() calls) without an error.
    auto topic = topics::imu_msg::create_topic( subscriber->get_participant(), topic_name.c_str() );

    // To support automatic streaming (without the need to handle start/stop-streaming commands) the reader is created
    // here and destroyed on close()
    _reader = std::make_shared< dds_topic_reader_thread >( topic, subscriber );
    _reader->on_data_available( [this]() { handle_data(); } );
    _reader->run( dds_topic_reader::qos( eprosima::fastdds::dds::BEST_EFFORT_RELIABILITY_QOS ) );  // no retries
}


void dds_inference_stream::open( std::string const & topic_name, std::shared_ptr< dds_subscriber > const & subscriber )
{
    if( is_open() )
        DDS_THROW( runtime_error, "stream '" + name() + "' is already open" );
    if( profiles().empty() )
        DDS_THROW( runtime_error, "stream '" + name() + "' has no profiles" );

    // Topics with same name and type can be created multiple times (multiple open() calls) without an error.
    auto topic = topics::string_msg::create_topic( subscriber->get_participant(), topic_name.c_str() );

    // To support automatic streaming (without the need to handle start/stop-streaming commands) the reader is created
    // here and destroyed on close()
    _reader = std::make_shared< dds_topic_reader_thread >( topic, subscriber );
    _reader->on_data_available( [this]() { handle_data(); } );
    _reader->run( dds_topic_reader::qos( eprosima::fastdds::dds::BEST_EFFORT_RELIABILITY_QOS ) );  // no retries
}


void dds_stream::close()
{
    if( _reader )
        _reader->stop();
    _reader.reset();
}


void dds_stream::start_streaming()
{
    if( ! is_open() )
        DDS_THROW( runtime_error, "stream '" + name() + "' is not open" );
    if( is_streaming() )
        DDS_THROW( runtime_error, "stream '" + name() + "' is already streaming" );
    if( ! can_start_streaming() )
        DDS_THROW( runtime_error, "stream '" + name() + "' cannot start streaming" );

    _streaming = true;
}


void dds_video_stream::handle_data()
{
    if( _compressed )
        handle_image< topics::compressed_image_msg >();
    else
        handle_image< topics::image_msg >();
}


void dds_motion_stream::handle_data()
{
    topics::imu_msg imu;
    dds_sample sample;
    while( _reader && topics::imu_msg::take_next( *_reader, &imu, &sample ) )
    {
        if( is_streaming() && _on_data_available )
            _on_data_available( std::move( imu ), std::move( sample ) );
    }
}


void dds_inference_stream::handle_data()
{
    topics::string_msg msg;
    dds_sample sample;
    while( _reader && topics::string_msg::take_next( *_reader, &msg, &sample ) )
    {
        if( is_streaming() && _on_data_available )
            _on_data_available( std::move( msg ), std::move( sample ) );
    }
}


void dds_stream::stop_streaming()
{
    if( ! is_streaming() )
        DDS_THROW( runtime_error, "stream '" + name() + "' is not streaming" );

    _streaming = false;
}


std::shared_ptr< dds_topic > const & dds_stream::get_topic() const
{
    DDS_THROW( runtime_error, "stream '" + name() + "' must be open to get_topic()" );
}

dds_video_stream::dds_video_stream( std::string const & stream_name, std::string const & sensor_name )
    : super( stream_name, sensor_name )
{
}


dds_depth_stream::dds_depth_stream( std::string const & stream_name, std::string const & sensor_name )
    : super( stream_name, sensor_name )
{
}


dds_ir_stream::dds_ir_stream( std::string const & stream_name, std::string const & sensor_name )
    : super( stream_name, sensor_name )
{
}


dds_color_stream::dds_color_stream( std::string const & stream_name, std::string const & sensor_name )
    : super( stream_name, sensor_name )
{
}


dds_confidence_stream::dds_confidence_stream( std::string const & stream_name, std::string const & sensor_name )
    : super( stream_name, sensor_name )
{
}


dds_motion_stream::dds_motion_stream( std::string const & stream_name, std::string const & sensor_name )
    : super( stream_name, sensor_name )
{
}


dds_inference_stream::dds_inference_stream( std::string const & stream_name, std::string const & sensor_name )
    : super( stream_name, sensor_name )
{
}


dds_object_detection_stream::dds_object_detection_stream( std::string const & stream_name,
                                                          std::string const & sensor_name )
    : super( stream_name, sensor_name )
{
}


}  // namespace realdds
