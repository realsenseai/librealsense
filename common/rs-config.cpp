// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#include "rs-config.h"

#include <librealsense2/rs.h>
#include <rsutils/os/special-folder.h>
#include <rsutils/os/atomic-write-file.h>
#include <rsutils/json.h>
#include <rsutils/json-config.h>
#include <sstream>
#include <rsutils/easylogging/easyloggingpp.h>

using json = rsutils::json;

using namespace rs2;

void config_file::set(const char* key, const char* value)
{
    std::lock_guard< std::recursive_mutex > lk( _mutex );
    _j[key] = value;
    save();
}

void config_file::set_default(const char* key, const char* calculate)
{
    std::lock_guard< std::recursive_mutex > lk( _mutex );
    _defaults[key] = calculate;
}

void config_file::remove(const char* key)
{
    std::lock_guard< std::recursive_mutex > lk( _mutex );
    _j.erase(key);
    save();
}

void config_file::reset()
{
    std::lock_guard< std::recursive_mutex > lk( _mutex );
    _j = json::object();
    save();
}

std::string config_file::get(const char* key, const char* def) const
{
    std::lock_guard< std::recursive_mutex > lk( _mutex );
    auto it = _j.find(key);
    if (it != _j.end() && it->is_string())
    {
        return it->string_ref();
    }
    return get_default(key, def);
}

bool config_file::contains(const char* key) const
{
    std::lock_guard< std::recursive_mutex > lk( _mutex );
    auto it = _j.find(key);
    return it != _j.end() && it->is_string();
}

std::string config_file::get_default(const char* key, const char* def) const
{
    std::lock_guard< std::recursive_mutex > lk( _mutex );
    auto it = _defaults.find(key);
    if (it == _defaults.end()) return def;
    return it->second;
}

config_value config_file::get(const char* key) const
{
    std::lock_guard< std::recursive_mutex > lk( _mutex );
    if (!contains(key)) return config_value(get_default(key, ""));
    return config_value(get(key, ""));
}

void config_file::save(const char* filename)
{
    std::string serialized;
    {
        std::lock_guard< std::recursive_mutex > lk( _mutex );
        if( ! filename )
        {
            LOG_ERROR( "Config file name is null, cannot save config." );
            return;
        }
        std::ostringstream oss;
        oss << std::setw( 2 ) << _j;
        serialized = oss.str();
    }
    if( ! rsutils::os::atomic_write_file( filename, serialized ) )
        LOG_ERROR( "Failed to save config file '" + std::string( filename ) + "'" );
}

config_file& config_file::instance()
{
    static config_file inst( rsutils::os::get_special_folder( rsutils::os::special_folder::app_data )
                             + RS2_CONFIG_FILENAME );
    return inst;
}

config_file::config_file( std::string const & filename )
    : _filename( filename )
{
    try
    {
        auto j = rsutils::json_config::load_from_file( filename );
        if( j.exists() )
            _j = std::move( j );
    }
    catch(...)
    {

    }
}

void config_file::save()
{
    save( _filename.c_str() );
}

config_file::config_file()
    : _j( rsutils::json::object() )
{
}

config_file& config_file::operator=(const config_file& other)
{
    if (this != &other)
    {
        // Snapshot under other's lock only, so the disk write below doesn't block readers of `other`.
        rsutils::json j_copy;
        std::map< std::string, std::string > defaults_copy;
        {
            std::lock_guard< std::recursive_mutex > lk_other( other._mutex );
            j_copy = other._j;
            defaults_copy = other._defaults;
        }
        std::lock_guard< std::recursive_mutex > lk_this( _mutex );
        _j = std::move( j_copy );
        _defaults = std::move( defaults_copy );
        save();
    }
    return *this;
}

bool config_file::operator==(const config_file& other) const
{
    if (this == &other) return true;
    std::lock( _mutex, other._mutex );
    std::lock_guard< std::recursive_mutex > lk_this( _mutex, std::adopt_lock );
    std::lock_guard< std::recursive_mutex > lk_other( other._mutex, std::adopt_lock );
    return _j == other._j;
}
