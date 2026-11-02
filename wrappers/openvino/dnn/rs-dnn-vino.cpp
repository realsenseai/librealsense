// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.hpp>   // Include RealSense Cross Platform API

#include "cv-helpers.hpp"         // frame_to_mat
#include <opencv2/core/utils/filesystem.hpp>   // glob
namespace fs = cv::utils::fs;

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

#include <rs-vino/openvino-helpers.h>
namespace openvino = InferenceEngine;

#include <chrono>
using namespace std::chrono;


/*
    Enable loading multiple detectors at once, so we can switch at runtime.
    Each detector has its associated labels.
*/
struct detector_and_labels
{
    std::shared_ptr< openvino_helpers::object_detection > detector;
    std::vector< std::string > labels;

    detector_and_labels( std::string const & path_to_xml )
        : detector( std::make_shared< openvino_helpers::object_detection >( path_to_xml, 0.5 ) )
    {
    }

    openvino_helpers::object_detection * operator->() { return detector.get(); }

    void load_labels()
    {
        try
        {
            labels = openvino_helpers::read_labels( openvino_helpers::remove_ext( detector->pathToModel ) + ".labels" );
        }
        catch( const std::exception & e )
        {
            // If we have no labels, warn and continue... we can continue without them
            LOG(WARNING) << "Failed to load labels: " << e.what();
        }
    }
};


/*
    Populate a collection of detectors from those we find on disk (*.xml), load
    their labels, add them to the engine & device, etc.

    The detectors are loaded with all default values.
*/
void load_detectors_into(
    std::vector< detector_and_labels > & detectors,
    openvino::Core & engine,
    std::string const & device_name
)
{
    std::vector< std::string > xmls;
    fs::glob_relative( ".", "*.xml", xmls );
    for( auto path_to_xml : xmls )
    {
        if (path_to_xml == "plugins.xml") continue; // plugin.xml is not model file, skip it in model search if exist

        detector_and_labels detector { path_to_xml };
        try
        {
            detector->load_into( engine, device_name );  // May throw!
            detector.load_labels();
            detectors.push_back( detector );
            LOG(INFO) << "   ... press '" << char( '0' + detectors.size() ) << "' to switch to it";
        }
        catch( const std::exception & e )
        {
            // The model files should have been downloaded automatically by CMake into build/wrappers/openvino/dnn,
            // which is also where Visual Studio runs the sample from. However, you may need to copy these files:
            //     *.bin
            //     *.xml
            //     *.labels  [optional]
            // Into the local directory where you run from (or change the path given in the ctor above)
            LOG(ERROR) << "Failed to load model: " << e.what();
        }
    }
}


/*
    Main detection code:

    Detected objects are placed into 'objects'. Each new object is assigned 'next_id', which is then incremented.
    The 'labels' are optional, and used to give labels to each object.
    
    Some basic effort is made to keep the creation of new objects to a minimum: previous objects (passed in via
    'objects') are compared with new detections to see if the new are simply new positions for the old. An
    "intersection over union" (IoU) quotient is calculated and, if over a threshold, an existing object is moved
    rather than a new one created.
*/
void detect_objects(
    cv::Mat const & image,
    std::vector< openvino_helpers::object_detection::Result > const & results,
    std::vector< std::string > & labels,
    size_t & next_id,
    openvino_helpers::detected_objects & objects
)
{
    openvino_helpers::detected_objects prev_objects{ std::move( objects ) };
    objects.clear();
    for( auto const & result : results )
    {
        if( result.label <= 0 )
            continue;  // ignore "background", though not clear why we'd get it
        cv::Rect rect = result.location;
        rect = rect & cv::Rect( 0, 0, image.cols, image.rows );
        auto object_ptr = openvino_helpers::find_object( rect, prev_objects );
        if( ! object_ptr )
        {
            // New object
            std::string label;
            if( result.label < labels.size() )
                label = labels[result.label];
            object_ptr = std::make_shared< openvino_helpers::detected_object >( next_id++, label, rect );
        }
        else
        {
            // Existing face; just update its parameters
            object_ptr->move( rect );
        }
        objects.push_back( object_ptr );
    }
}


/*
    Draws the detected objects with a distance calculated at the center pixel of each face
*/
void draw_objects(
    cv::Mat & image,
    rs2::depth_frame depth_frame,
    openvino_helpers::detected_objects const & objects
)
{
    cv::Scalar const green( 0, 255, 0 );  // BGR
    cv::Scalar const white( 255, 255, 255 );  // BGR
    
    for( auto && object : objects )
    {
        auto r = object->get_location();
        cv::rectangle( image, r, green );

        // Output the distance to the center
        auto center_x = r.x + r.width / 2;
        auto center_y = r.y + r.height / 2;
        auto d = depth_frame.get_distance( center_x, center_y );
        if( d )
        {
            std::ostringstream ss;
            ss << object->get_label() << " ";
            ss << std::setprecision( 2 ) << d;
            ss << " meters away";
            cv::putText( image, ss.str(), cv::Point( r.x + 5, r.y + r.height - 5 ), cv::FONT_HERSHEY_SIMPLEX, 0.4, white );
        }
    }
}


