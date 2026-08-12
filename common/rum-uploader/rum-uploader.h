// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>


namespace rs2 {


class ux_window;
class device_model;


// RUM uploader, linked into the viewer. Owns the background upload thread and joins it in the
// destructor, so a stray exception can't leave a thread running at shutdown.
class rum_uploader
{
public:
    rum_uploader() = default;
    ~rum_uploader();
    rum_uploader( rum_uploader const & ) = delete;
    rum_uploader & operator=( rum_uploader const & ) = delete;

    // The accumulated report saved to disk (<app-data>/rum/rum.json), or "" if none — this is what
    // the boot upload ships. The SDK writes it on context destruction (rs2::rum::get_report_path()).
    static std::string saved_report();

    // True if the saved report actually carries usage (devices or notifications). False for a
    // missing file or a post-upload reset stub ({source_id} only) — nothing worth uploading/exporting.
    static bool saved_report_has_usage();

    // POST the report over HTTP(S); true on success. No-op returning false when built without HTTP.
    // This is the call that actually sends data off the machine.
    static bool upload( std::string const & json_report );

    // Returns immediately. If consented and a saved report exists, uploads it on a background
    // thread. The server dedups repeats via the report's session_id.
    void start();

    // Returns immediately. Uploads `report` on a background thread; skips if one is already running
    // so a UI button never blocks on a slow transfer. `on_done( ok )`, if given, runs on that thread
    // when the upload finishes (not called on the skip).
    void upload_async( std::string report, std::function< void( bool ) > on_done = {} );

    // Show consent popup if consent not set, upload if consent granted, no-op if rejected.
    void upload_data( ux_window & window );

    // On shutdown, join any pending async sensor stops so streamed-duration is recorded before teardown.
    static void join_pending_stops( std::shared_ptr< std::vector< std::unique_ptr< device_model > > > device_models );

private:
    std::thread _thread;
    std::atomic< bool > _uploading{ false };
};


}  // namespace rs2
