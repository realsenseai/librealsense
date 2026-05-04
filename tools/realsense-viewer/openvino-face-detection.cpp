// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2020 RealSense, Inc. All Rights Reserved.

// NOTE: This file will be compiled only with INTEL_OPENVINO_DIR pointing to an OpenVINO install!

#include "post-processing-filters-list.h"
#include "post-processing-worker-filter.h"

#include <rs-vino/object-detection.h>
#include <rs-vino/detected-object.h>
#include <cv-helpers.hpp>

#include <rsutils/string/from.h>

namespace openvino = InferenceEngine;

/* We need to extend the basic detected_object to include facial characteristics
*/
class detected_face : public openvino_helpers::detected_object
{
public:
    using ptr = std::shared_ptr< detected_face >;

    explicit detected_face( size_t id,
        cv::Rect const& location,
        cv::Rect const& depth_location = cv::Rect{},
        float intensity = 1,
        float depth = 0 )
        : detected_object( id, std::string(), location, depth_location, intensity, depth )
    {
    }   
};


/* Define a filter that will perform facial detection using OpenVINO
*/
class openvino_face_detection : public post_processing_worker_filter
{
    InferenceEngine::Core _ie;
    openvino_helpers::object_detection _face_detector;
    openvino_helpers::detected_objects _faces;
    size_t _id = 0;

    std::shared_ptr< atomic_objects_in_frame > _objects;
    
public:
    openvino_face_detection( std::string const & name )
        : post_processing_worker_filter( name )
        /*
            This face detector is from the OpenCV Model Zoo:
            https://github.com/opencv/open_model_zoo/blob/master/models/intel/face-detection-adas-0001/description/face-detection-adas-0001.md
        */
        , _face_detector(
            "face-detection-adas-0001.xml",
            0.5,    // Probability threshold
            false ) // Not async
    {
    }

    ~openvino_face_detection()
    {
        // Complete background worker to ensure it releases the instance's resources in controlled manner
        release_background_worker();
    }

public:
    void start( rs2::subdevice_model & model ) override
    {
        post_processing_worker_filter::start( model );
        _objects = model.detected_objects;
    }

private:
    void worker_start() override
    {
        LOG(INFO) << "Loading CPU extensions...";
        std::string const device_name{ "CPU" };

    // Cpu extensions library was removed in OpenVINO >= 2020.1, extensions were merged into the cpu plugin.
#ifdef OPENVINO2019
        _ie.AddExtension(std::make_shared< openvino::Extensions::Cpu::CpuExtensions >(), device_name);
#endif

        _face_detector.load_into( _ie, device_name);
    }

