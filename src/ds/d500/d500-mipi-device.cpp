// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "d500-mipi-device.h"
#include "ds/ds-device-common.h"
#include "fw-update/dfu-write.h"

#include <chrono>
#include <thread>

namespace librealsense
{
    d500_mipi_device::d500_mipi_device( const std::string & dfu_device_path,
                                        std::shared_ptr< ds_device_common > device_common,
                                        std::shared_ptr< polling_error_handler > error_poller )
        : _dfu_device_path( dfu_device_path )
        , _device_common( device_common )
        , _mipi( std::move( device_common ), std::move( error_poller ) )
    {
    }

    void d500_mipi_device::update( const void * fw_image, int fw_image_size,
                                   rs2_update_progress_callback_sptr update_progress_callback ) const
    {
        _mipi.perform_dfu_write( _dfu_device_path, fw_image,
                                 static_cast< std::size_t >( fw_image_size ),
                                 update_progress_callback,
                                 estimate_dfu_seconds( static_cast< std::size_t >( fw_image_size ) ),
                                 []() {
                                     // Wait inside perform_dfu_write's pause guards so the
                                     // poller and options-watchers stay quiet while HKR
                                     // completes its dfuMANIFEST_WAIT_RESET → dfuIDLE reboot.
                                     // HWRST mid-manifest-reset is treated as a recovery
                                     // request, so we defer it past this window.
                                     std::this_thread::sleep_for( std::chrono::seconds( 10 ) );
                                 } );

        // Run HWRST after perform_dfu_write returns so ds_device_common::
        // hardware_reset()'s inner options_watcher_pause_guard is not nested
        // inside the outer one (plain-bool pause would unpause the surroundings
        // on the inner dtor).
        hardware_reset();
        std::this_thread::sleep_for( std::chrono::seconds( 5 ) );
        if( update_progress_callback )
            update_progress_callback->on_update_progress( 1.f );
    }

    void d500_mipi_device::hardware_reset() const
    {
        _device_common->hardware_reset( std::chrono::seconds( 5 ) );
    }
}
