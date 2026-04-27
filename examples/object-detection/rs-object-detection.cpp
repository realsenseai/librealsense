// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

// This example demonstrates how to receive and print object detection results
// from a RealSense device that supports inference streaming.
//
// Prerequisites:
//   - A RealSense device with an inference sensor (e.g. connected via DDS).
//   - Color stream must be active; object detection runs on top of it.
//
// The example enables Color and Object Detection streams, then loops
// printing each detection details (class, confidence score, bounding box, etc...).

#include <librealsense2/rs.hpp>

#include <iomanip>
#include <iostream>


// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Human-readable label for a class_id reported by the detection engine.
// Extend this table to match the model deployed on your device.
static const char * class_label( int class_id )
{
    switch( class_id )
    {
    case 0:  return "Person";
    case 1:  return "Face";
    default: return "Unknown";
    }
}


// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main( int /*argc*/, char * /*argv*/[] ) try
{
    rs2::pipeline pipe;
    rs2::config   cfg;

    // Color is required for inference to run on. Use device default settings for resolution/fps.
    cfg.enable_stream( RS2_STREAM_COLOR );
    cfg.enable_stream( RS2_STREAM_OBJECT_DETECTION );

    pipe.start( cfg );

    std::cout << "Streaming — press Ctrl+C to stop.\n\n";

    while( true )
    {
        rs2::frameset frames = pipe.wait_for_frames();

        // The object detection frame may not be present in every frameset
        // (it is synced to color frames but can arrive at a lower rate).
        rs2::object_detection_frame odf = frames.get_object_detection_frame();
        if( ! odf )
            continue;

        unsigned int const count = odf.get_detection_count();
        if( count == 0 )
        {
            std::cout << "[frame " << odf.get_frame_number() << "]  No detections.\n";
            continue;
        }

        std::cout << "[frame " << odf.get_frame_number() << "]  "
                  << count << " detection(s):\n";

        for( unsigned int i = 0; i < count; ++i )
        {
            rs2_object_detection det = odf.get_detection( i );

            std::cout << "  [" << i << "] "
                      << std::left << std::setw( 8 ) << class_label( det.class_id )
                      << "  score=" << std::right << std::setw( 3 ) << det.score << "%"
                      << "  bbox=("  << det.top_left_x     << "," << det.top_left_y     << ")-"
                      <<       "("   << det.bottom_right_x << "," << det.bottom_right_y << ")"
                      << "\n";
        }
        std::cout << "\n";
    }

    return EXIT_SUCCESS;
}
catch( const rs2::error & e )
{
    std::cerr << "RealSense error calling " << e.get_failed_function()
              << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch( const std::exception & e )
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
