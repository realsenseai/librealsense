// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#include "rectification-filter.h"
#include "option.h"
#include <opencv2/opencv.hpp>

namespace librealsense
{
    rectification_filter::rectification_filter() : stream_filter_processing_block("Rectification Filter")
    {
    }

    rectification_filter::rectification_filter( rs2_stream stream_to_rectify,
                                                rsutils::number::float3x3 & k_distorted,
                                                std::vector< float > & dist_coeffs,
                                                rsutils::number::float3x3 & rotation_mat,
                                                rsutils::number::float3x3 & k_rect,
                                                uint16_t rect_width, uint16_t rect_height )
        : stream_filter_processing_block("Rectification Filter")
        , _stream_to_rectify( stream_to_rectify )
    {
        // cv::Mat is raw major, float3x3 is coloumn major
        cv::Mat camera_matrix = ( cv::Mat_< double >( 3, 3 ) << k_distorted( 0, 0 ), k_distorted( 1, 0 ), k_distorted( 2, 0 ),
                                                                k_distorted( 0, 1 ), k_distorted( 1, 1 ), k_distorted( 2, 1 ),
                                                                k_distorted( 0, 2 ), k_distorted( 1, 2 ), k_distorted( 2, 2 ) );

        cv::Mat coeffs = ( cv::Mat_< double >( 1, 5 ) << dist_coeffs[0], dist_coeffs[1], dist_coeffs[2], dist_coeffs[3], dist_coeffs[4] );

        cv::Mat R = (cv::Mat_<double>(3, 3) << rotation_mat( 0, 0 ), rotation_mat( 0, 1 ), rotation_mat( 0, 2 ),
                                               rotation_mat( 1, 0 ), rotation_mat( 1, 1 ), rotation_mat( 1, 2 ), 
                                               rotation_mat( 2, 0 ), rotation_mat( 2, 1 ), rotation_mat( 2, 2 ) );

        // k_rect should be adjusted to new resolution.
        rsutils::number::float3x3 scaled_k_rect = k_rect;

        auto scale_ratio_x = 1280.0f / static_cast < float >( rect_width );
        auto scale_ratio_y = 720.0f / static_cast< float >( rect_height );
        auto scale_ratio = std::max< float >( scale_ratio_x, scale_ratio_y );

        auto crop_x = ( static_cast< float >( rect_width ) * scale_ratio - 1280.0f ) * 0.5f;
        auto crop_y = ( static_cast< float >( rect_height ) * scale_ratio - 720.0f ) * 0.5f;

        scaled_k_rect( 2, 0 ) = ( scaled_k_rect( 2, 0 ) + 0.5f ) * scale_ratio - crop_x - 0.5f;
        scaled_k_rect( 2, 1 ) = ( scaled_k_rect( 2, 1 ) + 0.5f ) * scale_ratio - crop_y - 0.5f;

        scaled_k_rect( 0, 0 ) = scaled_k_rect( 0, 0 ) * scale_ratio;
        scaled_k_rect( 1, 1 ) = scaled_k_rect( 1, 1 ) * scale_ratio;

        cv::Mat new_camera_matrix = (cv::Mat_<double>(3, 3) << scaled_k_rect( 0, 0 ), scaled_k_rect( 1, 0 ), scaled_k_rect( 2, 0 ),
                                                               scaled_k_rect( 0, 1 ), scaled_k_rect( 1, 1 ), scaled_k_rect( 2, 1 ),
                                                               scaled_k_rect( 0, 2 ), scaled_k_rect( 1, 2 ), scaled_k_rect( 2, 2 ) );
        cv::Size image_size = cv::Size( 1280, 720 );

        // Precompute undistort/rectify maps
        cv::initUndistortRectifyMap( camera_matrix, coeffs, R, new_camera_matrix, image_size, CV_16SC2, _map1, _map2 );

        // Preallocate buffer
        _rectified_buffer.create( image_size, CV_8UC3 );
    }

    bool rectification_filter::should_process( const rs2::frame & frame )
    {
        if( _rectified_buffer.empty() )
            return false; // TODO - throw?

        if( ! frame || frame.is< rs2::frameset >() )
            return false;

        return frame.get_profile().format() == RS2_FORMAT_RGB8 && frame.get_profile().stream_type() == _stream_to_rectify;
    }

    rs2::frame rectification_filter::process_frame( const rs2::frame_source & source, const rs2::frame & f )
    {
        auto vf = f.as< rs2::video_frame >();
        auto profile = f.get_profile().as< rs2::video_stream_profile >();

        int width = vf.get_width();
        int height = vf.get_height();
        int bpp = vf.get_bytes_per_pixel();
        auto ret = source.allocate_video_frame( profile, f, bpp, 1280, 720, 1280 * bpp, RS2_EXTENSION_VIDEO_FRAME );

        // Perform rectification
        uint8_t * src = const_cast< uint8_t * >( reinterpret_cast< const uint8_t * >( vf.get_data() ) );
        uint8_t * dst = const_cast< uint8_t * >( reinterpret_cast< const uint8_t * >( ret.get_data() ) );

        // Remap to rectified image
        cv::Mat rgb_buffer( height, width, CV_8UC3, src );  // Uses the data without copying it
        cv::remap( rgb_buffer, _rectified_buffer, _map1, _map2, cv::INTER_LINEAR );
        memcpy( dst, _rectified_buffer.data, 1280 * 720 * bpp );

        return ret;
    }
    }
