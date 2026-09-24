// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "ds-mipi-device.h"
#include "ds-device-common.h"
#include "error-handling.h"
#include "fw-update/dfu-write.h"

#include <chrono>
#include <thread>


namespace librealsense
{
    ds_mipi_device::ds_mipi_device( std::shared_ptr< ds_device_common > device_common,
                                    std::shared_ptr< polling_error_handler > error_poller )
        : _device_common( std::move( device_common ) )
        , _error_poller( std::move( error_poller ) )
    {
    }

    void ds_mipi_device::perform_dfu_write( const std::string & dfu_path,
                                            const void * fw_image, std::size_t fw_image_size,
                                            rs2_update_progress_callback_sptr progress_callback,
                                            int estimated_seconds,
                                            std::function< void() > before_polling_resume ) const
    {
        options_watcher_pause_guard guard( *_device_common );

        // Pause the 1 Hz error-polling thread over the DFU write; its XU query would share
        // the d4xx I2C bus with the DFU status protocol. RAII resumes on any exit.
        struct poller_gate {
            polling_error_handler * p;
            unsigned interval;
            bool was_active;
            ~poller_gate() {
                if( ! p || ! was_active ) return;
                // start() spins up a dispatcher thread and can throw std::system_error;
                // swallow it here so we never std::terminate from a destructor.
                try { p->start( interval ); }
                catch( const std::exception & e ) { LOG_ERROR( "polling_error_handler restart failed: " << e.what() ); }
                catch( ... ) { LOG_ERROR( "polling_error_handler restart failed (unknown)" ); }
            }
        };
        poller_gate _pg{ _error_poller.get(),
                         _error_poller ? _error_poller->get_polling_interval() : 0,
                         _error_poller && _error_poller->is_active() };
        if( _error_poller ) _error_poller->stop();

        perform_dfu_chardev_write( dfu_path, fw_image, fw_image_size,
                                   progress_callback, estimated_seconds );

        // Keep both options watchers and the error poller paused over the reset
        // window. The D585 disappears from I2C while HKR and GMSL restart.
        if( before_polling_resume )
            before_polling_resume();
    }
}
