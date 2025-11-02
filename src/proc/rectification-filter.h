// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/proc/synthetic-stream.h>
#include <rsutils/number/float3.h>

#include <opencv2/opencv.hpp>

namespace librealsense
{
    class rectification_filter : public stream_filter_processing_block
    {
    public:
        rectification_filter( rsutils::number::float3x3 & k_distorted,
                              std::vector< float > & dist_coeffs,
                              rsutils::number::float3x3 & rotation_mat,
                              rsutils::number::float3x3 & k_rect );

    protected:
        rs2::frame process_frame(const rs2::frame_source& source, const rs2::frame& f) override;
        bool should_process(const rs2::frame& frame) override;

    private:
        cv::Mat _map1;
        cv::Mat _map2;
        cv::Mat _rectified_buffer;
        
        // Rectification parameters
        cv::Mat _camera_matrix;
        cv::Mat _dist_coeffs;
        cv::Mat _R;
        cv::Mat _new_camera_matrix;
        cv::Size _image_size;
    };
    
    MAP_EXTENSION(RS2_EXTENSION_RECTIFICATION_FILTER, librealsense::rectification_filter);
}
