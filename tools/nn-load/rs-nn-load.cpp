// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.hpp>
#include <common/cli.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <string>
#include <vector>

#define WAIT_FOR_DEVICE_TIMEOUT 15

#if _WIN32
#include <io.h>
#define ISATTY _isatty
#define FILENO _fileno
#else
#include <unistd.h>
#define ISATTY isatty
#define FILENO fileno
#endif

static std::string info( const rs2::device & d, rs2_camera_info i )
{
    return d.supports( i ) ? d.get_info( i ) : "unknown";
}

static void list_devices( const rs2::context & ctx )
{
    std::cout << "Connected NN loader devices:" << std::endl;
    int counter = 0;
    for( auto && d : ctx.query_devices() )
    {
        rs2::nn_loader loader( d );
        if( ! loader )
            continue;
        std::string reason;
        bool supported = loader.is_upload_supported( &reason );
        std::cout << ++counter << ") " << info( d, RS2_CAMERA_INFO_NAME ) << " s/n "
                  << info( d, RS2_CAMERA_INFO_SERIAL_NUMBER ) << ", firmware version: "
                  << info( d, RS2_CAMERA_INFO_FIRMWARE_VERSION ) << ", "
                  << ( supported ? "upload supported" : reason ) << std::endl;
    }
    if( ! counter )
        std::cout << "None" << std::endl;
}

static std::vector< uint8_t > read_file( const std::string & path )
{
    std::ifstream file( path, std::ios::binary );
    if( ! file )
        throw std::runtime_error( "Cannot open " + path );
    return std::vector< uint8_t >( std::istreambuf_iterator< char >( file ), std::istreambuf_iterator< char >() );
}

int main( int argc, char ** argv )
try
{
    using rs2::cli;
    cli cmd( "librealsense rs-nn-load tool" );

    cli::flag list_devices_arg( 'l', "list_devices", "List the connected devices that accept NN models" );
    cli::value< std::string > file_arg( 'f', "file", "path", "", "Path of the NN model package from the Perception Service" );
    cli::value< std::string > serial_number_arg( 's', "serial_number", "string", "", "The serial number of the device, mandatory if more than one NN loader device is connected" );
    cli::value< std::string > target_arg( "target", "flash|ddr", "flash", "Where to store the model (only flash is supported)" );
    cli::value< int > slot_arg( "slot", "number", 0, "Model slot (only 0 is supported)" );
    cli::flag no_activate_arg( "no-activate", "Store the model without activating it (not supported)" );

    cmd.default_log_level( RS2_LOG_SEVERITY_WARN );
    cmd.add( list_devices_arg );
    cmd.add( file_arg );
    cmd.add( serial_number_arg );
    cmd.add( target_arg );
    cmd.add( slot_arg );
    cmd.add( no_activate_arg );

    auto settings = cmd.process( argc, argv );

    // Declared before ctx so the devices they hold are released before it
    std::condition_variable cv;
    std::mutex mutex;
    rs2::device reconnected;
    rs2::context ctx( settings.dump() );

    if( list_devices_arg.isSet() || ! file_arg.isSet() )
    {
        if( ! list_devices_arg.isSet() )
            std::cout << "Nothing to do, run again with -h for help" << std::endl;
        list_devices( ctx );
        return list_devices_arg.isSet() ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    rs2_nn_target target;
    if( target_arg.getValue() == "flash" )
        target = RS2_NN_TARGET_FLASH;
    else if( target_arg.getValue() == "ddr" )
        target = RS2_NN_TARGET_DDR;
    else
        throw std::runtime_error( "Unknown target '" + target_arg.getValue() + "', use flash or ddr" );

    std::vector< rs2::nn_loader > loaders;
    for( auto && d : ctx.query_devices() )
    {
        rs2::nn_loader loader( d );
        if( loader && ( ! serial_number_arg.isSet()
                        || info( d, RS2_CAMERA_INFO_SERIAL_NUMBER ) == serial_number_arg.getValue() ) )
            loaders.push_back( loader );
    }
    if( loaders.empty() )
    {
        std::cout << ( serial_number_arg.isSet() ? "No NN loader device with serial number " + serial_number_arg.getValue()
                                                 : std::string( "No NN loader device was found" ) )
                  << std::endl;
        return EXIT_FAILURE;
    }
    if( loaders.size() > 1 )
    {
        std::cout << "Several NN loader devices are connected, select one using -s <serial_number>" << std::endl;
        list_devices( ctx );
        return EXIT_FAILURE;
    }

    rs2::nn_loader loader = loaders.front();
    std::string serial = info( loader, RS2_CAMERA_INFO_SERIAL_NUMBER );
    std::string reason;
    if( ! loader.is_upload_supported( &reason ) )
    {
        std::cout << reason << std::endl;
        return EXIT_FAILURE;
    }

    std::vector< uint8_t > package = read_file( file_arg.getValue() );

    // The camera resets once the model is installed: catch it coming back
    ctx.set_devices_changed_callback( [&]( rs2::event_information & ev )
    {
        for( auto && d : ev.get_new_devices() )
        {
            if( info( d, RS2_CAMERA_INFO_SERIAL_NUMBER ) != serial )
                continue;
            std::lock_guard< std::mutex > lk( mutex );
            reconnected = d;
            cv.notify_one();
        }
    } );

    std::cout << "Uploading " << file_arg.getValue() << " to " << info( loader, RS2_CAMERA_INFO_NAME ) << " s/n "
              << serial << ". Please don't disconnect the device!" << std::endl;
    bool activate = ! no_activate_arg.isSet();
    if( ISATTY( FILENO( stdout ) ) )
        loader.nn_upload( package, target, slot_arg.getValue(), activate, []( float progress )
        {
            printf( "\rNN model upload progress: %d[%%]", int( progress * 100 ) );
            fflush( stdout );
        } );
    else
        loader.nn_upload( package, target, slot_arg.getValue(), activate );
    std::cout << std::endl << "NN model installed, waiting for the device to reconnect..." << std::endl;

    std::unique_lock< std::mutex > lk( mutex );
    if( ! cv.wait_for( lk, std::chrono::seconds( WAIT_FOR_DEVICE_TIMEOUT ), [&] { return bool( reconnected ); } ) )
    {
        std::cout << "Device did not reconnect within " << WAIT_FOR_DEVICE_TIMEOUT
                  << " seconds: reconnect it to use the new model" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Device " << serial << " reconnected with the new NN model" << std::endl;
    return EXIT_SUCCESS;
}
catch( const rs2::error & e )
{
    std::cerr << std::endl << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args()
              << "):" << std::endl << "    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch( const std::exception & e )
{
    std::cerr << std::endl << e.what() << std::endl;
    return EXIT_FAILURE;
}
