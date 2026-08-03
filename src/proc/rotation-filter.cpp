// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#include <librealsense2/hpp/rs_sensor.hpp>
#include "option.h"
#include "stream.h"
#include "core/video.h"
#include "proc/rotation-filter.h"
#include <rsutils/easylogging/easyloggingpp.h>

namespace librealsense {

    const int rotation_min_val = -90;
    const int rotation_max_val = 180;  
    const int rotation_default_val = 0;
    const int rotation_step = 90; 


    rotation_filter::rotation_filter()
        : rotation_filter( std::vector< rs2_stream >{ RS2_STREAM_DEPTH } )
    {
    }

    rotation_filter::rotation_filter( std::vector< rs2_stream > streams_to_rotate )
        : stream_filter_processing_block( "Rotation Filter" )
        , _streams_to_rotate( streams_to_rotate )
        , _control_val( rotation_default_val )
        , _real_width( 0 )
        , _real_height( 0 )
        , _rotated_width( 0 )
        , _rotated_height( 0 )
        , _value( 0 )
    {
        auto rotation_control = std::make_shared< ptr_option< int > >( rotation_min_val,
                                                                       rotation_max_val,
                                                                       rotation_step,
                                                                       rotation_default_val,
                                                                       &_control_val,
                                                                       "Rotation angle" );

        auto weak_rotation_control = std::weak_ptr< ptr_option< int > >( rotation_control );
        rotation_control->on_set(
            [this, weak_rotation_control]( float val )
            {
                auto strong_rotation_control = weak_rotation_control.lock();
                if( ! strong_rotation_control )
                    return;

                if( ! strong_rotation_control->is_valid( val ) )
                    throw invalid_value_exception( rsutils::string::from()
                                                   << "Unsupported rotation scale " << val << " is out of range." );

                _value = val;
            } );

        register_option( RS2_OPTION_ROTATION, rotation_control );
    }

    rs2::frame rotation_filter::process_frame(const rs2::frame_source& source, const rs2::frame& f)
    {
        if( _value == rotation_default_val || _streams_to_rotate.empty() )
            return f;

        // Copying to a local variable to avoid locking.
        float local_value = _value;
        auto src = f.as< rs2::video_frame >();
        rs2::stream_profile profile = f.get_profile();

        update_output_profile( f, local_value );

        // Create the stream key to get the matching target profile.
        auto stream_type = profile.stream_type();
        auto stream_index = profile.stream_index();
        std::pair< rs2_stream, int > stream_key( stream_type, stream_index );

        // Retrieve the per-stream target profile.
        rs2::stream_profile target_profile = _target_stream_profiles[stream_key];

        rs2_extension tgt_type;
        if( stream_type == RS2_STREAM_COLOR || stream_type == RS2_STREAM_INFRARED )
            tgt_type = RS2_EXTENSION_VIDEO_FRAME;
        else
            tgt_type = f.is< rs2::disparity_frame >() ? RS2_EXTENSION_DISPARITY_FRAME : RS2_EXTENSION_DEPTH_FRAME;

        if (auto tgt = prepare_target_frame( f, source, target_profile, tgt_type ))
        {
            auto format = profile.format();
            if( format == RS2_FORMAT_YUYV && ( local_value == 90 || local_value == -90 ) )
            {
                LOG_ERROR( "Rotating YUYV format is disabled for 90 or -90 degrees" );
                return f;
            }

            if( format == RS2_FORMAT_YUYV )
            {
                rotate_YUYV_frame( static_cast< uint8_t * >( const_cast< void * >( tgt.get_data() ) ),
                                   static_cast< const uint8_t * >( src.get_data() ),
                                   src.get_width(),
                                   src.get_height() );
            }
            else
            {
                rotate_frame( static_cast< uint8_t * >( const_cast< void * >( tgt.get_data() ) ),
                              static_cast< const uint8_t * >( src.get_data() ),
                              src.get_width(),
                              src.get_height(),
                              src.get_bytes_per_pixel(),
                              local_value );
            }
            return tgt;
        }
        return f;
    }
    
