// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "rum-hooks.h"
#include "rum-collector.h"

#include "core/device-interface.h"          // device_interface, supports_info/get_info
#include "core/video.h"                      // stream_profile_interface, video_stream_profile_interface
#include "core/sensor-interface.h"           // sensor_interface
#include "core/options-interface.h"          // options_interface
#include "core/enum-helpers.h"               // get_string( rs2_stream / rs2_format / rs2_option / rs2_notification_category )

#include <string>
#include <set>


// The single place ENABLED_STATS is checked. Each hook leads with RETURN_IF_NO_RUM, so when stats
// are compiled out the collector is never touched (no file I/O) and call sites need no guard.
#ifdef ENABLED_STATS
#define RETURN_IF_NO_RUM ( (void)0 )
#else
#define RETURN_IF_NO_RUM return
#endif


namespace librealsense {
namespace rum {
namespace hooks {


void on_device( device_interface & dev )
{
    RETURN_IF_NO_RUM;
    auto info = [&]( rs2_camera_info i ) -> std::string {
        return dev.supports_info( i ) ? dev.get_info( i ) : std::string();
    };
    rum_collector::instance().record_device( info( RS2_CAMERA_INFO_NAME ),
                                             info( RS2_CAMERA_INFO_FIRMWARE_VERSION ),
                                             info( RS2_CAMERA_INFO_CONNECTION_TYPE ),
                                             info( RS2_CAMERA_INFO_MIPI_DRIVER_VERSION ) );
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
    RETURN_IF_NO_RUM;
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
    RETURN_IF_NO_RUM;
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
    RETURN_IF_NO_RUM;
    if( value == default_value )
        return;
    // Only record options set on an actual device sensor; processing-block options
    // (set internally by apps like the viewer) are not user device-tuning.
    if( dynamic_cast< sensor_interface * >( &target ) == nullptr )
        return;
    rum_collector::instance().record_option_change( get_string( option ), value );
}


// The known SDK post-processing filters. Everything else that passes through the
// processing_block base ctor (syncer, format converters, custom callback blocks,
// alignment, etc.) is internal plumbing and intentionally not recorded.
static bool is_tracked_filter( std::string const & name )
{
    static const std::set< std::string > filters = {
        "Decimation Filter",
        "Spatial Filter",
        "Temporal Filter",
        "Hole Filling Filter",
        "Threshold Filter",
        "Disparity to Depth",
        "Depth to Disparity",
        "HDR Merge",
        "Rotation Filter",
        "Filter By Sequence id",
    };
    return filters.count( name ) != 0;
}


void on_filter( std::string const & name )
{
    RETURN_IF_NO_RUM;
    if( is_tracked_filter( name ) )
        rum_collector::instance().record_filter( name );
}


void on_notification( rs2_notification_category category )
{
    RETURN_IF_NO_RUM;
    rum_collector::instance().record_notification( get_string( category ) );
}


void on_context_closed() noexcept
{
    RETURN_IF_NO_RUM;
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
