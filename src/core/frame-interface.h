// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.
#pragma once

#include <librealsense2/h/rs_frame.h>
#include <memory>
#include <cstdint> // for uint8_t
#include <iosfwd>


namespace librealsense {


class stream_profile_interface;
class archive_interface;
class sensor_interface;
struct frame_header;
class frame_continuation;


class frame_interface
{
public:
    virtual frame_header const & get_header() const = 0;

    virtual bool find_metadata( rs2_frame_metadata_value, rs2_metadata_type * p_output_value ) const = 0;
    virtual int get_frame_data_size() const = 0;
    virtual const uint8_t * get_frame_data() const = 0;
    // Return a GPU device pointer for the frame, uploading it if needed. Zero-copy when the
    // frame is already GPU-mapped (sets copied=false); otherwise the SDK copies it to a cached
    // device buffer (copied=true). Returns nullptr on non-CUDA builds. Default: no GPU data.
    virtual const void * get_gpu_data_or_upload( bool * copied ) { if( copied ) *copied = false; return nullptr; }
    virtual rs2_time_t get_frame_timestamp() const = 0;
    virtual rs2_timestamp_domain get_frame_timestamp_domain() const = 0;
    virtual void set_timestamp( double new_ts ) = 0;
    virtual unsigned long long get_frame_number() const = 0;

    virtual void set_timestamp_domain( rs2_timestamp_domain timestamp_domain ) = 0;
    virtual rs2_time_t get_frame_system_time() const = 0;  // TIME_OF_ARRIVAL
    virtual std::shared_ptr< stream_profile_interface > get_stream() const = 0;
    virtual void set_stream( std::shared_ptr< stream_profile_interface > sp ) = 0;

    virtual std::shared_ptr< sensor_interface > get_sensor() const = 0;
    virtual void set_sensor( std::shared_ptr< sensor_interface > ) = 0;

    virtual void acquire() = 0;
    virtual void release() = 0;
    virtual frame_interface * publish( std::shared_ptr< archive_interface > new_owner ) = 0;
    virtual void unpublish() = 0;
    virtual void attach_continuation( frame_continuation && continuation ) = 0;
    virtual void disable_continuation() = 0;
    virtual archive_interface * get_owner() const = 0;

    virtual void mark_fixed() = 0;
    virtual bool is_fixed() const = 0;
    virtual void set_blocking( bool state ) = 0;
    virtual bool is_blocking() const = 0;

    virtual void keep() = 0;

    virtual ~frame_interface() = default;
};


std::ostream & operator<<( std::ostream &, const frame_interface & f );


}  // namespace librealsense
