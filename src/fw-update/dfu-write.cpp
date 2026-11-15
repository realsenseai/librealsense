// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "dfu-write.h"
#include "../librealsense-exception.h"
#include "../types.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <thread>


namespace librealsense
{
    int estimate_dfu_seconds( std::size_t fw_image_size )
    {
        // Rig-measured D5xx GMSL DFU throughput: ~13 s per 128 KiB chunk (HKR
        // limits the DFU status protocol pace, not the I2C bus). Derive the
        // progress-bar estimate from image size so both full (~19 MB, ~30 min)
        // and compressed (~7 MB, ~12 min) images map to real elapsed time.
        // D4xx recovery writes finish sooner; the heartbeat bar creeps to 0.99
        // and holds — safer than pinning too early.
        constexpr std::size_t DFU_CHUNK_BYTES = 128 * 1024;
        constexpr int         SEC_PER_CHUNK   = 13;
        std::size_t chunks = ( fw_image_size + DFU_CHUNK_BYTES - 1 ) / DFU_CHUNK_BYTES;
        return std::max( 1, static_cast< int >( chunks ) * SEC_PER_CHUNK );
    }

    void perform_dfu_chardev_write( const std::string & dfu_path,
                                    const void * fw_image, std::size_t fw_image_size,
                                    rs2_update_progress_callback_sptr progress_callback,
                                    int estimated_seconds )
    {
        // Heartbeat divides by estimated_seconds; a caller-side 0 or negative
        // would render inf/-inf and jam the bar at 0.99f from the first tick.
        estimated_seconds = std::max( 1, estimated_seconds );

        std::ofstream fw_path_in_device( dfu_path.c_str(), std::ios::binary );
        if( ! fw_path_in_device )
            throw io_exception( "Firmware Update failed - wrong path or permissions missing: " + dfu_path );

        // Nested scope so the RAII joiner tears down the heartbeat thread — the
        // sole owner of the join — before the "write complete" log fires; no
        // straggling "MIPI DFU in progress" tick can then land after it.
        {
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
        }
        LOG_INFO( "MIPI DFU write complete for " << dfu_path );
    }
}
