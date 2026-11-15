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
    if( slot != 0 )
        throw not_implemented_exception( "NN upload: only slot 0 is supported yet" );
    if( ! activate )
        throw not_implemented_exception( "NN upload: uploading without activating is not supported yet" );
}

}  // namespace librealsense
