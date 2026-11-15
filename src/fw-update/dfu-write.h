// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <librealsense2/hpp/rs_types.hpp>

#include <cstddef>
#include <string>


namespace librealsense
{
    // Rig-derived progress-bar estimate for the MIPI/GMSL DFU chardev write.
    // Derived from image size using the D5xx HKR throughput figure (see the
    // definition for the constants and rationale). Used by both the operational
    // and recovery paths so the DFU rig number stays in one place.
    int estimate_dfu_seconds( std::size_t fw_image_size );

    // Shared MIPI/GMSL DFU chardev write. One std::ofstream::write() of the whole
    // firmware image; a background thread ticks progress every 500 ms capped at
    // 0.99f from elapsed/estimated_seconds and logs a 30 s heartbeat.
    // Throws io_exception on any I/O failure and is safe to unwind through
    // (the heartbeat thread is joined by RAII on every exit path).
    //
    // The terminal on_update_progress(1.f) is the caller's responsibility so
    // the viewer's DFU bar hits 100% only after any post-write recovery step
    // the caller owns (HW reset, GMSL relink) has completed.
    void perform_dfu_chardev_write( const std::string & dfu_path,
                                    const void * fw_image, std::size_t fw_image_size,
                                    rs2_update_progress_callback_sptr progress_callback,
                                    int estimated_seconds );
}
