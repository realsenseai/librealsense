// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#pragma once

#include <librealsense2/h/rs_option.h>  // rs2_option
#include <librealsense2/h/rs_types.h>   // rs2_notification_category

#include <memory>
#include <vector>
#include <string>


namespace librealsense {


class device_interface;
class stream_profile_interface;
class processing_block_interface;
class options_interface;


// Instrumentation facade. Call sites (in the C API / core) invoke these one-liners;
// all data extraction and aggregation lives here and in the collector, so adding or
// changing a reported field never reopens the call site. Each is no-op-safe: when
// ENABLED_STATS is off every hook returns immediately (via RETURN_IF_NO_RUM in the .cpp),
// so call sites need no guard of their own.
namespace rum {
namespace hooks {


// A device was created — record its type, firmware version, connection and MIPI driver version.
void on_device( device_interface & dev );

// A sensor was opened with these stream profiles — record each configuration.
void on_open( std::vector< std::shared_ptr< stream_profile_interface > > const & profiles );

// A sensor stopped after streaming `seconds` — add that to each active profile's running total.
void on_stream_duration( std::vector< std::shared_ptr< stream_profile_interface > > const & profiles, double seconds );

// An option was set — recorded only when the value differs from its default AND the
// target is a device sensor (processing-block options are ignored as internal noise).
void on_set_option( options_interface & target, rs2_option option, float value, float default_value );

// A processing block actually processed a frame (reported once per block) — recorded only if
// its name is one of the known SDK post-processing filters (internal blocks: syncer, converters,
// custom, are ignored). Counts real usage, not mere construction (the viewer builds its whole
// recommended set regardless of whether the user enables them).
void on_filter( std::string const & name );

// A notification was raised — record it by category.
void on_notification( rs2_notification_category category );

// A context (an SDK session) is being torn down — persist the collected report to the local
// store. Called from the context destructor, so it never throws.
void on_context_closed() noexcept;


}  // namespace hooks
}  // namespace rum
}  // namespace librealsense
