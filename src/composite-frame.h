// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

#pragma once

#include "frame.h"

namespace librealsense {
class composite_frame : public frame
{
public:
    composite_frame()
        : frame()
    {
    }

    frame_interface * get_frame( int i ) const { return _frames[i]; }

    frame_interface ** get_frames() { return _frames.data(); }
    size_t storage_size() const { return _frames.size() * sizeof( frame_interface * ); }
    void resize_storage( size_t bytes ) { _frames.resize( bytes / sizeof( frame_interface * ) ); }

    const frame_interface * first() const { return get_frame( 0 ); }
    frame_interface * first() { return get_frame( 0 ); }

    void keep() override
    {
        for( auto * child : _frames )
            if( child )
                child->keep();
        frame::keep();
    }

    size_t get_embedded_frames_count() const { return _frames.size(); }

    // In the next section we make the composite frame "look and feel" like the first of its
    // children
    frame_header const & get_header() const override { return first()->get_header(); }
    bool find_metadata( rs2_frame_metadata_value frame_metadata, rs2_metadata_type * p_output_value ) const override
    {
        return first()->find_metadata( frame_metadata, p_output_value );
    }
    int get_frame_data_size() const override { return first()->get_frame_data_size(); }
    const uint8_t * get_frame_data() const override { return first()->get_frame_data(); }
    rs2_time_t get_frame_timestamp() const override { return first()->get_frame_timestamp(); }
    rs2_timestamp_domain get_frame_timestamp_domain() const override
    {
        return first()->get_frame_timestamp_domain();
    }
    unsigned long long get_frame_number() const override
    {
        if( first() )
            return first()->get_frame_number();
        else
            return frame::get_frame_number();
    }
    rs2_time_t get_frame_system_time() const override { return first()->get_frame_system_time(); }
    std::shared_ptr< sensor_interface > get_sensor() const override
    {
        return first()->get_sensor();
    }

private:
    // Only the CPU accesses the child pointers. Pixel buffers belong to the children.
    std::vector< frame_interface * > _frames;
};

MAP_EXTENSION( RS2_EXTENSION_COMPOSITE_FRAME, librealsense::composite_frame );
}  // namespace librealsense
