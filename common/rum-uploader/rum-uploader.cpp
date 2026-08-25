// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#ifdef ENABLE_STATS
#include "../rs-config.h"        // config_file, configurations::stats
#include "../device-model.h"     // configurations, device_model
#include "../ux-window.h"        // ux_window (consent popup font)
#include "../subdevice-model.h"  // subdevice_model::wait_for_stop
#include <librealsense2/rs.hpp>  // rs2::rum::is_cloud_enabled, rs2::rum::get_report_path
#include <rsutils/os/atomic-write-file.h>
#include <rsutils/json.h>
#include <imgui.h>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#endif  // ENABLE_STATS

#include "rum-uploader.h"
#include "../http/http-uploader.h"
#include <rsutils/easylogging/easyloggingpp.h>


namespace rs2 {


#ifndef ENABLE_STATS

// Dummy functions - built without RUM collection/upload; the whole feature compiles to no-ops.
std::string rum_uploader::saved_report() { return std::string(); }
bool rum_uploader::saved_report_has_usage() { return false; }
bool rum_uploader::upload( std::string const & ) { LOG_WARNING( "RUM upload unavailable: built without ENABLE_STATS" ); return false; }
void rum_uploader::start() {}
void rum_uploader::upload_async( std::string, std::function< void( bool ) > ) {}
rum_uploader::~rum_uploader() {}
void rum_uploader::upload_data( ux_window & ) {}
void rum_uploader::join_pending_stops( std::shared_ptr< std::vector< std::unique_ptr< device_model > > > ) {}

#else  // ENABLE_STATS


// ---- tunables ----
// No production endpoint yet; upload to the local dev-server stub (see dev-server/) for now.
// TODO: use the real cloud endpoint once it exists.
static char const * RUM_ENDPOINT = "http://127.0.0.1:8080/v1/rum";
static char const * CONSENT_POPUP_ID = "Help improve RealSense";
static const int  DEFAULT_UPLOAD_INTERVAL_HOURS = 24;   // 0 disables the throttle
static const int  SECONDS_PER_HOUR = 3600;


static void reset_saved_report( rsutils::json const & report )
{
    auto src = report.value( "source_id", std::string() );
    rsutils::os::atomic_write_file( rs2::rum::get_report_path(), rsutils::json{ { "source_id", src } }.dump( 2 ) );
}


static bool report_has_usage( rsutils::json const & report )
{
    return ! report.value( "devices", rsutils::json::object() ).empty()        // devices (streams/options nested)
        || ! report.value( "notifications", rsutils::json::array() ).empty();  // notifications (top-level)
}


std::string rum_uploader::saved_report()
{
    // Read the file exactly as saved (binary = no newline translation).
    std::ifstream f( rs2::rum::get_report_path(), std::ios::binary );
    if( ! f )
        return std::string();
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}


bool rum_uploader::saved_report_has_usage()
{
    try { return report_has_usage( rsutils::json::parse( saved_report() ) ); }
    catch( ... ) { return false; }  // no file / unparseable / reset stub -> nothing to send
}


bool rum_uploader::upload( std::string const & json_report )
{
    // Refuse to send without consent, so no caller can leak data by forgetting to check.
    if( ! rs2::rum::is_cloud_enabled() )
    {
        LOG_WARNING( "RUM upload refused: cloud upload not consented" );
        return false;
    }

    // All HTTP/curl lives in http_uploader; we just hand it the endpoint and body.
    http::http_uploader uploader;
    return uploader.upload( RUM_ENDPOINT, json_report );
}


static void run_boot_upload()
{
    try
    {
        if( ! rs2::rum::is_cloud_enabled() )
            return;

        // Throttle boot uploads to at most once per interval, read from config (no UI; default
        // 24h, 0 disables). The last-upload time persists in the same config file.
        auto & cfg = config_file::instance();
        int interval_hours = cfg.get_or_default( configurations::stats::rum_upload_interval_hours, DEFAULT_UPLOAD_INTERVAL_HOURS );
        auto now = std::chrono::duration_cast< std::chrono::seconds >(
                       std::chrono::system_clock::now().time_since_epoch() ).count();
        long long last = cfg.get_or_default< long long >( configurations::stats::rum_last_upload, 0 );
        if( last > now )
            last = now;  // future timestamp (clock skew / corrupt config) - invalidate to now
        if( interval_hours > 0 && now - last < (long long)interval_hours * SECONDS_PER_HOUR )
            return;  // uploaded recently

        auto report = rum_uploader::saved_report();   // sessions accumulated on disk since the last upload
        rsutils::json j;
        try { j = rsutils::json::parse( report ); }
        catch( ... ) { return; }  // no file / unparseable -> nothing to send
        if( ! report_has_usage( j ) )
            return;  // only source_id left after a reset, or empty
        if( rum_uploader::upload( report ) )
        {
            reset_saved_report( j );  // clear the delivered batch; keep source_id
            cfg.set( configurations::stats::rum_last_upload, now );
            LOG_INFO( "RUM report uploaded to " << RUM_ENDPOINT );
        }
    }
    catch( std::exception const & e ) { LOG_ERROR( "RUM upload error: " << e.what() ); }
}


void rum_uploader::start()
{
    if( _uploading.exchange( true ) )
        return;  // an upload (boot or manual) is already running; don't overwrite it or block the UI thread
    if( _thread.joinable() )
        _thread.join();  // the previous worker has finished (gate was clear); join is instant
    _thread = std::thread( [this]() { run_boot_upload(); _uploading = false; } );
}


void rum_uploader::upload_async( std::string report, std::function< void( bool ) > on_done )
{
    if( _uploading.exchange( true ) )
        return;  // an upload is already running; don't block the caller
    if( _thread.joinable() )
        _thread.join();  // the previous worker has finished (gate was clear); join is instant
    _thread = std::thread( [this, report = std::move( report ), on_done = std::move( on_done )]()
    {
        bool ok = false;
        try
        {
            ok = upload( report );
            if( ok )
                LOG_INFO( "RUM report uploaded to " << RUM_ENDPOINT );
            else
                LOG_ERROR( "RUM upload failed" );
        }
        catch( std::exception const & e ) { LOG_ERROR( "RUM upload error: " << e.what() ); }
        if( on_done )
            on_done( ok );
        _uploading = false;
    } );
}


rum_uploader::~rum_uploader()
{
    if( _thread.joinable() )
        _thread.join();
}


static void draw_consent_popup( rum_uploader & uploader, ux_window & window )
{
    return;  // TODO: remove this when server side is ready
    ImGui::SetNextWindowSize( { 460.f, 0.f } );
    if( ! ImGui::BeginPopupModal( CONSENT_POPUP_ID, nullptr,
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove ) )
        return;

    ImGui::PushFont( window.get_large_font() );
    ImGui::Text( "%s", CONSENT_POPUP_ID );
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextWrapped( "Share anonymous usage statistics (devices, stream configs, options, "
                        "and errors) to help us prioritize fixes and features." );
    ImGui::Spacing();
    ImGui::TextWrapped( "No personal data, serial numbers, or image content is ever collected. "
                        "You can change this any time in Settings > Online Services." );
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if( ImGui::Button( "Yes, enable", ImVec2( 150, 30 ) ) )
    {
        config_file::instance().set( configurations::stats::rum_cloud_enabled, true );
        uploader.start();  // upload any saved report now (e.g. re-consent after a reset); no-op on a true first run
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if( ImGui::Button( "No thanks", ImVec2( 150, 30 ) ) )
    {
        config_file::instance().set( configurations::stats::rum_cloud_enabled, false );
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}


void rum_uploader::upload_data( ux_window & window )
{
    // First-run consent: decided once (missing -> ask; true/false -> silent). Persists immediately.
    static bool startup_done = false;
    if( ! startup_done )
    {
        if( ! config_file::instance().contains( configurations::stats::rum_cloud_enabled ) )
            ImGui::OpenPopup( CONSENT_POPUP_ID );  // first run: ask; nothing saved yet, so no upload this session
        else
            start();  // background-upload the previous session's saved report
        startup_done = true;
    }
    draw_consent_popup( *this, window );
}


void rum_uploader::join_pending_stops( std::shared_ptr< std::vector< std::unique_ptr< device_model > > > device_models )
{
    // stop() runs asynchronously and is where streamed-duration is recorded, so join any pending
    // stop here; the session is persisted automatically when the SDK context is destroyed.
    for( auto && dm : *device_models )
        for( auto && sub : dm->subdevices )
            try { sub->wait_for_stop(); } catch( ... ) {}
}


#endif  // ENABLE_STATS


}  // namespace rs2
