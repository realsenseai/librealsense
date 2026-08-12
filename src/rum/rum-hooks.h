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
class frame_interface;


// When ENABLE_STATS is off these become inline no-ops — call sites need no guard of their own
namespace rum {
namespace hooks {


#ifdef ENABLE_STATS

// A device was created — record its type, firmware, connection and MIPI driver, keyed per device.
void on_device( device_interface & dev );

// A sensor was opened with these stream profiles — record each config under its device.
void on_open( device_interface & dev, std::vector< std::shared_ptr< stream_profile_interface > > const & profiles );

// A sensor stopped after streaming `seconds` — add that to each active profile's total, under its device.
void on_stream_duration( device_interface & dev, std::vector< std::shared_ptr< stream_profile_interface > > const & profiles, double seconds );

// An option was set — recorded only when non-default and on a device sensor (the device is taken
// from that sensor); processing-block options are ignored.
void on_set_option( options_interface & target, rs2_option option, float value, float default_value );

// A processing block processed a frame (once per block) — attributed to the frame's owning device
// (frame -> sensor -> device). Restricting to recommended filters is done in the collector.
void on_filter( std::string const & name, frame_interface & f );

// A notification was raised — record it by category (top-level).
void on_notification( rs2_notification_category category );

// An SDK session (context) is closing — save the report to the local file. Called from the
// context destructor, so it never throws.
void on_context_closed() noexcept;

#else  // ENABLE_STATS — inline no-ops so call sites compile away when stats are disabled

inline void on_device( device_interface & ) {}
inline void on_open( device_interface &, std::vector< std::shared_ptr< stream_profile_interface > > const & ) {}
inline void on_stream_duration( device_interface &, std::vector< std::shared_ptr< stream_profile_interface > > const &, double ) {}
inline void on_set_option( options_interface &, rs2_option, float, float ) {}
inline void on_filter( std::string const &, frame_interface & ) {}
inline void on_notification( rs2_notification_category ) {}
inline void on_context_closed() noexcept {}

#endif  // ENABLE_STATS


}  // namespace hooks


// Times a sensor's streaming intervals and reports each to RUM. A sensor holds one of these instead
// of a raw stopwatch: restart() when streaming begins, record() when it ends (a no-op unless actually
// streaming). record() takes the sensor's device so the duration lands under the right device.
class stream_timer
{
public:
    void restart() { _sw.reset(); }

    void record( bool streaming,
                 device_interface & dev,
                 std::vector< std::shared_ptr< stream_profile_interface > > const & active )
    {
        if( streaming )
            hooks::on_stream_duration( dev, active, std::chrono::duration< double >( _sw.get_elapsed() ).count() );
    }

private:
    rsutils::time::stopwatch _sw;
};


}  // namespace rum
}  // namespace librealsense
