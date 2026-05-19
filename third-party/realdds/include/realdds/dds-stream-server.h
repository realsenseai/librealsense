// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#pragma once

#include <realdds/dds-stream-profile.h>
#include <realdds/dds-stream-base.h>
#include <realdds/dds-trinsics.h>

#include <memory>
#include <string>
#include <set>
#include <functional>


namespace realdds {
namespace topics {
class image_msg;
class imu_msg;
class string_msg;
}


class dds_topic;
class dds_topic_writer;
class dds_publisher;
class dds_stream_profile;


struct image_header
{
    dds_video_encoding encoding;
    int height = -1;
    int width = -1;

    bool is_valid() const { return width != -1 && height != -1; }
    void invalidate() { width = -1; height = -1; }
};


// Distributes stream images into a dedicated topic
// 
// This is a base class: you need to specify the type of stream via the instantiation of a video_stream_server, etc.
//
class dds_stream_server : public dds_stream_base
{
protected:
    dds_stream_server( std::string const & stream_name, std::string const & sensor_name );

public:
    virtual ~dds_stream_server();

    bool is_open() const override { return !! _writer; }
    virtual void open( std::string const & topic_name, std::shared_ptr< dds_publisher > const & ) = 0;
    void close();

    bool is_streaming() const override { return _streaming; }
    virtual void stop_streaming();

    std::shared_ptr< dds_topic > const & get_topic() const override;

    // We don't know how to actually stream -- someone else calls publish_image. So we provide an easy callback
    // to let our owner know if streaming needs to start or end based on the number of readers we are matched to:
    typedef std::function< void( std::shared_ptr< dds_stream_server > const &, int n_readers ) >
        readers_changed_callback;
    void on_readers_changed( readers_changed_callback callback ) { _on_readers_changed = std::move( callback ); }

protected:
    std::shared_ptr< dds_topic_writer > _writer;
    readers_changed_callback _on_readers_changed;
    bool _streaming = false;

    // Called at the end of open(), when the _writer has been initialized. Override to provide custom QOS etc...
    virtual void run_stream();

    void start_streaming();
};


class dds_video_stream_server : public dds_stream_server
{
    typedef dds_stream_server super;

public:
    dds_video_stream_server( std::string const & stream_name, std::string const & sensor_name );

    void open( std::string const & topic_name, std::shared_ptr< dds_publisher > const & ) override;

    void set_intrinsics( const std::set< video_intrinsics > & intrinsics ) { _intrinsics = intrinsics; }
    const std::set< video_intrinsics > & get_intrinsics() const { return _intrinsics; }

    void start_streaming( const image_header & );
    void stop_streaming() override;
    image_header const & get_image_header() const { return _image_header; }

    virtual void publish_image( topics::image_msg & );

private:
    void check_profile( std::shared_ptr< dds_stream_profile > const & ) const override;

    std::set< video_intrinsics > _intrinsics;
    image_header _image_header;
};


class dds_depth_stream_server : public dds_video_stream_server
{
    typedef dds_video_stream_server super;

public:
    dds_depth_stream_server( std::string const & stream_name, std::string const & sensor_name );

    char const * type_string() const override { return "depth"; }
};


class dds_ir_stream_server : public dds_video_stream_server
{
    typedef dds_video_stream_server super;

public:
    dds_ir_stream_server( std::string const & stream_name, std::string const & sensor_name );

    char const * type_string() const override { return "ir"; }
};


class dds_color_stream_server : public dds_video_stream_server
{
    typedef dds_video_stream_server super;

public:
    dds_color_stream_server( std::string const & stream_name, std::string const & sensor_name );

    char const * type_string() const override { return "color"; }
};


class dds_confidence_stream_server : public dds_video_stream_server
{
    typedef dds_video_stream_server super;

public:
    dds_confidence_stream_server( std::string const & stream_name, std::string const & sensor_name );

    char const * type_string() const override { return "confidence"; }
};


// Combined Gyro and Accel stream (otherwise known as "IMU")
//
class dds_motion_stream_server : public dds_stream_server
{
    typedef dds_stream_server super;

public:
    dds_motion_stream_server( std::string const & stream_name, std::string const & sensor_name );

    void open( std::string const & topic_name, std::shared_ptr< dds_publisher > const & ) override;

    void set_accel_intrinsics( const motion_intrinsics & intrinsics ) { _accel_intrinsics = intrinsics; }
    void set_gyro_intrinsics( const motion_intrinsics & intrinsics ) { _gyro_intrinsics = intrinsics; }
    const motion_intrinsics & get_accel_intrinsics() const { return _accel_intrinsics; }
    const motion_intrinsics & get_gyro_intrinsics() const { return _gyro_intrinsics; }

    void start_streaming();
    virtual void publish_motion( topics::imu_msg && );

    char const * type_string() const override { return "motion"; }

private:
    void check_profile( std::shared_ptr< dds_stream_profile > const & ) const override;

    motion_intrinsics _accel_intrinsics;
    motion_intrinsics _gyro_intrinsics;
};



class dds_inference_stream_server : public dds_stream_server
{
    typedef dds_stream_server super;

public:
    dds_inference_stream_server( std::string const & stream_name, std::string const & sensor_name );

    void open( std::string const & topic_name, std::shared_ptr< dds_publisher > const & ) override;

    void start_streaming();
    virtual void publish_inference( topics::string_msg const & msg );

    char const * type_string() const override { return "inference"; }

private:
    void check_profile( std::shared_ptr< dds_stream_profile > const & ) const override;
};


class dds_object_detection_stream_server : public dds_inference_stream_server
{
    typedef dds_inference_stream_server super;

public:
    dds_object_detection_stream_server( std::string const & stream_name, std::string const & sensor_name );

    char const * type_string() const override { return "object_detection"; }
};


}  // namespace realdds
