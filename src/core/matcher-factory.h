// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#pragma once

#include <librealsense2/h/rs_types.h>

#include <vector>
#include <memory>


namespace librealsense {


class stream_interface;
class matcher;


class matcher_factory
{
public:
    static std::shared_ptr< matcher > create( rs2_matchers matcher,
                                              std::vector< stream_interface * > const & profiles );

private:
    static std::shared_ptr< matcher > create_DLR_C_matcher( std::vector< stream_interface * > const & profiles );
    static std::shared_ptr< matcher > create_DLR_matcher( std::vector< stream_interface * > const & profiles );
    static std::shared_ptr< matcher > create_DI_C_matcher( std::vector< stream_interface * > const & profiles );
    static std::shared_ptr< matcher > create_DI_matcher( std::vector< stream_interface * > const & profiles );
    static std::shared_ptr< matcher > create_DIC_matcher( std::vector< stream_interface * > const & profiles );
    static std::shared_ptr< matcher > create_DIC_C_matcher( std::vector< stream_interface * > const & profiles );

    static std::shared_ptr< matcher > create_identity_matcher( stream_interface * profiles );
    static std::shared_ptr< matcher > create_frame_number_matcher( std::vector< stream_interface * > const & profiles );
    static std::shared_ptr< matcher > create_timestamp_matcher( std::vector< stream_interface * > const & profiles );

    static std::vector< stream_interface * > get_color_profiles( std::vector< stream_interface * > const & profiles );
    static std::vector< stream_interface * > get_inference_profiles( std::vector< stream_interface * > const & profiles );
    static std::shared_ptr< matcher >
        create_color_composite_matcher( std::vector< stream_interface * > const & profiles );
    static std::shared_ptr< matcher >
        create_timestamp_composite_matcher( std::vector< std::shared_ptr< matcher > > const & matchers );
    static std::shared_ptr< matcher >
        create_frame_number_composite_matcher( std::vector< std::shared_ptr< matcher > > const & matchers );
};


}  // namespace librealsense