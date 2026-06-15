// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "rum-collector.h"
#include "rum-config.h"

#include <librealsense2/rs.h>  // RS2_API_VERSION_STR

#include <rsutils/os/atomic-write-file.h>
#include <rsutils/os/special-folder.h>
#include <rsutils/os/os.h>  // get_os_name
#include <rsutils/json.h>
#include <rsutils/json-config.h>

#include <cstdio>
#include <random>


#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

using json = rsutils::json;


namespace librealsense {
namespace rum {


namespace {


// Rolling report lives at <app-data>/rum/rum.json. Forward slashes work for WinAPI and POSIX.
std::string report_path()
{
    return rsutils::os::get_special_folder( rsutils::os::special_folder::app_data ) + "rum/rum.json";
}


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


// Stable, anonymous, opaque id in the canonical UUID 8-4-4-4-12 layout (not a versioned RFC-4122 UUID).
std::string generate_source_id()
{
    std::random_device rd;
    // Evaluate before formatting — snprintf argument order is unspecified.
    unsigned a = rd(), b = rd() & 0xFFFF, c = rd() & 0xFFFF, d = rd() & 0xFFFF, e = rd() & 0xFFFF, f = rd();
    char buf[37];
    std::snprintf( buf, sizeof( buf ), "%08x-%04x-%04x-%04x-%04x%08x", a, b, c, d, e, f );
    return std::string( buf );
}


// The id persists inside the report store (rum.json): reuse it across runs, or mint a new one the
// first time. Kept out of the shared realsense-config.json so the viewer's whole-file config writes
// can't clobber it.
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

constexpr int rum_schema_version = 2;

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

#ifdef ENABLED_STATS
constexpr bool cmake_enabled_stats = true;
#else
constexpr bool cmake_enabled_stats = false;
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

char const * cpu_arch()
{
#if defined( _M_X64 ) || defined( __x86_64__ )
    return "x86_64";
#elif defined( _M_ARM64 ) || defined( __aarch64__ )
    return "arm64";
#elif defined( _M_IX86 ) || defined( __i386__ )
    return "x86";
#elif defined( __arm__ )
    return "arm";
#else
    return "unknown";
#endif
}

}  // namespace


rum_collector::rum_collector()
    : _source_id( load_or_create_source_id() )
{
}


rum_collector & rum_collector::instance()
{
    static rum_collector inst;
    return inst;
}


void rum_collector::record_device( std::string const & type,
                                   std::string const & fw_version,
                                   std::string const & connection,
                                   std::string const & mipi_driver_version )
{
    std::lock_guard< std::mutex > lk( _mutex );
    ++_device_counts[device_key{ type, fw_version, connection, mipi_driver_version }];
}


void rum_collector::record_stream( std::string const & stream_type,
                                   std::string const & format,
                                   std::string const & resolution,
                                   int fps )
{
    std::lock_guard< std::mutex > lk( _mutex );
    ++_stream_counts[stream_key{ stream_type, format, resolution, fps }].count;
}


void rum_collector::record_stream_duration( std::string const & stream_type,
                                            std::string const & format,
                                            std::string const & resolution,
                                            int fps,
                                            double seconds )
{
    std::lock_guard< std::mutex > lk( _mutex );
    _stream_counts[stream_key{ stream_type, format, resolution, fps }].duration_seconds += seconds;
}


void rum_collector::record_option_change( std::string const & option, float value )
{
    std::lock_guard< std::mutex > lk( _mutex );
    auto & entry = _option_changes[option];
    ++entry.first;
    entry.second = value;
}


void rum_collector::record_filter( std::string const & name )
{
    std::lock_guard< std::mutex > lk( _mutex );
    ++_filter_counts[name];
}


void rum_collector::record_notification( std::string const & category )
{
    std::lock_guard< std::mutex > lk( _mutex );
    ++_notification_counts[category];
}


std::string rum_collector::get_report() const
{
    std::lock_guard< std::mutex > lk( _mutex );
    json report = json::object();
    report["schema_version"] = rum_schema_version;
    report["source_id"] = _source_id;
    report["sdk"] = json::object();
    report["sdk"]["version"] = RS2_API_VERSION_STR;
    report["sdk"]["build_type"] = build_type();
    report["sdk"]["backend"] = backend();
    report["sdk"]["cmake_flags"] = {
        { "ENABLED_STATS", cmake_enabled_stats },
        { "BUILD_WITH_DDS", cmake_build_with_dds },
        { "BUILD_WITH_CUDA", cmake_build_with_cuda },
    };
    report["system"] = json::object();
    report["system"]["os"] = rsutils::os::get_os_name();
    report["system"]["arch"] = cpu_arch();

    report["devices"] = json::array();
    for( auto const & entry : _device_counts )
    {
        auto const & key = entry.first;
        json device = json::object();
        device["type"] = key.type;
        device["fw_version"] = key.fw_version;
        device["connection"] = key.connection;
        device["mipi_driver_version"] = key.mipi_driver_version;
        device["count"] = entry.second;
        report["devices"].push_back( device );
    }

    report["streams"] = json::array();
    for( auto const & entry : _stream_counts )
    {
        auto const & key = entry.first;
        json stream = json::object();
        stream["type"] = key.type;
        stream["format"] = key.format;
        stream["resolution"] = key.resolution;
        stream["fps"] = key.fps;
        stream["count"] = entry.second.count;
        stream["duration_seconds"] = entry.second.duration_seconds;
        report["streams"].push_back( stream );
    }

    report["options_changed"] = json::array();
    for( auto const & entry : _option_changes )
    {
        json option = json::object();
        option["option"] = entry.first;
        option["set_count"] = entry.second.first;
        option["last_value"] = entry.second.second;
        report["options_changed"].push_back( option );
    }

    report["filters"] = json::array();
    for( auto const & entry : _filter_counts )
    {
        json filter = json::object();
        filter["name"] = entry.first;
        filter["count"] = entry.second;
        report["filters"].push_back( filter );
    }

    report["notifications"] = json::array();
    for( auto const & entry : _notification_counts )
    {
        json notification = json::object();
        notification["category"] = entry.first;
        notification["count"] = entry.second;
        report["notifications"].push_back( notification );
    }
    return report.dump( 2 );
}


void rum_collector::flush()
{
    ensure_report_directory();
    rsutils::os::atomic_write_file( report_path(), get_report() );
}


}  // namespace rum
}  // namespace librealsense