    void rotation_filter::update_output_profile(const rs2::frame & f, float & value)
    {
        // Get the current source profile for this frame
        rs2::stream_profile current_source_profile = f.get_profile();
        auto stream_type = current_source_profile.stream_type();
        auto stream_index = current_source_profile.stream_index();
        std::pair< rs2_stream, int > stream_key( stream_type, stream_index );

        // Initialize the rotation value to 0 if not already stored
        if( _last_rotation_values.find( stream_key ) == _last_rotation_values.end() )
        {
            _last_rotation_values[stream_key] = 0.0f;
        }
        float last_rotation = _last_rotation_values[stream_key];

        // Check if we've already processed this same source profile and rotation value.
        if( _source_stream_profiles.find( stream_key ) != _source_stream_profiles.end()
            && current_source_profile.get() == _source_stream_profiles[stream_key].get() && value == last_rotation )
        {
            return;  // No change needed for this stream.
        }

        // Update the source stream profile map for this stream key.
        _source_stream_profiles[stream_key] = current_source_profile;

        // Clone the current source profile into a target profile for further processing.
        rs2::stream_profile target_profile
            = current_source_profile.clone( stream_type, stream_index, current_source_profile.format() );

        auto src_vspi = dynamic_cast< video_stream_profile_interface * >( current_source_profile.get()->profile );
        if( ! src_vspi )
            throw std::runtime_error( "Stream profile interface is not video stream profile interface" );
        auto tgt_vspi = dynamic_cast< video_stream_profile_interface * >( target_profile.get()->profile );
        if( ! tgt_vspi )
            throw std::runtime_error( "Profile is not video stream profile" );

        // Some profiles (e.g. unrectified Y16/Y12I infrared) have no usable intrinsics; get_intrinsics() can
        // fail in more than one way (never registered, or the calibration lookup itself rejects the stream),
        // so treat any failure the same: fall back to the profile's own dimensions and skip target intrinsics.
        rs2_intrinsics src_intrin{};
        bool has_intrinsics = true;
        try
        {
            src_intrin = src_vspi->get_intrinsics();
        }
        catch( const std::exception & )
        {
            has_intrinsics = false;
        }

        int src_width = has_intrinsics ? src_intrin.width : src_vspi->get_width();
        int src_height = has_intrinsics ? src_intrin.height : src_vspi->get_height();

        int rotated_width = 0;
        int rotated_height = 0;
        if( value == 90 || value == -90 )
        {
            rotated_width = src_height;
            rotated_height = src_width;
        }
        else if( value == 180 )
        {
            rotated_width = src_width;
            rotated_height = src_height;
        }
        else { throw std::invalid_argument( "Unsupported rotation angle" ); }

        tgt_vspi->set_dims( rotated_width, rotated_height );

        if( has_intrinsics )
        {
            // The target profile was cloned from the source, so its intrinsics start identical; only the
            // fields affected by rotation need adjusting.
            rs2_intrinsics tgt_intrin = src_intrin;
            if( value == 90 )
            {
                tgt_intrin.fx = src_intrin.fy;
                tgt_intrin.fy = src_intrin.fx;
                tgt_intrin.ppx = src_intrin.height - src_intrin.ppy;
                tgt_intrin.ppy = src_intrin.ppx;
            }
            else if( value == -90 )
            {
                tgt_intrin.fx = src_intrin.fy;
                tgt_intrin.fy = src_intrin.fx;
                tgt_intrin.ppx = src_intrin.ppy;
                tgt_intrin.ppy = src_intrin.width - src_intrin.ppx;
            }
            else  // 180
            {
                tgt_intrin.fx = src_intrin.fx;
                tgt_intrin.fy = src_intrin.fy;
                tgt_intrin.ppx = src_intrin.width - src_intrin.ppx;
                tgt_intrin.ppy = src_intrin.height - src_intrin.ppy;
            }
            tgt_intrin.width = rotated_width;
            tgt_intrin.height = rotated_height;
            tgt_vspi->set_intrinsics( [tgt_intrin]() { return tgt_intrin; } );
        }

        _last_rotation_values[stream_key] = value;
        _target_stream_profiles[stream_key] = target_profile;
    }

