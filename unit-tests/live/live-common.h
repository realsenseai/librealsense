// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

#include <unit-tests/test.h>
#include <librealsense2/rs.hpp>
#include <condition_variable>
#include <src/hw-monitor.h>
#include <src/firmware-version.h>

using namespace rs2;


inline std::string repr( rs2::device const & self )
{
    // same as Python device repr() in pyrs_device.cpp
    std::ostringstream ss;
    ss << "<device: ";
    if( ! self )
        ss << "NULL";
    else
    {
        ss << self.get_info( RS2_CAMERA_INFO_NAME );
        if( self.supports( RS2_CAMERA_INFO_SERIAL_NUMBER ) )
            ss << " (S/N: " << self.get_info( RS2_CAMERA_INFO_SERIAL_NUMBER );
        else
            ss << " (FW update id: " << self.get_info( RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID );
        if( self.supports( RS2_CAMERA_INFO_FIRMWARE_VERSION ) )
            ss << " FW: " << self.get_info( RS2_CAMERA_INFO_FIRMWARE_VERSION );
        if( self.supports( RS2_CAMERA_INFO_CAMERA_LOCKED )
            && strcmp( "YES", self.get_info( RS2_CAMERA_INFO_CAMERA_LOCKED ) ) )
            ss << " UNLOCKED";
        if( self.supports( RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR ) )
            ss << " on USB" << self.get_info( RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR );
        ss << ")";
    }
    ss << ">";
    return ss.str();
}


inline std::string repr( rs2::device_list const & list )
{
    std::ostringstream ss;
    ss << '[';
    for( auto && dev : list )
        ss << repr( dev );
    ss << ']';
    return ss.str();
}


inline rs2::device find_first_device_or_exit()
{
    rs2::context ctx( "{\"dds\":false}" );
    rs2::device_list devices_list = ctx.query_devices();
    if( devices_list.size() == 0 )
    {
        std::cout << "No device was found" << std::endl;
        exit( 1 );
    }
    rs2::device dev = devices_list[0];
    test::log.d( "found", repr(dev) );
    return dev;
}

inline rs2::device_list find_devices_by_product_line_or_exit( int product )
{
    rs2::context ctx( "{\"dds\":false}" );
    rs2::device_list devices_list = ctx.query_devices( product );
    if( devices_list.size() == 0 )
    {
        std::cout << "No device of the " << product << " product line was found"
                  << std::endl;
        exit( 1 );
    }
    test::log.d( "found", devices_list.size(), "devices:", repr( devices_list ));
    return devices_list;
}

inline void exit_if_fw_version_is_under( rs2::device & dev, librealsense::firmware_version version )
{
    std::string fw_version;
    REQUIRE_NOTHROW(fw_version = dev.get_info( RS2_CAMERA_INFO_FIRMWARE_VERSION ));

    if (librealsense::firmware_version(fw_version) < version)
    {
        std::cout << "FW version " << fw_version << " is under the minimum requiered FW version "
                  << version << std::endl;
        exit( 0 );
    }
}

// This function returns the first device found that it's name contains the given string,
// if there is no such device it will exit the test.
// Can get as input a full device name or short like "L515/D435..."
inline rs2::device find_first_device_by_name_or_exit( const std::string & dev_name )
{
    rs2::context ctx( "{\"dds\":false}" );
    std::vector< rs2::device > devices_list = ctx.query_devices();

    auto dev_iter
        = std::find_if( devices_list.begin(), devices_list.end(), [dev_name]( rs2::device dev ) {
              return dev.supports( RS2_CAMERA_INFO_NAME )
                  && std::string( dev.get_info( RS2_CAMERA_INFO_NAME ) ).find( dev_name )
                         != std::string::npos;
          } );

    if( dev_iter != devices_list.end() )
    {
        test::log.d( "found", repr( *dev_iter ) );
        return *dev_iter;
    }

    std::cout << "No " << dev_name << " device was found" << std::endl;
    exit( 1 );
}

// Return the first device that supports the input option or exits.
// Can get as input a full device name or short like "L515/D435..."
rs2::depth_sensor find_first_supported_depth_sensor_or_exit(const std::string & dev_name,
    rs2_option opt)
{
    auto dev = find_first_device_by_name_or_exit(dev_name);

    auto ds = dev.first< rs2::depth_sensor >();
    if (!ds.supports(opt))
    {
        std::cout << "Device: " << dev_name << " does not support option: " << rs2_option_to_string(opt) << std::endl;
        exit(0);
    }

    return ds;
}

