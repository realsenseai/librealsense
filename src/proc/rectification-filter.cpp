// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#include "rectification-filter.h"
#include "option.h"
#include <opencv2/opencv.hpp>

namespace librealsense
{
    rectification_filter::rectification_filter( rsutils::number::float3x3 & k_distorted,
                                                std::vector< float > & dist_coeffs,
                                                rsutils::number::float3x3 & rotation_mat,
                                                rsutils::number::float3x3 & k_rect )
        : stream_filter_processing_block("Rectification Filter")
    {
        _stream_filter.stream = RS2_STREAM_COLOR;
        _stream_filter.format = RS2_FORMAT_RGB8;
        
        // Initialize rectification parameters with the values from color-formats-converter.cpp

        // cv::Mat is raw major, float3x3 is coloumn major
        cv::Mat camera_matrix = ( cv::Mat_< double >( 3, 3 ) << k_distorted( 0, 0 ), k_distorted( 1, 0 ), k_distorted( 2, 0 ),
                                                                k_distorted( 0, 1 ), k_distorted( 1, 1 ), k_distorted( 2, 1 ),
                                                                k_distorted( 0, 2 ), k_distorted( 1, 2 ), k_distorted( 2, 2 ) );

        cv::Mat coeffs = ( cv::Mat_< double >( 1, 5 ) << dist_coeffs[0], dist_coeffs[1], dist_coeffs[2], dist_coeffs[3], dist_coeffs[4] );
        cv::Mat R = (cv::Mat_<double>(3, 3) << rotation_mat( 0, 0 ), rotation_mat( 1, 0 ), rotation_mat( 2, 0 ),
                                               rotation_mat( 0, 1 ), rotation_mat( 1, 1 ), rotation_mat( 2, 1 ), 
                                               rotation_mat( 0, 2 ), rotation_mat( 1, 2 ), rotation_mat( 2, 2 ) );
        cv::Mat new_camera_matrix = (cv::Mat_<double>(3, 3) << k_rect( 0, 0 ), k_rect( 1, 0 ), k_rect( 2, 0 ),
                                                               k_rect( 0, 1 ), k_rect( 1, 1 ), k_rect( 2, 1 ),
                                                               k_rect( 0, 2 ), k_rect( 1, 2 ), k_rect( 2, 2 ) );
        cv::Size image_size = cv::Size( 1280, 720 );

        // Precompute undistort/rectify maps
        cv::initUndistortRectifyMap( camera_matrix, coeffs, R, new_camera_matrix, image_size, CV_16SC2, _map1, _map2 );

        // Preallocate buffer
        _rectified_buffer.create( image_size, CV_8UC3 );
    }

    bool rectification_filter::should_process( const rs2::frame & frame )
    {
        if( ! frame || frame.is< rs2::frameset >() )
            return false;

        auto profile = frame.get_profile();
        return profile.stream_type() == RS2_STREAM_COLOR && profile.format() == RS2_FORMAT_RGB8;
    }

    rs2::frame rectification_filter::process_frame( const rs2::frame_source & source, const rs2::frame & f )
    {
        auto vf = f.as< rs2::video_frame >();
        auto profile = f.get_profile().as< rs2::video_stream_profile >();

        int width = vf.get_width();
        int height = vf.get_height();
        int bpp = vf.get_bytes_per_pixel();
        int actual_size = width * height * bpp;

        // Allocate output frame
        auto target_profile = profile.clone( profile.stream_type(), profile.stream_index(), profile.format() );
        auto ret = source.allocate_video_frame( target_profile, f, bpp, width, height, width * bpp, RS2_EXTENSION_VIDEO_FRAME );

        // Perform rectification
        uint8_t * src = const_cast< uint8_t * >( reinterpret_cast< const uint8_t * >( vf.get_data() ) );
        uint8_t * dst = const_cast< uint8_t * >( reinterpret_cast< const uint8_t * >( ret.get_data() ) );

        // Remap to rectified image
        cv::Mat rgb_buffer( height, width, CV_8UC3, src );  // Uses the data without copying it
        cv::remap( rgb_buffer, _rectified_buffer, _map1, _map2, cv::INTER_LINEAR );
        memcpy( dst, _rectified_buffer.data, actual_size );

        return ret;
    }
    }