    rs2::frame rotation_filter::prepare_target_frame( const rs2::frame & f,
                                                      const rs2::frame_source & source,
                                                      const rs2::stream_profile & target_profile,
                                                      rs2_extension tgt_type )
    {
        auto vf = f.as<rs2::video_frame>();
        if( ! vf )
            throw std::runtime_error( "Failed to cast frame to video_frame" );
        std::pair< rs2_stream, int > stream_key( f.get_profile().stream_type(), f.get_profile().stream_index() );

        int out_width = 0, out_height = 0;
        auto video_profile = dynamic_cast< video_stream_profile_interface * >( target_profile.get()->profile );
        if( ! video_profile )
            throw std::runtime_error( "Target profile is not a video stream profile interface" );

        out_width = video_profile->get_width();
        out_height = video_profile->get_height();

        auto ret = source.allocate_video_frame( target_profile,
                                                f,
                                                vf.get_bytes_per_pixel(),
                                                out_width,
                                                out_height,
                                                out_width * vf.get_bytes_per_pixel(),
                                                tgt_type );
        return ret;
    }

    void rotation_filter::rotate_frame( uint8_t * const out, const uint8_t * source, int width, int height, int bpp, float & value )
    {
        if( value != 90 && value != -90 && value != 180 )
        {
            throw std::invalid_argument( "Invalid rotation angle. Only 90, -90, and 180 degrees are supported." );
        }

        // Define output dimensions
        int width_out = ( value == 90 || value == -90 ) ? height : width;    // rotate by 180 will keep the values as is
        int height_out = ( value == 90 || value == -90 ) ? width : height;  // rotate by 180 will keep the values as is

        // Perform rotation
        for( int i = 0; i < height; ++i )
        {
            for( int j = 0; j < width; ++j )
            {
                // Calculate source index
                size_t src_index = ( i * width + j ) * bpp;

                // Determine output index based on rotation angle
                size_t out_index;
                if( value == 90 )
                {
                    out_index = ( j * height + ( height - i - 1 ) ) * bpp;
                }
                else if( value == -90 )
                {
                    out_index = ( ( width - j - 1 ) * height + i ) * bpp;
                }
                else  // 180 degrees
                {
                    out_index = ( ( height - i - 1 ) * width + ( width - j - 1 ) ) * bpp;
                }

                // Copy pixel data from source to destination
                std::memcpy( &out[out_index], &source[src_index], bpp );
            }
        }
    }


    void rotation_filter::rotate_YUYV_frame( uint8_t * const out, const uint8_t * source, int width, int height ) 
    {
        int frame_size = width * height * 2;  // YUYV has 2 bytes per pixel
        for( int i = 0; i < frame_size; i += 4 )
        {
            // Find the corresponding flipped position in the output
            int flipped_index = frame_size - 4 - i;

            // Copy YUYV pixels in reverse order to rotate 180 degrees
            out[flipped_index] = source[i];          // Y1
            out[flipped_index + 1] = source[i + 1];  // U
            out[flipped_index + 2] = source[i + 2];  // Y2
            out[flipped_index + 3] = source[i + 3];  // V
        }
    }

    void rotation_filter::set_streams_to_rotate(const std::vector<rs2_stream>& streams_to_rotate)
    {
        _streams_to_rotate = streams_to_rotate;
    }

    bool rotation_filter::should_process( const rs2::frame & frame )
    {
        if( ! frame || frame.is< rs2::frameset >() )
            return false;
        auto type = frame.get_profile().stream_type();

        for( auto & stream_type : _streams_to_rotate ){
            if( stream_type == type )
                return true;
        }
        return false;
    }
       
}

