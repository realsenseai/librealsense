// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "nn-loader.h"
#include "nn-protocol.h"

#include <src/librealsense-exception.h>

#include <rsutils/string/from.h>

namespace librealsense {


void validate_nn_upload_args( size_t size, rs2_nn_target target, int slot, bool activate )
{
    if( ! size )
        throw invalid_value_exception( "NN package is empty" );
    if( size > nn::MAX_FILE_SIZE )
        throw invalid_value_exception( rsutils::string::from()
                                       << "NN package is " << size << " bytes; at most " << nn::MAX_FILE_SIZE << " are supported" );
    if( target != RS2_NN_TARGET_FLASH )
        throw not_implemented_exception( "NN upload: only the flash target is supported yet" );
    // Slot 0 takes a weights package, slot 2 a custom network (code + weights); the device checks the package matches
    if( slot != nn::DETECTION_SLOT && slot != nn::CUSTOM_SLOT )
        throw not_implemented_exception( rsutils::string::from() << "NN upload: slot " << slot << " is not supported" );
    if( ! activate )
        throw not_implemented_exception( "NN upload: uploading without activating is not supported yet" );
}


// Object Detection runs a network; Occupancy and the other depth-derived streams don't, so they may stream during an
// upload. Segmentation and Custom NN join this list when they become streams.
bool is_nn_stream( rs2_stream stream )
{
    return stream == RS2_STREAM_OBJECT_DETECTION;
}

}  // namespace librealsense
