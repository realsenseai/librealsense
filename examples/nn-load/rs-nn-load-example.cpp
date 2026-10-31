// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

// This example uploads an NN model package (from the Perception Service) to a camera, waits for the camera to
// reset with the new model, then prints its object detections for a few seconds.
//
// Usage: rs-nn-load-example <model package>

#include <librealsense2/rs.hpp>

#include <chrono>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <vector>

int main( int argc, char * argv[] ) try
{
    if( argc != 2 )
    {
        std::cerr << "Usage: " << argv[0] << " <model package>" << std::endl;
        return EXIT_FAILURE;
    }
    std::ifstream file( argv[1], std::ios::binary );
    if( ! file )
    {
        std::cerr << "Cannot open " << argv[1] << std::endl;
        return EXIT_FAILURE;
    }
    std::vector< uint8_t > package( ( std::istreambuf_iterator< char >( file ) ), std::istreambuf_iterator< char >() );

    // Find a camera that accepts NN models
    rs2::context ctx;
    rs2::nn_loader loader;
    for( auto && dev : ctx.query_devices() )
        if( ( loader = rs2::nn_loader( dev ) ) )
            break;
    if( ! loader )
    {
        std::cerr << "No camera that accepts NN models was found" << std::endl;
        return EXIT_FAILURE;
    }
    std::string reason;
    if( ! loader.is_upload_supported( &reason ) )
    {
        std::cerr << reason << std::endl;
        return EXIT_FAILURE;
    }
    std::string const serial = loader.get_info( RS2_CAMERA_INFO_SERIAL_NUMBER );

    // Once the model is installed the camera resets: watch for it to come back
    std::mutex mutex;
    std::condition_variable cv;
    bool reconnected = false;
    ctx.set_devices_changed_callback( [&]( rs2::event_information & info )
    {
        for( auto && dev : info.get_new_devices() )
        {
            if( dev.get_info( RS2_CAMERA_INFO_SERIAL_NUMBER ) != serial )
                continue;
            std::lock_guard< std::mutex > lock( mutex );
            reconnected = true;
            cv.notify_one();
        }
    } );

    // The upload blocks until the model is installed; the Perception sensor must not be streaming
    std::cout << "Uploading " << argv[1] << " to " << loader.get_info( RS2_CAMERA_INFO_NAME ) << " " << serial << std::endl;
    loader.nn_upload( package, []( float progress ) { std::cout << "\rProgress: " << int( progress * 100 ) << "%" << std::flush; } );
    std::cout << std::endl << "Model installed, waiting for the camera to reset" << std::endl;
    {
        std::unique_lock< std::mutex > lock( mutex );
        if( ! cv.wait_for( lock, std::chrono::seconds( 15 ), [&] { return reconnected; } ) )
        {
            std::cerr << "The camera did not come back: reconnect it to use the new model" << std::endl;
            return EXIT_FAILURE;
        }
    }

    // Stream with the new model: object detection runs on the color stream
    rs2::pipeline pipe( ctx );
    rs2::config cfg;
    cfg.enable_device( serial );
    cfg.enable_stream( RS2_STREAM_COLOR );
    cfg.enable_stream( RS2_STREAM_OBJECT_DETECTION );
    pipe.start( cfg );

    auto const end = std::chrono::steady_clock::now() + std::chrono::seconds( 10 );
    while( std::chrono::steady_clock::now() < end )
    {
        rs2::object_detection_frame odf = pipe.wait_for_frames().get_object_detection_frame();
        if( ! odf )
            continue;
        std::cout << "[frame " << odf.get_frame_number() << "] " << odf.get_detection_count() << " detection(s)";
        for( unsigned int i = 0; i < odf.get_detection_count(); ++i )
        {
            rs2_object_detection det = odf.get_detection( i );
            std::cout << "  class " << det.class_id << " score " << det.score << " (" << det.top_left_x << ","
                      << det.top_left_y << ")-(" << det.bottom_right_x << "," << det.bottom_right_y << ")";
        }
        std::cout << std::endl;
    }
    pipe.stop();
    return EXIT_SUCCESS;
}
catch( const rs2::error & e )
{
    std::cerr << std::endl << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args()
              << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch( const std::exception & e )
{
    std::cerr << std::endl << e.what() << std::endl;
    return EXIT_FAILURE;
}