    /*
        Returns the "intensity" of the face in the picture, and calculates the distance to it, ignoring
        Invalid depth pixels or those outside a range that would be appropriate for a face.
    */
    static float calc_face_attrs(
        const rs2::video_frame & cf,
        const rs2::depth_frame & df,
        cv::Rect const & depth_bbox, 
        float * p_mean_depth )
    {
        uint16_t const * const pdw = reinterpret_cast<const uint16_t*>( df.get_data() );
        uint8_t const * const pcb = reinterpret_cast<uint8_t*>(const_cast<void*>( cf.get_data() ));
        float const depth_scale = df.get_units();

        int const depth_width = df.get_width();
        int const color_width = cf.get_width();
        int const color_bpp = cf.get_bytes_per_pixel();

        int const top = depth_bbox.y;
        int const bot = top + depth_bbox.height;
        int const left = depth_bbox.x;
        int const right = left + depth_bbox.width;

        // Find a center point that has a depth on it
        int center_x = (left + right) / 2;
        int center_index = (top + bot) / 2 * depth_width + center_x;
        for( int d = 1; !pdw[center_index] && d < 10; ++d )
        {
            if( pdw[center_index + d] ) center_index += d;
            if( pdw[center_index - d] ) center_index -= d;
            if( pdw[center_index + depth_width] ) center_index += depth_width;
            if( pdw[center_index - depth_width] ) center_index -= depth_width;
        }
        if( !pdw[center_index] )
        {
            if( p_mean_depth )
                *p_mean_depth = 0;
            return 1;
        }
        float const d_center = pdw[center_index] * depth_scale;

        // Set a "near" and "far" threshold -- anything closer or father, respectively,
        // would be deemed not a part of the face and therefore background:
        float const d_far_threshold = d_center + 0.2f;
        float const d_near_threshold = std::max( d_center - 0.5f, 0.001f );
        // Average human head diameter ~= 7.5" or ~19cm
        // Assume that the center point is in the front of the face, so the near threshold
        // should be very close to that, while the far farther...

        float total_luminance = 0;
        float total_depth = 0;
        unsigned pixel_count = 0;
#pragma omp parallel for schedule(dynamic) //Using OpenMP to try to parallelise the loop
        for( int y = top; y < bot; ++y )
        {
            auto depth_pixel_index = y * depth_width + left;
            for( int x = left; x < right; ++x, ++depth_pixel_index )
            {
                // Get the depth value of the current pixel
                auto d = depth_scale * pdw[depth_pixel_index];

                // Check if the depth value is invalid (<=0) or greater than the threashold
                if( d >= d_near_threshold && d <= d_far_threshold )
                {
                    // Calculate the offset in other frame's buffer to current pixel
                    auto const coffset = depth_pixel_index * color_bpp;
                    auto const pc = &pcb[coffset];

                    // Using RGB...
                    auto r = pc[0], g = pc[1], b = pc[2]; 
                    total_luminance += 0.2989f * r + 0.5870f * g + 0.1140f * b;  // CCIR 601 -- see https://en.wikipedia.org/wiki/Luma_(video)
                    ++pixel_count;

                    // And get a mean depth, too
                    total_depth += d;
                }
            }
        }
        if( p_mean_depth )
            *p_mean_depth = pixel_count ? total_depth / pixel_count : 0;
        return pixel_count ? total_luminance / pixel_count : 1;
    }

