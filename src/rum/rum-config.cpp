// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "rum-config.h"

#include <librealsense2/rs.h>  // RS2_CONFIG_FILENAME

#include <rsutils/os/special-folder.h>
#include <rsutils/os/atomic-write-file.h>
#include <rsutils/json.h>
#include <rsutils/json-config.h>
#include <rsutils/easylogging/easyloggingpp.h>

#include <cstdlib>

using json = rsutils::json;


namespace librealsense {
namespace rum {


namespace {


char const * const cloud_enabled_env = "RS2_RUM_CLOUD_ENABLED";


// Environment variable value, or empty if unset/blank.
std::string env_value( char const * name )
{
    char const * v = std::getenv( name );
    return ( v && *v ) ? std::string( v ) : std::string();
}


std::string default_config_path()
{
    return rsutils::os::get_special_folder( rsutils::os::special_folder::app_data ) + RS2_CONFIG_FILENAME;
}


bool parse_bool( std::string const & s )
{
    return s == "1";   // anything else (incl. "0", empty, malformed) -> opt-out
}


}  // namespace


rum_config & rum_config::instance()
{
    static rum_config inst( default_config_path() );
    return inst;
}


rum_config::rum_config( std::string filename )
    : _filename( std::move( filename ) )
{
}


json rum_config::load_config() const
{
    try
    {
        auto j = rsutils::json_config::load_from_file( _filename );
        if( j.is_object() )
            return j;
    }
    catch( ... )
    {
    }
    return json::object();
}


bool rum_config::save_config( json const & j )
{
    return rsutils::os::atomic_write_file( _filename, j.dump( 2 ) );
}


bool rum_config::is_cloud_enabled() const
{
    auto env = env_value( cloud_enabled_env );
    if( ! env.empty() )
        return parse_bool( env );
    std::lock_guard< std::mutex > lk( _mutex );
    auto s = load_config().nested( "rum_cloud_enabled", &json::is_string ).string_ref_or_empty();
    return ! s.empty() && parse_bool( s );
}


void rum_config::set_cloud_enabled( bool enabled )
{
    std::lock_guard< std::mutex > lk( _mutex );
    auto j = load_config();
    // Store as a string ("1"/"0") to match the viewer's config_file, whose contains()
    // only recognizes string values; a native bool would make the first-run popup re-prompt.
    j["rum_cloud_enabled"] = enabled ? "1" : "0";
    if( ! save_config( j ) )
        LOG_WARNING( "RUM: failed to persist rum_cloud_enabled to " << _filename );
}


}  // namespace rum
}  // namespace librealsense
