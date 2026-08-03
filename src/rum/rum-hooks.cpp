// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "rum-hooks.h"
#include "rum-collector.h"

#include "core/device-interface.h"          // device_interface, supports_info/get_info
#include "core/video.h"                      // stream_profile_interface, video_stream_profile_interface
#include "core/sensor-interface.h"           // sensor_interface, get_recommended_processing_blocks
#include "core/processing-block-interface.h" // processing_block_interface (recommended-filter names)
#include "core/options-interface.h"          // options_interface
#include "core/enum-helpers.h"               // get_string( rs2_stream / rs2_format / rs2_option / rs2_notification_category )

#include <string>


#ifdef ENABLE_STATS


namespace librealsense {
namespace rum {
namespace hooks {


void on_device( device_interface & dev )
{
    auto info = [&]( rs2_camera_info i ) -> std::string {
        return dev.supports_info( i ) ? dev.get_info( i ) : std::string();
    };
    rum_collector::instance().record_device( info( RS2_CAMERA_INFO_NAME ),
                                             info( RS2_CAMERA_INFO_FIRMWARE_VERSION ),
                                             info( RS2_CAMERA_INFO_CONNECTION_TYPE ),
                                             info( RS2_CAMERA_INFO_MIPI_DRIVER_VERSION ) );

    // Mark this device's recommended post-processing filters as the ones worth recording, so
    // record_filter ignores viewer/internal blocks (colorizer/pointcloud/align, format converters).
    for( size_t i = 0; i < dev.get_sensors_count(); ++i )
        for( auto const & block : dev.get_sensor( i ).get_recommended_processing_blocks() )
            if( block && block->supports_info( RS2_CAMERA_INFO_NAME ) )
                rum_collector::instance().add_recommended_filter( block->get_info( RS2_CAMERA_INFO_NAME ) );
}


namespace {

// Extract the (type, format, resolution, fps) stream-tally key from a profile.
void stream_key_of( std::shared_ptr< stream_profile_interface > const & p,
                    std::string & type, std::string & format, std::string & resolution, int & fps )
{
    type = get_string( p->get_stream_type() );
    format = get_string( p->get_format() );
    fps = static_cast< int >( p->get_framerate() );
    resolution.clear();
    if( auto vp = std::dynamic_pointer_cast< video_stream_profile_interface >( p ) )
        resolution = std::to_string( vp->get_width() ) + "x" + std::to_string( vp->get_height() );
}

}  // namespace


void on_open( std::vector< std::shared_ptr< stream_profile_interface > > const & profiles )
{
    for( auto const & p : profiles )
    {
        if( ! p )
            continue;
        std::string type, format, resolution;
        int fps;
        stream_key_of( p, type, format, resolution, fps );
        rum_collector::instance().record_stream( type, format, resolution, fps );
    }
}


void on_stream_duration( std::vector< std::shared_ptr< stream_profile_interface > > const & profiles, double seconds )
{
    for( auto const & p : profiles )
    {
        if( ! p )
            continue;
        std::string type, format, resolution;
        int fps;
        stream_key_of( p, type, format, resolution, fps );
        rum_collector::instance().record_stream_duration( type, format, resolution, fps, seconds );
    }
}


void on_set_option( options_interface & target, rs2_option option, float value, float default_value )
{
    if( value == default_value )
        return;
    // Only record options set on a device sensor; processing-block options are set
    // internally, not user tuning.
    if( dynamic_cast< sensor_interface * >( &target ) == nullptr )
        return;
    rum_collector::instance().record_option_change( get_string( option ), value );
}


void on_filter( std::string const & name )
{
    // Record any processing block that processes a frame; narrowing to recommended filters
    // is left to the consumer.
    rum_collector::instance().record_filter( name );
}


void on_notification( rs2_notification_category category )
{
    rum_collector::instance().record_notification( get_string( category ) );
}


void on_context_closed() noexcept
{
    try
    {
        rum_collector::instance().flush();
    }
    catch( ... )
    {
    }
}


}  // namespace hooks
}  // namespace rum
}  // namespace librealsense

#endif  // ENABLE_STATS
