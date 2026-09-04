// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <librealsense2/hpp/rs_types.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>


namespace librealsense
{
    class ds_device_common;
    class polling_error_handler;

    // Shared MIPI DFU write helper. Single-shot ofstream write of the whole
    // image; a background thread emits progress ticks and a 30 s heartbeat log
    // while the write blocks and is joined on any exit path via RAII.
    class ds_mipi_device
    {
    public:
        // Pass an error_poller to have it stopped over the DFU write and restarted on any exit;
        // pass nullptr to leave any existing poller running (current D400 behaviour).
        explicit ds_mipi_device( std::shared_ptr< ds_device_common > device_common,
                                 std::shared_ptr< polling_error_handler > error_poller = nullptr );

        void perform_dfu_write( const std::string & dfu_path,
                                const void * fw_image, std::size_t fw_image_size,
                                rs2_update_progress_callback_sptr progress_callback = nullptr,
                                int estimated_seconds = 120,
                                std::function< void() > before_polling_resume = {} ) const;

    private:
        std::shared_ptr< ds_device_common > _device_common;
        std::shared_ptr< polling_error_handler > _error_poller;
    };
}
