// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#pragma once

#include "nn-info.h"
#include <src/core/video-frame.h>

#include <memory>
#include <mutex>

namespace librealsense {

// The output tensors of a custom network, parsed on first access; the raw 1024 x H frame stays available
class custom_nn_frame : public video_frame
{
public:
    custom_nn_frame() = default;
    custom_nn_frame( custom_nn_frame && other );
    custom_nn_frame & operator=( custom_nn_frame && other );

    // Describes the tensors when its generation is the frame's; set before the first access
    void set_description( std::shared_ptr< const nn::model_description > description ) { _description = std::move( description ); }

    // False when the frame can't be parsed (logged once per frame)
    bool is_valid() const { return layout() != nullptr; }
    size_t get_tensor_count() const;
    rs2_nn_tensor get_tensor( size_t index ) const;
    uint8_t get_status() const;
    const char * get_status_message() const;

    bool find_metadata( rs2_frame_metadata_value, rs2_metadata_type * p_output_value ) const override;

private:
    struct parsed
    {
        nn::custom_nn_frame_layout layout;
        std::vector< std::vector< int > > shapes;
        std::vector< const nn::output_description * > outputs;  // null for a tensor exposed raw
    };
    parsed const * layout() const;

    mutable std::mutex _parse_mutex;
    mutable bool _parse_done = false;
    mutable std::unique_ptr< parsed > _parsed;
    std::shared_ptr< const nn::model_description > _description;
};

MAP_EXTENSION( RS2_EXTENSION_CUSTOM_NN_FRAME, librealsense::custom_nn_frame );

}  // namespace librealsense
