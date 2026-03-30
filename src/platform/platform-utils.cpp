// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#include "platform-utils.h"

#include "uvc-device-info.h"
#include "hid-device-info.h"
#include <src/librealsense-exception.h>
#include <rsutils/version.h>
#include <fstream>

namespace librealsense {
namespace platform {


std::vector< uvc_device_info > filter_by_product( const std::vector< uvc_device_info > & devices,
                                                  const std::set< uint16_t > & pid_list )
{
    std::vector< uvc_device_info > result;
    for( auto && info : devices )
    {
        if( pid_list.count( info.pid ) )
            result.push_back( info );
    }
    return result;
}

// TODO: Make template
std::vector< usb_device_info > filter_by_product( const std::vector< usb_device_info > & devices,
                                                  const std::set< uint16_t > & pid_list )
{
    std::vector< usb_device_info > result;
    for( auto && info : devices )
    {
        if( pid_list.count( info.pid ) )
            result.push_back( info );
    }
    return result;
}

std::vector< std::pair< std::vector< uvc_device_info >, std::vector< hid_device_info > > >
group_devices_and_hids_by_unique_id( const std::vector< std::vector< uvc_device_info > > & devices,
                                     const std::vector< hid_device_info > & hids )
{
    std::vector< std::pair< std::vector< uvc_device_info >, std::vector< hid_device_info > > > results;
    uint16_t vid;
    uint16_t pid;

    for( auto && dev : devices )
    {
        std::vector< hid_device_info > hid_group;
        auto unique_id = dev.front().unique_id;
        auto device_serial = dev.front().serial;

        for( auto && hid : hids )
        {
            if( ! hid.unique_id.empty() )
            {
                std::stringstream( hid.vid ) >> std::hex >> vid;
                std::stringstream( hid.pid ) >> std::hex >> pid;

                if( hid.unique_id == unique_id )
                {
                    hid_group.push_back( hid );
                }
            }
        }
        results.push_back( std::make_pair( dev, hid_group ) );
    }
    return results;
}

std::vector< std::vector< uvc_device_info > >
group_devices_by_unique_id( const std::vector< uvc_device_info > & devices )
{
    std::map< std::string, std::vector< uvc_device_info > > map;
    for( auto && info : devices )
    {
        map[info.unique_id].push_back( info );
    }
    std::vector< std::vector< uvc_device_info > > result;
    for( auto && kvp : map )
    {
        result.push_back( kvp.second );
    }
    return result;
}

// TODO: Sergey
// Make template
void trim_device_list( std::vector< usb_device_info > & devices, const std::vector< usb_device_info > & chosen )
{
    if( chosen.empty() )
        return;

    auto was_chosen = [&chosen]( const usb_device_info & info )
    {
        return find( chosen.begin(), chosen.end(), info ) != chosen.end();
    };
    devices.erase( std::remove_if( devices.begin(), devices.end(), was_chosen ), devices.end() );
}

void trim_device_list( std::vector< uvc_device_info > & devices, const std::vector< uvc_device_info > & chosen )
{
    if( chosen.empty() )
        return;

    auto was_chosen = [&chosen]( const uvc_device_info & info )
    {
        return find( chosen.begin(), chosen.end(), info ) != chosen.end();
    };
    devices.erase( std::remove_if( devices.begin(), devices.end(), was_chosen ), devices.end() );
}

bool mi_present( const std::vector< uvc_device_info > & devices, uint32_t mi )
{
    for( auto && info : devices )
    {
        if( info.mi == mi )
            return true;
    }
    return false;
}

uvc_device_info get_mi( const std::vector< uvc_device_info > & devices, uint32_t mi )
{
    for( auto && info : devices )
    {
        if( info.mi == mi )
            return info;
    }
    throw invalid_value_exception( "Interface not found!" );
}

std::vector< uvc_device_info > filter_by_mi( const std::vector< uvc_device_info > & devices, uint32_t mi )
{
    std::vector< uvc_device_info > results;
    for( auto && info : devices )
    {
        if( info.mi == mi )
            results.push_back( info );
    }
    return results;
}

#ifdef __linux__

rsutils::version get_jetson_driver_version()
{
    // Cache the result to avoid reading the file multiple times
    static bool queried = false;
    static rsutils::version cached_version;
    
    if (queried)
        return cached_version.is_valid() ? cached_version : rsutils::version();
    
    // Read driver version from sysfs
    std::ifstream version_file("/sys/module/d4xx/version");
    if (version_file.is_open())
    {
        std::string version_str;
        std::getline(version_file, version_str);
        version_file.close();
        
        if (!version_str.empty())
            cached_version = rsutils::version(version_str);
    }
    
    queried = true;
    return cached_version.is_valid() ? cached_version : rsutils::version();
}
#else
rsutils::version get_jetson_driver_version()
{
    return rsutils::version();
}
#endif


}  // namespace platform
}  // namespace librealsense
