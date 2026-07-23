// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#pragma once

#include <librealsense2/h/rs_option.h>  // rs2_option
#include <librealsense2/h/rs_types.h>   // rs2_notification_category
#include <rsutils/time/stopwatch.h>

#include <memory>
#include <vector>
#include <string>
#include <chrono>


namespace librealsense {


class device_interface;
class stream_profile_interface;
class options_interface;


// Instrumentation facade. Call sites invoke these one-liners; all data extraction lives here
// and in the collector, so changing a reported field never touches the call site. When
// ENABLE_STATS is off these become inline no-ops, so call sites need no guard of their own.
namespace rum {
namespace hooks {


#ifdef ENABLE_STATS

// A device was created — record its type, firmware version, connection and MIPI driver version.
void on_device( device_interface & dev );

// A sensor was opened with these stream profiles — record each configuration.
void on_open( std::vector< std::shared_ptr< stream_profile_interface > > const & profiles );

// A sensor stopped after streaming `seconds` — add that to each active profile's running total.
void on_stream_duration( std::vector< std::shared_ptr< stream_profile_interface > > const & profiles, double seconds );

// An option was set — recorded only when the value is non-default and the target is a
// device sensor (processing-block options are ignored).
void on_set_option( options_interface & target, rs2_option option, float value, float default_value );

// A processing block processed a frame (once per block) — real usage, not construction.
// Restricting to recommended filters is left to the consumer.
void on_filter( std::string const & name );

// A notification was raised — record it by category.
void on_notification( rs2_notification_category category );

// An SDK session (context) is closing — save the report to the local file. Called from the
// context destructor, so it never throws.
void on_context_closed() noexcept;

#else  // ENABLE_STATS — inline no-ops so call sites compile away when stats are disabled

inline void on_device( device_interface & ) {}
inline void on_open( std::vector< std::shared_ptr< stream_profile_interface > > const & ) {}
inline void on_stream_duration( std::vector< std::shared_ptr< stream_profile_interface > > const &, double ) {}
inline void on_set_option( options_interface &, rs2_option, float, float ) {}
inline void on_filter( std::string const & ) {}
inline void on_notification( rs2_notification_category ) {}
inline void on_context_closed() noexcept {}

#endif  // ENABLE_STATS


}  // namespace hooks


// Times a sensor's streaming intervals and reports each to RUM. A sensor holds one of these
// instead of a raw stopwatch: restart() when streaming begins, record() when it ends (a no-op
// unless actually streaming, so re-start/close/teardown never double-count or drop an interval).
class stream_timer
{
public:
    void restart() { _sw.reset(); }

    void record( bool streaming,
                 std::vector< std::shared_ptr< stream_profile_interface > > const & active )
    {
        if( streaming )
            hooks::on_stream_duration( active, std::chrono::duration< double >( _sw.get_elapsed() ).count() );
    }

private:
    rsutils::time::stopwatch _sw;
};


}  // namespace rum
}  // namespace librealsense
