// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#pragma once

#include <string>
#include <mutex>
#include <map>
#include <set>
#include <utility>


namespace librealsense {
namespace rum {


// Path of the on-disk report file (<app-data>/rum/rum.json) — the single source of truth for the
// location; the viewer asks for it via the C-API rather than re-deriving it.
std::string report_path();


// Process-wide collector of RUM data: SDK metadata plus per-device tallies filled by the hooks.
// The report nests everything under the device it belongs to: device -> streams -> filters, and
// device -> options. Builds the JSON report on demand. Thread-safe.
class rum_collector
{
public:
    static rum_collector & instance();

    // Record a created device, keyed by "<name>-<connection>"; connection / fw / mipi are stored as
    // values. `serial` is used ONLY in memory to count distinct units this session (reconnects of
    // the same serial don't recount); it is never serialized or uploaded - only the count is.
    void record_device( std::string const & device_key,
                        std::string const & connection,
                        std::string const & fw_version,
                        std::string const & mipi_driver_version,
                        std::string const & serial );

    // Record an opened stream config under its device, keyed by a stream label.
    void record_stream( std::string const & device_key, std::string const & stream_label );

    // Add streamed seconds (start->stop) to a stream's running total, under its device.
    void record_stream_duration( std::string const & device_key, std::string const & stream_label, double seconds );

    // Record an option set to a non-default value, under its device; tallies set-count and last value.
    void record_option_change( std::string const & device_key, std::string const & option, float value );

    // Add a filter name that counts as user-facing post-processing (a sensor's recommended block).
    void add_recommended_filter( std::string const & name );

    // Record that a recommended filter processed a frame, attributed to its device. Non-recommended ignored.
    void record_filter( std::string const & device_key, std::string const & name );

    // Record a raised notification, tallied per category (top-level, not per-device).
    void record_notification( std::string const & category );

    // Write the current report to the local file. No network.
    void flush();

    // The current in-memory report as JSON. flush() writes this to the on-disk report file.
    std::string get_report() const;

private:
    rum_collector();

    // Fold a not-yet-uploaded report already on disk into the in-memory tallies, so data
    // accumulates across sessions until a successful upload resets the file. Called from flush().
    void merge_saved_report();

    struct stream_stat
    {
        int count = 0;
        double duration_seconds = 0.0;
    };
    struct device_stat
    {
        std::string connection, fw_version, mipi_driver_version;
        int count = 0;                        // peak distinct units, carried (max-merged) from prior batches
        bool seen_this_session = false;       // RAM only
        std::set< std::string > serials;      // distinct serials seen this session; RAM only, never serialized
        std::map< std::string, stream_stat > streams;                    // stream label -> stat
        std::map< std::string, std::pair< int, float > > options;        // option -> (set_count, last_value)
        std::map< std::string, int > filters;                            // recommended filter name -> use count
    };

    mutable std::mutex _mutex;
    bool _merged_from_disk = false;  // fold the prior on-disk report in once per process, not per flush
    std::string const _source_id;   // loaded from rum.json or created at construction; stable across runs
    std::string const _session_id;  // new per run; lets the server dedup a report uploaded twice
    std::map< std::string, device_stat > _devices;    // "<name>-<connection>" -> tallies
    std::set< std::string > _recommended_filters;     // gate for record_filter
    std::map< std::string, int > _notification_counts;
};


}  // namespace rum
}  // namespace librealsense
