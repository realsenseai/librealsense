// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "rum-collector.h"
#include "rum-config.h"

#include <librealsense2/rs.h>  // RS2_API_VERSION_STR

#include <rsutils/os/atomic-write-file.h>
#include <rsutils/os/special-folder.h>
#include <rsutils/os/os.h>  // get_os_name, cpu_arch
#include <rsutils/json.h>
#include <rsutils/json-config.h>
#include <rsutils/easylogging/easyloggingpp.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <chrono>
#include <vector>


#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

using json = rsutils::json;


namespace librealsense {
namespace rum {

static constexpr int rum_schema_version = 1;

// The server rejects a body over this; an uncapped report would grow past it and never be sent again.
static constexpr size_t max_report_bytes = 64 * 1024;


// Report file: <app-data>/rum/rum.json. Forward slashes work on Windows and POSIX.
std::string report_path()
{
    return rsutils::os::get_special_folder( rsutils::os::special_folder::app_data ) + "rum/rum.json";
}


namespace {


// Create the report's parent directory; benign if it already exists.
void ensure_report_directory()
{
    auto dir = rsutils::os::get_special_folder( rsutils::os::special_folder::app_data ) + "rum";
#ifdef _WIN32
    CreateDirectoryA( dir.c_str(), nullptr );
#else
    mkdir( dir.c_str(), 0700 );
#endif
}


// Random anonymous id, formatted to look like a UUID.
std::string generate_source_id()
{
    std::random_device rd;
    // Draw the random values first; snprintf may read its arguments in any order.
    unsigned a = rd(), b = rd() & 0xFFFF, c = rd() & 0xFFFF, d = rd() & 0xFFFF, e = rd() & 0xFFFF, f = rd();
    char buf[37];
    std::snprintf( buf, sizeof( buf ), "%08x-%04x-%04x-%04x-%04x%08x", a, b, c, d, e, f );
    return std::string( buf );
}


// Reuse the id saved in rum.json, or make a new one on first run. Kept out of
// realsense-config.json so the viewer's config writes can't overwrite it.
std::string load_or_create_source_id()
{
    try
    {
        auto id = rsutils::json_config::load_from_file( report_path() )
                      .nested( "source_id", &json::is_string ).string_ref_or_empty();
        if( ! id.empty() )
            return id;
    }
    catch( ... )
    {
    }
    return generate_source_id();
}

char const * build_type()
{
#ifdef NDEBUG
    return "Release";
#else
    return "Debug";
#endif
}

// Build-time configuration, read from the SDK's existing compile macros (no RUM-specific defines).
#ifdef BUILD_WITH_DDS
constexpr bool cmake_build_with_dds = true;
#else
constexpr bool cmake_build_with_dds = false;
#endif

#ifdef RS2_USE_CUDA
constexpr bool cmake_build_with_cuda = true;
#else
constexpr bool cmake_build_with_cuda = false;
#endif

#ifdef ENABLE_STATS
constexpr bool cmake_enable_stats = true;
#else
constexpr bool cmake_enable_stats = false;
#endif

char const * backend()
{
#if defined( RS2_USE_WMF_BACKEND )
    return "wmf";
#elif defined( RS2_USE_V4L2_BACKEND )
    return "v4l2";
#elif defined( RS2_USE_LIBUVC_BACKEND )
    return "libuvc";
#elif defined( RS2_USE_WINUSB_UVC_BACKEND )
    return "winusb_uvc";
#elif defined( RS2_USE_ANDROID_BACKEND )
    return "android";
#else
    return "unknown";
#endif
}


}  // namespace


rum_collector::rum_collector()
    : _source_id( load_or_create_source_id() )
    , _session_id( generate_source_id() )
{
}


rum_collector & rum_collector::instance()
{
    // Never destroyed on purpose: on_context_closed() flushes through this from ~context, which can
    // run during process exit, when the destruction order of this singleton vs. the context is
    // unspecified (different translation units) - a normally-destroyed static would be a
    // use-after-free. The single allocation is reclaimed by the OS at exit (it stays "still
    // reachable" via this pointer, so it is not reported as a leak by Valgrind or LeakSanitizer).
    static rum_collector * const inst = new rum_collector();
    return *inst;
}


void rum_collector::record_device( std::string const & device_key,
                                   std::string const & connection,
                                   std::string const & fw_version,
                                   std::string const & mipi_driver_version,
                                   std::string const & serial )
{
    std::lock_guard< std::mutex > lk( _mutex );
    auto & d = _devices[device_key];  // create entry if doesn't exist
    d.connection = connection;
    d.fw_version = fw_version;
    d.mipi_driver_version = mipi_driver_version;
    d.seen_this_session = true;
    if( ! serial.empty() )
        d.serials.insert( serial );  // in memory only - distinct units this session; never serialized
}


rum_collector::device_stat * rum_collector::find_device( std::string const & device_key )
{
    auto it = _devices.find( device_key );
    return it == _devices.end() ? nullptr : &it->second;
}


void rum_collector::record_stream( std::string const & device_key, std::string const & stream_label )
{
    std::lock_guard< std::mutex > lk( _mutex );
    if( auto * d = find_device( device_key ) )
        ++d->streams[stream_label].count;
}


void rum_collector::record_stream_duration( std::string const & device_key, std::string const & stream_label, double seconds )
{
    std::lock_guard< std::mutex > lk( _mutex );
    if( auto * d = find_device( device_key ) )
        d->streams[stream_label].duration_seconds += seconds;
}


void rum_collector::record_option_change( std::string const & device_key, std::string const & option, float value )
{
    std::lock_guard< std::mutex > lk( _mutex );
    auto * d = find_device( device_key );
    if( ! d )
        return;
    auto & entry = d->options[option];
    ++entry.first;
    entry.second = value;
}


void rum_collector::add_recommended_filter( std::string const & name )
{
    std::lock_guard< std::mutex > lk( _mutex );
    _recommended_filters.insert( name );
}


void rum_collector::record_filter( std::string const & device_key, std::string const & name )
{
    std::lock_guard< std::mutex > lk( _mutex );
    if( ! _recommended_filters.count( name ) )   // ignore viewer/internal blocks
        return;
    if( auto * d = find_device( device_key ) )
        ++d->filters[name];
}


void rum_collector::record_notification( std::string const & category )
{
    std::lock_guard< std::mutex > lk( _mutex );
    ++_notification_counts[category];
}


std::string rum_collector::get_report()
{
    std::lock_guard< std::mutex > lk( _mutex );
    json report = build_report();
    trim_to_limit( report );
    return report.dump( 2 );   // stored readable; the uploader compacts it before sending
}


json rum_collector::build_report() const
{
    json report = json::object();
    report["schema_version"] = rum_schema_version;
    report["source_id"] = _source_id;
    report["session_id"] = _session_id;
    report["generated_at"] = std::chrono::duration_cast< std::chrono::seconds >(
        std::chrono::system_clock::now().time_since_epoch() ).count();
    report["sdk"] = json::object();
    report["sdk"]["version"] = RS2_API_VERSION_STR;
    report["sdk"]["build_type"] = build_type();
    report["sdk"]["backend"] = backend();
    report["sdk"]["cmake_flags"] = {
        { "ENABLE_STATS", cmake_enable_stats },
        { "BUILD_WITH_DDS", cmake_build_with_dds },
        { "BUILD_WITH_CUDA", cmake_build_with_cuda },
    };
    report["system"] = json::object();
    report["system"]["os"] = rsutils::os::get_os_name();
    report["system"]["arch"] = rsutils::os::cpu_arch();

    report["devices"] = json::object();
    for( auto const & de : _devices )
    {
        auto const & d = de.second;
        json device = json::object();
        device["connection"] = d.connection;
        device["fw_version"] = d.fw_version;
        device["mipi_driver_version"] = d.mipi_driver_version;
        // count = number of identical devices seen at the same time, can't count different devices across sessions as we do not store SNs
        int session_count = d.seen_this_session ? ( d.serials.empty() ? 1 : (int)d.serials.size() ) : 0;
        device["count"] = std::max( session_count, d.count );

        device["streams"] = json::object();
        for( auto const & se : d.streams )
        {
            json stream = json::object();
            stream["count"] = se.second.count;
            stream["duration_seconds"] = std::round( se.second.duration_seconds * 10. ) / 10.;
            device["streams"][se.first] = stream;
        }

        device["options_changed"] = json::object();
        for( auto const & oe : d.options )
        {
            json option = json::object();
            option["set_count"] = oe.second.first;
            option["last_value"] = oe.second.second;
            device["options_changed"][oe.first] = option;
        }

        device["filters"] = json::object();
        for( auto const & fe : d.filters )
            device["filters"][fe.first] = fe.second;

        report["devices"][de.first] = device;
    }

    report["notifications"] = json::array();
    for( auto const & entry : _notification_counts )
    {
        json notification = json::object();
        notification["category"] = entry.first;
        notification["count"] = entry.second;
        report["notifications"].push_back( notification );
    }
    return report;
}


void rum_collector::trim_to_limit( json & report )
{
    // The limit is on what gets uploaded, so measure the compact form - the file is the same report
    // written readable.
    size_t size = report.dump().size();
    if( size <= max_report_bytes )
        return;

    // Streams are the only entry whose key count is unbounded - one per configuration ever opened -
    // so they are what grows a report past the limit. Drop the least-used ones first.
    struct entry { int count; device_stat * device; std::string label; };
    std::vector< entry > streams;
    for( auto & de : _devices )
        for( auto const & se : de.second.streams )
            streams.push_back( { se.second.count, &de.second, se.first } );
    std::sort( streams.begin(), streams.end(),
               []( entry const & l, entry const & r ) { return l.count < r.count; } );

    size_t dropped = 0;
    while( dropped < streams.size() && size > max_report_bytes )
    {
        size_t const per_entry = std::max< size_t >( 1, size / ( streams.size() - dropped ) );
        size_t const batch = std::max< size_t >( 1, ( size - max_report_bytes ) / per_entry );
        for( size_t n = 0; n < batch && dropped < streams.size(); ++n, ++dropped )
            streams[dropped].device->streams.erase( streams[dropped].label );
        report = build_report();
        size = report.dump().size();
    }
    // Reports the resulting size, which stays over the limit if dropping every stream wasn't enough.
    LOG_WARNING( "RUM report over " << max_report_bytes << " bytes: dropped " << dropped
                 << " least-used stream entries, now " << size );
}


void rum_collector::merge_saved_report()
{
    std::lock_guard< std::mutex > lk( _mutex );
    if( _merged_from_disk )
        return;  // fold in once per process; flush() runs per context close, re-merging would double
    _merged_from_disk = true;

    json j;
    try
    {
        j = rsutils::json_config::load_from_file( report_path() );
    }
    catch( ... )
    {
        return;  // no file yet, or unreadable - nothing to fold in
    }
    if( ! j.is_object() )
        return;

    auto devices = j.value( "devices", json::object() );
    for( auto dit = devices.begin(); dit != devices.end(); ++dit )
    {
        auto const & dobj = dit.value();
        auto & d = _devices[ dit.key() ];
        if( d.connection.empty() )          d.connection = dobj.value( "connection", std::string() );
        if( d.fw_version.empty() )          d.fw_version = dobj.value( "fw_version", std::string() );
        if( d.mipi_driver_version.empty() ) d.mipi_driver_version = dobj.value( "mipi_driver_version", std::string() );
        d.count = std::max( d.count, dobj.value( "count", 0 ) );  // peak -> max, not sum

        auto streams = dobj.value( "streams", json::object() );
        for( auto sit = streams.begin(); sit != streams.end(); ++sit )
        {
            auto & s = d.streams[ sit.key() ];
            s.count += sit.value().value( "count", 0 );
            s.duration_seconds += sit.value().value( "duration_seconds", 0.0 );
        }

        auto filters = dobj.value( "filters", json::object() );
        for( auto fit = filters.begin(); fit != filters.end(); ++fit )
            d.filters[ fit.key() ] += fit.value().get< int >();

        auto options = dobj.value( "options_changed", json::object() );
        for( auto oit = options.begin(); oit != options.end(); ++oit )
        {
            auto found = d.options.find( oit.key() );
            if( found == d.options.end() )
                d.options[ oit.key() ] = { oit.value().value( "set_count", 0 ), oit.value().value( "last_value", 0.0f ) };
            else
                found->second.first += oit.value().value( "set_count", 0 );  // keep newer last_value
        }
    }

    for( auto const & e : j.value( "notifications", json::array() ) )
    {
        auto category = e.value( "category", std::string() );
        if( ! category.empty() )
            _notification_counts[ category ] += e.value( "count", 0 );
    }
}


void rum_collector::flush()
{
    merge_saved_report();  // accumulate across sessions; a successful upload is what resets the file
    ensure_report_directory();
    rsutils::os::atomic_write_file( report_path(), get_report() );
}


}  // namespace rum
}  // namespace librealsense
