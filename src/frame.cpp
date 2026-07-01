// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

#include "core/depth-frame.h"
#include "composite-frame.h"
#include "core/stream-profile-interface.h"

#include "metadata-parser.h"
#include "core/enum-helpers.h"
#ifdef RS2_USE_CUDA
#include "cuda/cuda-frame-memory.h"
#endif

#include <rsutils/string/from.h>


namespace librealsense {


std::ostream & operator<<( std::ostream & s, const frame_interface & f )
{
    if( !&f )
    {
        s << "[null]";
    }
    else
    {
        auto composite = dynamic_cast<const composite_frame *>(&f);
        if( composite )
        {
            s << "[";
            for( int i = 0; i < composite->get_embedded_frames_count(); i++ )
            {
                s << *composite->get_frame( i );
            }
            s << "]";
        }
        else
        {
            s << "[" << get_abbr_string( f.get_stream()->get_stream_type() );
            s << f.get_stream()->get_unique_id();
            s << " " << f.get_header();
            s << "]";
        }
    }
    return s;
}


std::ostream & operator<<( std::ostream & os, frame_header const & header )
{
    os << "#" << header.frame_number;
    os << " @" << rsutils::string::from( header.timestamp );
    if( header.timestamp_domain != RS2_TIMESTAMP_DOMAIN_HARDWARE_CLOCK )
        os << "/" << rs2_timestamp_domain_to_string( header.timestamp_domain );
    return os;
}


std::ostream & operator<<( std::ostream & os, frame_holder const & f )
{
    return os << *f.frame;
}



archive_interface * frame::get_owner() const
{
    return owner.get();
}

std::shared_ptr< sensor_interface > frame::get_sensor() const
{
    auto res = sensor.lock();
    if( ! res )
    {
        auto archive = get_owner();
        if( archive )
            return archive->get_sensor();
    }
    return res;
}
void frame::set_sensor( std::shared_ptr< sensor_interface > s )
{
    sensor = s;
}

void frame::release()
{
    if( ref_count.fetch_sub( 1 ) == 1 && owner )
    {
        unpublish();
        on_release();
        owner->unpublish_frame( this );
    }
}

void frame::keep()
{
    if( ! _kept.exchange( true ))
    {
        if (owner)
            owner->keep_frame( this );
    }
}

frame_interface * frame::publish( std::shared_ptr< archive_interface > new_owner )
{
    owner = new_owner;
    _kept = false;
    return owner->publish_frame( this );
}

bool frame::find_metadata( rs2_frame_metadata_value frame_metadata, rs2_metadata_type * p_value ) const
{
    if( ! metadata_parsers )
        return false;
    auto parsers = metadata_parsers->equal_range( frame_metadata );

    bool value_retrieved = false;
    for( auto it = parsers.first; it != parsers.second; ++it )
        if( it->second->find( *this, p_value ) )
            value_retrieved = true;

    return value_retrieved;
}

int frame::get_frame_data_size() const
{
    return (int)data.size();
}

const uint8_t * frame::get_frame_data() const
{
    const uint8_t * frame_data = data.data();

    if( on_release.get_data() )
    {
        frame_data = static_cast< const uint8_t * >( on_release.get_data() );
    }

    return frame_data;
}

const void * frame::get_gpu_data_or_upload( bool * copied )
{
#ifdef RS2_USE_CUDA
  #ifdef RS2_USE_CUDA_ZEROCOPY
    // True zero-copy: the frame is already GPU-mapped, no copy needed.
    if( void * z = rs_frame_zc_device_ptr( get_frame_data() ) )
    {
        if( copied ) *copied = false;
        return z;
    }
  #endif
    // Otherwise upload to the frame's cached device buffer (a real, SDK-managed copy).
    if( void * dev = rs_frame_gpu_upload( &_gpu_upload_buffer, &_gpu_upload_capacity,
                                          get_frame_data(), static_cast< size_t >( get_frame_data_size() ) ) )
    {
        if( copied ) *copied = true;
        return dev;
    }
#endif
    if( copied ) *copied = false;
    return nullptr;
}

rs2_timestamp_domain frame::get_frame_timestamp_domain() const
{
    return additional_data.timestamp_domain;
}

rs2_time_t frame::get_frame_timestamp() const
{
    return additional_data.timestamp;
}

unsigned long long frame::get_frame_number() const
{
    return additional_data.frame_number;
}

double frame::calc_actual_fps() const
{
    auto const dt = ( additional_data.timestamp - additional_data.last_timestamp );
    if( dt > 0. && additional_data.frame_number > additional_data.last_frame_number )
    {
        auto const n_frames = additional_data.frame_number - additional_data.last_frame_number;
        return 1000. * n_frames / dt;
    }

    return 0.;  // Unknown actual FPS
}

rs2_time_t frame::get_frame_system_time() const
{
    return additional_data.system_time;
}

float depth_frame::get_distance( int x, int y ) const
{
    // If this frame does not itself contain Z16 depth data,
    // fall back to the original frame it was created from
    if( _original && get_stream()->get_format() != RS2_FORMAT_Z16 )
        return ( (depth_frame *)_original.frame )->get_distance( x, y );

    uint64_t pixel = 0;
    switch( get_bpp() / 8 )  // bits per pixel
    {
    case 1:
        pixel = get_frame_data()[y * get_width() + x];
        break;
    case 2:
        pixel = reinterpret_cast< const uint16_t * >( get_frame_data() )[y * get_width() + x];
        break;
    case 4:
        pixel = reinterpret_cast< const uint32_t * >( get_frame_data() )[y * get_width() + x];
        break;
    case 8:
        pixel = reinterpret_cast< const uint64_t * >( get_frame_data() )[y * get_width() + x];
        break;
    default:
        throw std::runtime_error( rsutils::string::from()
                                  << "Unrecognized depth format " << int( get_bpp() / 8 ) << " bytes per pixel" );
    }

    return pixel * get_units();
}


}  // namespace librealsense
