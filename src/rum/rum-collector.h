// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#pragma once

#include <string>
#include <mutex>
#include <map>
#include <set>
#include <tuple>
#include <utility>


namespace librealsense {
namespace rum {


// Process-wide collector of RUM data: SDK metadata plus per-session tallies
// (devices, streams, options, filters, notifications) filled by the hooks.
// Builds the JSON report on demand. Thread-safe.
class rum_collector
{
public:
    static rum_collector & instance();

    // Record a created device, counted by (type, fw version, connection, mipi driver).
    // Safe to call repeatedly.
    void record_device( std::string const & type,
                        std::string const & fw_version,
                        std::string const & connection,
                        std::string const & mipi_driver_version );

    // Record an opened stream config, counted by (type, format, resolution, fps).
    // Safe to call repeatedly.
    void record_stream( std::string const & stream_type,
                        std::string const & format,
                        std::string const & resolution,
                        int fps );

    // Add streamed seconds (start->stop) to a stream config's running total.
    void record_stream_duration( std::string const & stream_type,
                                 std::string const & format,
                                 std::string const & resolution,
                                 int fps,
                                 double seconds );

    // Record an option set to a non-default value; tallies set-count and last value per option.
    void record_option_change( std::string const & option, float value );

    // Add a filter name that counts as user-facing post-processing (a sensor's recommended block).
    // record_filter only tallies names added here, so viewer/internal blocks (colorizer, pointcloud,
    // align, format converters, ...) never pollute the report.
    void add_recommended_filter( std::string const & name );

    // Record that a filter processed a frame (first time per block); tallied only if recommended.
    void record_filter( std::string const & name );

    // Record a raised notification, tallied per category.
    void record_notification( std::string const & category );

    // Write the current report to the local file. No network.
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
    std::string const _source_id;   // loaded from rum.json or created at construction; stable across runs
    std::string const _session_id;  // new per run; lets the server dedup a session uploaded twice
    // Deduplicated device tallies -> count.
    std::map< device_key, int > _device_counts;
    // Deduplicated stream tallies -> (open count, total streamed seconds).
    struct stream_stat { int count = 0; double duration_seconds = 0.0; };
    std::map< stream_key, stream_stat > _stream_counts;
    // Per-option change tallies, keyed by option name -> (set_count, last_value).
    std::map< std::string, std::pair< int, float > > _option_changes;
    // Filter usage tallies (first frame through each block), keyed by filter name -> count.
    std::map< std::string, int > _filter_counts;
    // Names that count as user-facing post-processing (sensors' recommended blocks); record_filter
    // ignores anything not in here.
    std::set< std::string > _recommended_filters;
    // Notification tallies, keyed by category -> count.
    std::map< std::string, int > _notification_counts;
};


}  // namespace rum
}  // namespace librealsense