    void worker_body( rs2::frame f ) override
    {
        auto fs = f.as< rs2::frameset >();
        auto cf = f;
        rs2::depth_frame df = rs2::frame{};
        if (fs)
        {
            cf = fs.get_color_frame();
            df = fs.get_depth_frame();
        }

        if ((!fs && f.get_profile().stream_name() != "Color") || (fs && !cf))
        {
            _objects->clear();
            return;
        }

        // A color video frame is the minimum we need for detection
        if( cf.get_profile().format() != RS2_FORMAT_RGB8 )
        {
            LOG(ERROR) << get_context(fs) << "color format must be RGB8; it's " << cf.get_profile().format();
            return;
        }

        // A depth frame is optional: if not enabled, we won't get it, and we simply won't provide depth info...

        if (df && df.get_profile().format() != RS2_FORMAT_Z16)
        {
            LOG(ERROR) << get_context(fs) << "depth format must be Z16; it's " << df.get_profile().format();
            return;
        }
        try
        {
            rs2_intrinsics color_intrin, depth_intrin;
            rs2_extrinsics color_extrin, depth_extrin;
            get_trinsics( cf, df, color_intrin, depth_intrin, color_extrin, depth_extrin );

            objects_in_frame objects;

            cv::Mat image( color_intrin.height, color_intrin.width, CV_8UC3, const_cast<void *>(cf.get_data()), cv::Mat::AUTO_STEP );
            _face_detector.enqueue( image );
            _face_detector.submit_request();
            auto results = _face_detector.fetch_results();

            openvino_helpers::detected_objects prev_faces { std::move( _faces ) };
            _faces.clear();
            for( auto && result : results )
            {
                cv::Rect rect = result.location & cv::Rect( 0, 0, image.cols, image.rows );
                detected_face::ptr face = std::dynamic_pointer_cast< detected_face >(
                    openvino_helpers::find_object( rect, prev_faces ));
                try
                {
                    // Use a mean of the face intensity to help identify faces -- if the intensity changes too much,
                    // it's not the same face...
                    float depth = 0, intensity = 1;
                    cv::Rect depth_rect;
                    if( df )
                    {
                        rs2::rect depth_bbox = project_rect_to_depth(
                            rs2::rect { float( rect.x ), float( rect.y ), float( rect.width ), float( rect.height ) },
                            df,
                            color_intrin, depth_intrin, color_extrin, depth_extrin
                        );
                        // It is possible to get back an invalid rect!
                        if( depth_bbox == depth_bbox.intersection( rs2::rect { 0.f, 0.f, float( depth_intrin.width ), float( depth_intrin.height) } ) )
                        {
                            depth_rect = cv::Rect( int( depth_bbox.x ), int( depth_bbox.y ), int( depth_bbox.w ), int( depth_bbox.h ) );
                            intensity = calc_face_attrs( cf, df, depth_rect, &depth );
                        }
                        else
                        {
                            LOG(DEBUG) << get_context(fs) << "depth_bbox is no good!";
                        }
                    }
                    else
                    {
                        intensity = openvino_helpers::calc_intensity( image( rect ) );
                    }
                    float intensity_change = face ? std::abs( intensity - face->get_intensity() ) / face->get_intensity() : 1;
                    float depth_change = ( face  &&  face->get_depth() ) ? std::abs( depth - face->get_depth() ) / face->get_depth() : 0;

                    if( intensity_change > 0.07f || depth_change > 0.2f )
                    {
                        // Figure out the age for this new face
                        float age = 0, maleProb = 0.5;
                        // Enlarge the bounding box around the detected face for more robust operation of face analytics networks
                        cv::Mat face_image = image(
                            openvino_helpers::adjust_face_bbox( rect, 1.4f )
                                & cv::Rect( 0, 0, image.cols, image.rows ) );
                        face = std::make_shared< detected_face >( _id++, rect, depth_rect, intensity, depth );
                    }
                    else
                    {
                        face->move( rect, depth_rect, intensity, depth );
                    }

                    _faces.push_back( face );
                }
                catch( ... )
                {
                    LOG(ERROR) << get_context(fs) << "Unhandled exception!!!";
                }
            }

            for( auto && object : _faces )
            {
                auto face = std::dynamic_pointer_cast<detected_face>( object );
                cv::Rect const & loc = face->get_location();
                rs2::rect bbox { float( loc.x ), float( loc.y ), float( loc.width ), float( loc.height ) };
                rs2::rect normalized_color_bbox = bbox.normalize( rs2::rect { 0, 0, float(color_intrin.width), float(color_intrin.height) } );
                rs2::rect normalized_depth_bbox = normalized_color_bbox;
                if( df )
                {
                    cv::Rect const & depth_loc = face->get_depth_location();
                    rs2::rect depth_bbox { float( depth_loc.x ), float( depth_loc.y ), float( depth_loc.width ), float( depth_loc.height ) };
                    normalized_depth_bbox = depth_bbox.normalize( rs2::rect { 0, 0, float( df.get_width() ), float( df.get_height() ) } );
                }
                objects.emplace_back(
                    face->get_id(),
                    object_type_to_string( object_type::face ),
                    normalized_color_bbox,
                    normalized_depth_bbox,
                    face->get_depth(),
                    object_type::face
                );
            }

            std::lock_guard< std::mutex > lock( _objects->mutex );
            if( is_pb_enabled() )
            {
                if( _objects->sensor_is_on )
                    _objects->swap( objects );
            }
            else
            {
                _objects->clear();
            }
        }
        catch( const std::exception & e )
        {
            LOG(ERROR) << get_context(fs) << e.what();
        }
        catch( ... )
        {
            LOG(ERROR) << get_context(fs) << "Unhandled exception caught in openvino_face_detection";
        }
    }

    void on_processing_block_enable( bool e ) override
    {
        post_processing_worker_filter::on_processing_block_enable( e );
        if( !e )
        {
            // Make sure all the objects go away!
            std::lock_guard< std::mutex > lock( _objects->mutex );
            _objects->clear();
        }
    }

};


static auto it = post_processing_filters_list::register_filter< openvino_face_detection >( "Face Detection : OpenVINO" );

