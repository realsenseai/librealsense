// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "ds-mipi-device.h"
#include "ds-device-common.h"
#include "error-handling.h"
#include "librealsense-exception.h"
#include "types.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <string>
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

        std::ofstream fw_path_in_device( dfu_path.c_str(), std::ios::binary );
        if( ! fw_path_in_device )
            throw io_exception( "Firmware Update failed - wrong path or permissions missing: " + dfu_path );

        // Progress + heartbeat thread; RAII joiner covers the throw path too.
        std::atomic< bool > done{ false };
        std::thread heartbeat( [&]() {
            auto start = std::chrono::steady_clock::now();
            auto last_log = start;
            while( ! done.load() )
            {
                std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
                auto now = std::chrono::steady_clock::now();
                int elapsed = int( std::chrono::duration_cast< std::chrono::seconds >( now - start ).count() );
                if( progress_callback )
                    progress_callback->on_update_progress(
                        std::min( float( elapsed ) / float( estimated_seconds ), 0.99f ) );
                if( now - last_log >= std::chrono::seconds( 30 ) )
                {
                    LOG_INFO( "MIPI DFU in progress: elapsed " << elapsed << " s" );
                    last_log = now;
                }
            }
        } );
        struct joiner { std::atomic< bool > & d; std::thread & t; ~joiner() { d = true; if( t.joinable() ) t.join(); } };
        joiner _j{ done, heartbeat };

        fw_path_in_device.write( reinterpret_cast< const char * >( fw_image ), fw_image_size );
        if( ! fw_path_in_device )
            throw io_exception( "Firmware Update failed - DFU chardev write error: " + dfu_path );

        fw_path_in_device.close();
        if( ! fw_path_in_device )
            throw io_exception( "Firmware Update failed - DFU chardev flush/close error: " + dfu_path );

        // Stop the heartbeat here. The terminal on_update_progress(1.0f) is the
        // caller's responsibility — it must fire only after the caller's own
        // post-write recovery (HW reset, GMSL relink) has completed, so the
        // viewer's DFU dialog reports 100% at the moment the device is truly
        // back, not the moment the DFU chardev closes.
        done = true;
        if( heartbeat.joinable() )
            heartbeat.join();
        LOG_INFO( "MIPI DFU write complete for " << dfu_path );

        // Keep both options watchers and the error poller paused over the reset
        // window. The D585 disappears from I2C while HKR and GMSL restart.
        if( before_polling_resume )
            before_polling_resume();
    }
}
