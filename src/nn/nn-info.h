// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "nn-protocol.h"

#include <memory>
#include <string>
#include <vector>

namespace librealsense {
namespace nn {

// One output of a custom network, as its metadata (format custom_nn_v1) describes it
struct output_description
{
    std::string name;
    rs2_nn_dtype dtype;
    std::vector< int > sizes;  // after a leading N, in the order `format` gives
    std::string format;        // e.g. NHWC
    std::vector< int > strides;  // in elements
    uint32_t buffer_size;
    float dequant_scale;  // the raw value divided by it gives the real value
};

struct model_description
{
    uint32_t generation;  // CRC-32 of the metadata, as in each frame the model makes
    std::string model;
    std::vector< output_description > outputs;  // by the index the frame's tensors carry
};

// Throws invalid_value_exception when the metadata isn't custom_nn_v1 or is malformed
std::shared_ptr< const model_description > parse_model_description( const std::string & metadata, uint32_t generation );

// NN_INFO as JSON: present, model_state, generation, frame_size, frame_height, and the metadata when present
std::string to_json( const stored_model & stored );

}  // namespace nn
}  // namespace librealsense
