// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#pragma once

#include <string>
#include <mutex>
#include <map>
#include <tuple>
#include <utility>


namespace librealsense {
namespace rum {


// Process-wide aggregator for RUM data: SDK metadata plus the per-session arrays
// (devices, streams, options-changed, filters, notifications) filled by the instrumentation
// hooks. Builds the JSON report on demand. Thread-safe.
class rum_collector
{
public:
    static rum_collector & instance();

    // Record a device that was created, aggregated (deduplicated + counted) by
    // (type, firmware version, connection). Safe to call repeatedly.
    void record_device( std::string const & type,
                        std::string const & fw_version,
                        std::string const & connection,
                        std::string const & mipi_driver_version );

    // Record a stream configuration that was opened, aggregated by
    // (stream type, format, resolution, fps). Safe to call repeatedly.
    void record_stream( std::string const & stream_type,
                        std::string const & format,
                        std::string const & resolution,
                        int fps );

    // Add streamed seconds (sensor start->stop) to a stream configuration's running total.
    void record_stream_duration( std::string const & stream_type,
                                 std::string const & format,
                                 std::string const & resolution,
                                 int fps,
                                 double seconds );

    // Record an option set to a value that differs from its default. Aggregates
    // set-count and most-recent value per option name.
    void record_option_change( std::string const & option, float value );

    // Record that a filter actually processed a frame (first time per block), tallied per name.
    void record_filter( std::string const & name );

    // Record a raised notification, tallied per category.
    void record_notification( std::string const & category );

    // Persist the current aggregate to the local store (atomic). Opens no network socket.
    void flush();

    // The live in-memory report as JSON. This is what rs2_rum_get_report returns.
    std::string get_report() const;

private:
    rum_collector();

    struct device_key
    {
        std::string type, fw_version, connection, mipi_driver_version;
        bool operator<( device_key const & o ) const
        {
            return std::tie( type, fw_version, connection, mipi_driver_version )
                 < std::tie( o.type, o.fw_version, o.connection, o.mipi_driver_version );
        }
    };
    struct stream_key
    {
        std::string type, format, resolution;
        int fps;
        bool operator<( stream_key const & o ) const
        {
            return std::tie( type, format, resolution, fps ) < std::tie( o.type, o.format, o.resolution, o.fps );
        }
    };

    mutable std::mutex _mutex;
    std::string const _source_id;  // read from the store (rum.json) or minted at construction; stable across runs
    // Deduplicated device tallies -> count.
    std::map< device_key, int > _device_counts;
    // Deduplicated stream tallies -> (open count, total streamed seconds).
    struct stream_stat { int count = 0; double duration_seconds = 0.0; };
    std::map< stream_key, stream_stat > _stream_counts;
    // Per-option change tallies, keyed by option name -> (set_count, last_value).
    std::map< std::string, std::pair< int, float > > _option_changes;
    // Filter usage tallies (first frame through each block), keyed by filter name -> count.
    std::map< std::string, int > _filter_counts;
    // Notification tallies, keyed by category -> count.
    std::map< std::string, int > _notification_counts;
};


}  // namespace rum
}  // namespace librealsense
