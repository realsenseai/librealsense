// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "rum-hooks.h"
#include "rum-collector.h"

#include "core/device-interface.h"          // device_interface, supports_info/get_info
#include "core/video.h"                      // stream_profile_interface, video_stream_profile_interface
#include "core/sensor-interface.h"           // sensor_interface, get_recommended_processing_blocks, get_device
#include "core/processing-block-interface.h" // processing_block_interface (recommended-filter names)
#include "core/options-interface.h"          // options_interface
#include "core/enum-helpers.h"               // get_string( rs2_stream / rs2_format / rs2_option / rs2_notification_category )

#include "core/frame-interface.h"           // frame_interface::get_sensor (on_filter)

#include <string>


#ifdef ENABLE_STATS


namespace librealsense {
namespace rum {
namespace hooks {


namespace {

// Read a device info string, or "" if unsupported.
std::string dev_info( device_interface & dev, rs2_camera_info i )
{
    return dev.supports_info( i ) ? dev.get_info( i ) : std::string();
}

// Per-device key used to nest everything: "<name>-<connection>" (transports stay distinct).
// Connection is also stored as an explicit property so the consumer needn't parse the key.
std::string device_key_of( device_interface & dev )
{
    return dev_info( dev, RS2_CAMERA_INFO_NAME ) + "-" + dev_info( dev, RS2_CAMERA_INFO_CONNECTION_TYPE );
}

// Stream label used to nest streams/filters: "<type>-<format>-<WxH>@<fps>". Built the same way
// from an internal profile (on_open) and from a frame's profile (on_filter) so the keys match.
std::string stream_label( rs2_stream type, rs2_format format, int width, int height, int fps )
{
    std::string resolution = ( width > 0 && height > 0 )
                           ? std::to_string( width ) + "x" + std::to_string( height )
                           : std::string();
    return std::string( get_string( type ) ) + "-" + get_string( format ) + "-" + resolution + "@" + std::to_string( fps );
}

}  // namespace


void on_device( device_interface & dev )
{
    // Serial is read only to count distinct units in memory (see record_device); never uploaded.
    rum_collector::instance().record_device( device_key_of( dev ),
                                             dev_info( dev, RS2_CAMERA_INFO_CONNECTION_TYPE ),
                                             dev_info( dev, RS2_CAMERA_INFO_FIRMWARE_VERSION ),
                                             dev_info( dev, RS2_CAMERA_INFO_MIPI_DRIVER_VERSION ),
                                             dev_info( dev, RS2_CAMERA_INFO_SERIAL_NUMBER ) );

    // Mark this device's recommended post-processing filters as the ones worth recording, so
    // record_filter ignores viewer/internal blocks (colorizer/pointcloud/align, format converters).
    for( size_t i = 0; i < dev.get_sensors_count(); ++i )
        for( auto const & block : dev.get_sensor( i ).get_recommended_processing_blocks() )
            if( block && block->supports_info( RS2_CAMERA_INFO_NAME ) )
                rum_collector::instance().add_recommended_filter( block->get_info( RS2_CAMERA_INFO_NAME ) );
}


namespace {

// Build the stream label from an internal profile.
std::string label_of( std::shared_ptr< stream_profile_interface > const & p )
{
    int width = 0, height = 0;
    if( auto vp = std::dynamic_pointer_cast< video_stream_profile_interface >( p ) )
    {
        width = vp->get_width();
        height = vp->get_height();
    }
    return stream_label( p->get_stream_type(), p->get_format(), width, height, static_cast< int >( p->get_framerate() ) );
}

}  // namespace


void on_open( device_interface & dev, std::vector< std::shared_ptr< stream_profile_interface > > const & profiles )
{
    auto key = device_key_of( dev );
    for( auto const & p : profiles )
        if( p )
            rum_collector::instance().record_stream( key, label_of( p ) );
}


void on_stream_duration( device_interface & dev, std::vector< std::shared_ptr< stream_profile_interface > > const & profiles, double seconds )
{
    auto key = device_key_of( dev );
    for( auto const & p : profiles )
        if( p )
            rum_collector::instance().record_stream_duration( key, label_of( p ), seconds );
}


void on_set_option( options_interface & target, rs2_option option, float value, float default_value )
{
    if( value == default_value )
        return;
    // Only record options set on a device sensor; processing-block options are internal, not tuning.
    auto sensor = dynamic_cast< sensor_interface * >( &target );
    if( ! sensor )
        return;
    rum_collector::instance().record_option_change( device_key_of( sensor->get_device() ), get_string( option ), value );
}


void on_filter( std::string const & name, frame_interface & f )
{
    // Attribute to the device via the frame's sensor. Filter-output frames keep the original
    // sensor, so this resolves even for chained filters.
    auto sensor = f.get_sensor();
    if( ! sensor )
        return;
    rum_collector::instance().record_filter( device_key_of( sensor->get_device() ), name );
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
