// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/core/extension.h>

#include <librealsense2/hpp/rs_types.hpp>

#include <string>

namespace librealsense {

// A device that accepts NN model packages
class nn_loader_interface
{
public:
    virtual ~nn_loader_interface() = default;

    // False when the device firmware or link cannot take an upload at all
    virtual bool supports_nn_upload() const = 0;

    virtual void nn_upload( const uint8_t * file, size_t size, rs2_nn_target target, int slot, bool activate,
                            rs2_update_progress_callback_sptr callback ) = 0;

    // Whether the device supports an upload (product, firmware, not locked); otherwise fills the reason
    virtual bool is_upload_supported( std::string & reason ) = 0;
};

MAP_EXTENSION( RS2_EXTENSION_NN_LOADER, nn_loader_interface );

// Rejects the arguments the current protocol cannot carry, before any device traffic
void validate_nn_upload_args( size_t size, rs2_nn_target target, int slot, bool activate );

}  // namespace librealsense
