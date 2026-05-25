// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#include "rs-config.h"

#include <librealsense2/rs.h>
#include <rsutils/os/special-folder.h>
#include <rsutils/json.h>
#include <rsutils/json-config.h>
#include <fstream>

using json = rsutils::json;

using namespace rs2;

void config_file::set(const char* key, const char* value)
{
    _j[key] = value;
    save();
}

void config_file::set_default(const char* key, const char* calculate)
{
    _defaults[key] = calculate;
}

void config_file::remove(const char* key)
{
    _j.erase(key);
    save();
}

void config_file::reset()
{
    _j = json::object();
    save();
}

std::string config_file::get(const char* key, const char* def) const
{
    auto it = _j.find(key);
    if (it != _j.end() && it->is_string())
    {
        return it->string_ref();
    }
    return get_default(key, def);
}

bool config_file::contains(const char* key) const
{
    auto it = _j.find(key);
    return it != _j.end() && it->is_string();
}

std::string config_file::get_default(const char* key, const char* def) const
{
    auto it = _defaults.find(key);
    if (it == _defaults.end()) return def;
    return it->second;
}

config_value config_file::get(const char* key) const
{
    if (!contains(key)) return config_value(get_default(key, ""));
    return config_value(get(key, ""));
}

void config_file::save(const char* filename)
{
    try
    {
        std::ofstream out(filename);
        out << std::setw( 2 ) << _j;
        out.close();
    }
    catch (...)
    {
    }
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
    save(_filename.c_str());
}

config_file::config_file()
    : _j( rsutils::json::object() )
{
}

config_file& config_file::operator=(const config_file& other)
{
    if (this != &other)
    {
        _j = other._j;
        _defaults = other._defaults;
        save();
    }
    return *this;
}

bool config_file::operator==(const config_file& other) const
{
    return _j == other._j;
}