// Remove the frame's stream (or streams if a frameset) from the list of streams we expect to arrive
// If any stream is unexpected, it is ignored
inline void remove_all_streams_arrived( rs2::frame f,
                                        std::vector< rs2::stream_profile > & expected_streams )
{
    auto remove_stream = [&]() {
        auto it = std::remove_if( expected_streams.begin(),
                                  expected_streams.end(),
                                  [&]( rs2::stream_profile s ) {
                                      return s.stream_type() == f.get_profile().stream_type();
                                  } );


        if( it != expected_streams.end() )
            expected_streams.erase( it );
    };

    if( f.is< rs2::frameset >() )
    {
        auto set = f.as< rs2::frameset >();
        set.foreach_rs( [&]( rs2::frame fr ) { remove_stream(); } );
    }
    else
    {
        remove_stream();
    }
}

inline stream_profile find_default_depth_profile(rs2::depth_sensor depth_sens)
{
    std::vector< stream_profile > stream_profiles;
    REQUIRE_NOTHROW(stream_profiles = depth_sens.get_stream_profiles());

    auto depth_profile
        = std::find_if(stream_profiles.begin(), stream_profiles.end(), [](stream_profile sp) {
        return sp.is_default() && sp.stream_type() == RS2_STREAM_DEPTH;
    });

    REQUIRE(depth_profile != stream_profiles.end());
    return *depth_profile;
}

inline stream_profile find_default_ir_profile(rs2::depth_sensor depth_sens)
{
    std::vector< stream_profile > stream_profiles;
    REQUIRE_NOTHROW(stream_profiles = depth_sens.get_stream_profiles());

    auto ir_profile
        = std::find_if(stream_profiles.begin(), stream_profiles.end(), [](stream_profile sp) {
        return sp.is_default() && sp.stream_type() == RS2_STREAM_INFRARED;
    });

    REQUIRE(ir_profile != stream_profiles.end());
    return *ir_profile;
}

inline stream_profile find_confidence_corresponding_to_depth(rs2::depth_sensor depth_sens,
    stream_profile depth_profile)
{
    std::vector< stream_profile > stream_profiles;
    REQUIRE_NOTHROW(stream_profiles = depth_sens.get_stream_profiles());

    auto confidence_profile
        = std::find_if(stream_profiles.begin(), stream_profiles.end(), [&](stream_profile sp) {
        return sp.stream_type() == RS2_STREAM_CONFIDENCE
            && sp.as< rs2::video_stream_profile >().width()
            == depth_profile.as< rs2::video_stream_profile >().width()
            && sp.as< rs2::video_stream_profile >().height()
            == depth_profile.as< rs2::video_stream_profile >().height();
    });

    REQUIRE(confidence_profile != stream_profiles.end());
    return *confidence_profile;
}

inline void do_while_streaming( rs2::sensor depth_sens,
                                std::vector< stream_profile > profiles,
                                std::function< void() > action )
{
    REQUIRE_NOTHROW( depth_sens.open( profiles ) );
    REQUIRE_NOTHROW( depth_sens.start( [&]( rs2::frame f ) {} ) );

    action();

    depth_sens.stop();
    depth_sens.close();
}

rs2::stream_profile get_profile_by_stream_parameters( rs2::sensor s,
                                                      rs2_stream stream = RS2_STREAM_ANY,
                                                      rs2_format format = RS2_FORMAT_ANY,
                                                      int width = -1,
                                                      int height = -1,
                                                      int fps = -1,
                                                      int stream_index = -1 )
{
    auto profiles = s.get_stream_profiles();
    auto found_profile
        = std::find_if( profiles.begin(), profiles.end(), [=]( rs2::stream_profile sp ) {
              auto vp = sp.as< rs2::video_stream_profile >();
              return ( ( RS2_STREAM_ANY == stream || sp.stream_type() == stream )
                       && ( RS2_FORMAT_ANY == format || sp.format() == format )
                       && ( ( -1 == width )  || ( vp.width() == width ) )
                       && ( ( -1 == height ) || ( vp.height() == height ) )
                       && ( ( -1 == fps )    || ( vp.fps() == fps )
                       && ( ( -1 == stream_index ) || vp.stream_index() == stream_index ) ) );
          } );

    if( found_profile != profiles.end() )
        return *found_profile;
    else
        return rs2::stream_profile();
}