/*
    When the user switches betweem models we show the detector number for 1 second as an
    overlay over the image, centered.
*/
void draw_detector_overlay(
    cv::Mat & image,
    size_t current_detector,
    high_resolution_clock::time_point switch_time
)
{
    auto ms_since_switch = duration_cast< milliseconds >( high_resolution_clock::now() - switch_time ).count();
    if( ms_since_switch > 1000 )
        ms_since_switch = 1000;
    double alpha = ( 1000 - ms_since_switch ) / 1000.;
    std::string str( 1, char( '1' + current_detector ) );
    auto size = cv::getTextSize( str, cv::FONT_HERSHEY_SIMPLEX, 3, 1, nullptr );
    cv::Point center{ image.cols / 2, image.rows / 2 };
    cv::Rect r{ center.x - size.width, center.y - size.height, size.width * 2, size.height * 2 };
    cv::Mat roi = image( r );
    cv::Mat overlay( roi.size(), CV_8UC3, cv::Scalar( 32, 32, 32 ) );
    cv::putText( overlay, str, cv::Point{ r.width / 2 - size.width / 2, r.height / 2 + size.height / 2 }, cv::FONT_HERSHEY_SIMPLEX, 3, cv::Scalar{ 255, 255, 255 } );
    cv::addWeighted( overlay, alpha, roi, 1 - alpha, 0, roi );   // roi = overlay * alpha + roi * (1-alpha) + 0
}


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
    engine.AddExtension(std::make_shared< openvino::Extensions::Cpu::CpuExtensions >(), device_name);
#endif

    std::vector< detector_and_labels > detectors;
    load_detectors_into( detectors, engine, device_name );
    if( detectors.empty() )
    {
        LOG(ERROR) << "No detectors available in: " << fs::getcwd();
        return EXIT_FAILURE;
    }
    // Look for the mobilenet-ssd so it always starts the same... otherwise default to the first detector we found
    size_t current_detector = 0;
    for( size_t i = 1; i < detectors.size(); ++i )
    {
        if( detectors[i]->pathToModel == "mobilenet-ssd.xml" )
        {
            current_detector = i;
            break;
        }
    }
    auto p_detector = detectors[current_detector].detector;
    LOG(INFO) << "Current detector set to (" << current_detector+1 << ") \"" << openvino_helpers::remove_ext( p_detector->pathToModel ) << "\"";
    auto p_labels = &detectors[current_detector].labels;

    const auto window_name = "OpenVINO DNN sample";
    cv::namedWindow( window_name, cv::WINDOW_AUTOSIZE );

    cv::Mat prev_image;
    openvino_helpers::detected_objects objects;
    size_t id = 0;
    uint64 last_frame_number = 0;
    high_resolution_clock::time_point switch_time = high_resolution_clock::now();

    while( cv::getWindowProperty( window_name, cv::WND_PROP_AUTOSIZE ) >= 0 )
    {
        // Wait for the next set of frames
        auto frames = pipe.wait_for_frames();
        // Make sure the frames are spatially aligned
        frames = align_to.process( frames );

        auto color_frame = frames.get_color_frame();
        auto depth_frame = frames.get_depth_frame();
        if( ! color_frame  ||  ! depth_frame )
            continue;

        // If we only received a new depth frame, but the color did not update, continue
        if( color_frame.get_frame_number() == last_frame_number )
            continue;
        last_frame_number = color_frame.get_frame_number();

        auto image = frame_to_mat( color_frame );

        // We process the previous frame so if this is our first then queue it and continue
        if( ! p_detector->_request )
        {
            p_detector->enqueue( image );
            p_detector->submit_request();
            prev_image = image;
            continue;
        }

        // Wait for the results of the previous frame we enqueued: we're going to process these
        p_detector->wait();
        auto const results = p_detector->fetch_results();

        // Enqueue the current frame so we'd get the results when the next frame comes along!
        p_detector->enqueue( image );
        p_detector->submit_request();

        // MAIN DETECTION
        detect_objects( image, results, *p_labels, id, objects );

        // Keep it alive so we can actually process pieces of it once we have the results
        prev_image = image;

        // Display the results (from the last frame) as rectangles on top (of the current frame)
        draw_objects( image, depth_frame, objects );
        draw_detector_overlay( image, current_detector, switch_time );
        imshow( window_name, image );

        // Handle the keyboard before moving to the next frame
        const int key = cv::waitKey( 1 );
        if( key == 27 )
            break;  // escape
        if( key >= '1'  &&  key < '1' + detectors.size() )
        {
            size_t detector_index = key - '1';
            if( detector_index != current_detector )
            {
                current_detector = detector_index;
                p_detector = detectors[current_detector].detector;
                p_labels = &detectors[current_detector].labels;
                objects.clear();
                LOG(INFO) << "Current detector set to (" << current_detector+1 << ") \"" << openvino_helpers::remove_ext( p_detector->pathToModel ) << "\"";
            }
            switch_time = high_resolution_clock::now();
        }
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

