// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "d500-device.h"
#include <src/nn/nn-loader.h>

#include <mutex>

namespace librealsense
{
    // Uploads NN model packages over the HW monitor: D585 over USB only
    class d500_nn_loader
        : public virtual d500_device
        , public nn_loader_interface
    {
    public:
        d500_nn_loader( std::shared_ptr< const d500_info > const & dev_info );

        bool supports_nn_upload() const override;
        void nn_upload( const uint8_t * file, size_t size, rs2_nn_target target, int slot, bool activate,
                        rs2_update_progress_callback_sptr callback ) override;
        bool is_upload_supported( std::string & reason ) override;

        // The device refuses a FW update during an install
        void enter_update_state() const override;

    private:
        // Empty if this camera's product, connection and FW support the upload; otherwise why not
        std::string unsupported_reason() const;
        std::string firmware_reason() const;
        bool is_locked_now() const;
        std::string locked_reason() const;

        std::mutex _nn_upload_mutex;
    };
}
