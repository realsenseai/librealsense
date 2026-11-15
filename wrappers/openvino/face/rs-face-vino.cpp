// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.hpp>   // Include RealSense Cross Platform API

#include "cv-helpers.hpp"         // frame_to_mat

#include <rs-vino/object-detection.h>
#include <rs-vino/detected-object.h>

#include <rsutils/easylogging/easyloggingpp.h>
#if defined( BUILD_SHARED_LIBS ) && ! defined( __linux__ )
// With static linkage, ELPP is initialized by librealsense, so doing it here will
// create errors. When we're using the shared .dll/.dylib, the two are separate and we have
// to initialize ours if we want to use the APIs! On Linux we share the storage exported
// by librealsense; initializing ours would destroy it twice at exit.
INITIALIZE_EASYLOGGINGPP
#endif


namespace openvino = InferenceEngine;


int main(int argc, char * argv[]) try
{
    el::Configurations conf;
    conf.set( el::Level::Global, el::ConfigurationType::Format, "[%level] %msg" );
    //conf.set( el::Level::Debug, el::ConfigurationType::Enabled, "false" );
    el::Loggers::reconfigureLogger( "default", conf );
    rs2::log_to_console( RS2_LOG_SEVERITY_WARN );    // only warnings (and above) should come through

    // Declare RealSense pipeline, encapsulating the actual device and sensors
    rs2::pipeline pipe;
    pipe.start();
    rs2::align align_to( RS2_STREAM_COLOR );

    // Start the inference engine, needed to accomplish anything. We also add a CPU extension, allowing
    // us to run the inference on the CPU. A GPU solution may be possible but, at least without a GPU,
    // a CPU-bound process is faster. To change to GPU, use "GPU" instead (and remove AddExtension()):
    openvino::Core engine;

#ifdef OPENVINO2019
    openvino_helpers::error_listener error_listener;
    engine.SetLogCallback( error_listener );
#endif

    std::string const device_name { "CPU" };

    // Cpu extensions library was removed in OpenVINO >= 2020.1, extensions were merged into the cpu plugin.
#ifdef OPENVINO2019
    engine.AddExtension( std::make_shared< openvino::Extensions::Cpu::CpuExtensions >(), device_name );
#endif

    openvino_helpers::object_detection faceDetector(
        "face-detection-adas-0001.xml",
        0.5     // Probability threshold -- anything with less confidence will be thrown out
    );
    try
    {
        faceDetector.load_into( engine, device_name );
    }
    catch( const std::exception & e )
    {
        // The model files should have been downloaded automatically by CMake into build/wrappers/openvino/face,
        // which is also where Visual Studio runs the sample from. However, you may need to copy these two files:
        //     face-detection-adas-0001.bin
        //     face-detection-adas-0001.xml
        // Into the local directory where you run from (or change the path given in the ctor above)
        LOG(ERROR) << "Failed to load model files:\n    " << e.what();
        LOG(INFO) << "Please copy the model files into the working directory from which this sample is run";
        return EXIT_FAILURE;
    }


    const auto window_name = "OpenVINO face detection sample";
    cv::namedWindow( window_name, cv::WINDOW_AUTOSIZE );

    bool first_frame = true;
    cv::Mat prev_image;
    openvino_helpers::detected_objects faces;
    size_t id = 0;

    while( cv::getWindowProperty( window_name, cv::WND_PROP_AUTOSIZE ) >= 0 )
    {
        // Wait for the next set of frames
        auto data = pipe.wait_for_frames();
        // Make sure the frames are spatially aligned
        data = align_to.process( data );

        auto color_frame = data.get_color_frame();
        auto depth_frame = data.get_depth_frame();

        // If we only received a new depth frame, but the color did not update, continue
        static uint64 last_frame_number = 0;
        if( color_frame.get_frame_number() == last_frame_number )
            continue;
        last_frame_number = color_frame.get_frame_number();

        auto image = frame_to_mat( color_frame );

        // We process the previous frame so if this is our first then queue it and continue
        if( first_frame )
        {
            faceDetector.enqueue( image );
            faceDetector.submit_request();
            first_frame = false;
            prev_image = image;
            continue;
        }

        // Wait for the results of the previous frame we enqueued: we're going to process these
        faceDetector.wait();
        auto const results = faceDetector.fetch_results();

        // Enqueue the current frame so we'd get the results when the next frame comes along!
        faceDetector.enqueue( image );
        faceDetector.submit_request();

        openvino_helpers::detected_objects prev_faces { std::move( faces ) };
        faces.clear();
        for( auto const & result : results )
        {
            cv::Rect rect = result.location;
            rect = rect & cv::Rect( 0, 0, image.cols, image.rows );
            auto face_ptr = openvino_helpers::find_object( rect, prev_faces );
            if( !face_ptr )
                // New face
                face_ptr = std::make_shared< openvino_helpers::detected_object >( id++, std::string(), rect );
            else
                // Existing face; just update its parameters
                face_ptr->move( rect );
            faces.push_back( face_ptr );
        }

        // Keep this image so we can actually process pieces of it once we have the results
        prev_image = image;

        // Display the results (from the last frame) as rectangles on top (of the current frame)
        for( auto && face : faces )
        {
            cv::Scalar color( 255, 255, 255 );  // BGR
            auto r = face->get_location();
            cv::rectangle( image, r, color );

            // Get a very crude approximation (not aligned) of the center in the depth frame, and output the distance to it
            auto center_x = r.x + r.width / 2;
            auto center_y = r.y + r.height / 2;
            auto d = depth_frame.get_distance( center_x, center_y );
            if( d )
            {
                std::ostringstream ss;
                ss << std::setprecision( 2 ) << d;
                ss << " m";
                cv::putText( image, ss.str(), cv::Point( r.x + 5, r.y + r.height - 5 ), cv::FONT_HERSHEY_SIMPLEX, 0.3, color );
            }
        }

        imshow( window_name, image );
        if( cv::waitKey( 1 ) >= 0 )
            break;
    }

    return EXIT_SUCCESS;
}
catch (const rs2::error & e)
{
    LOG(ERROR) << "Caught RealSense exception from " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what();
    return EXIT_FAILURE;
}
catch (const std::exception& e)
{
    LOG(ERROR) << "Unknown exception caught: " << e.what();
    return EXIT_FAILURE;
}

