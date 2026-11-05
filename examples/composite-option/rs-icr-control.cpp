// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

// Composite-option device sweep against whatever RealSense devices are actually connected.
// Walks device -> depth sensor -> embedded filter -> each supported id, printing its current
// value, through the public C++ wrapper (rs2::options), a thin pass-through to the C API.

#include <librealsense2/rs.hpp>
#include <librealsense2/h/rs_decimation_filter_dpp.h>
#include <librealsense2/h/rs_temporal_filter_dpp.h>
#include <librealsense2/h/rs_icr_control.h>

#include <iostream>
#include <string>

namespace
{
    const char * composite_option_name( rs2_composite_option_id id )
    {
        switch( id )
        {
        case RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP:  return "DECIMATION_FILTER_DPP";
        case RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP:    return "TEMPORAL_FILTER_DPP";
        case RS2_COMPOSITE_OPTION_ICR_CONTROL:           return "ICR_CONTROL";
        default:                                           return "UNKNOWN";
        }
    }

    void print_decimation_filter_dpp( const rs2_decimation_filter_dpp_config & v )
    {
        std::cout << "        version=" << (int)v.header.version
                   << " flags=" << (int)v.header.flags
                   << " ctl_id=0x" << std::hex << v.header.ctl_id << std::dec
                   << " param_count=" << (int)v.header.param_count
                   << " param_type=" << (int)v.header.param_type
                   << " enabled=" << v.enabled
                   << " magnitude=" << v.magnitude << '\n';
    }

    void print_temporal_filter_dpp( const rs2_temporal_filter_dpp_config & v )
    {
        std::cout << "        version=" << (int)v.header.version
                   << " flags=" << (int)v.header.flags
                   << " ctl_id=0x" << std::hex << v.header.ctl_id << std::dec
                   << " param_count=" << (int)v.header.param_count
                   << " param_type=" << (int)v.header.param_type
                   << " enabled=" << v.enabled
                   << " smooth_alpha=" << v.smooth_alpha
                   << " smooth_delta=" << v.smooth_delta
                   << " persistency_index=" << v.persistency_index << '\n';
    }

    void print_icr_control( const rs2_icr_control & v )
    {
        std::cout << "        version=" << (int)v.header.version
                   << " flags=" << (int)v.header.flags
                   << " ctl_id=0x" << std::hex << v.header.ctl_id << std::dec
                   << " param_count=" << (int)v.header.param_count
                   << " param_type=" << (int)v.header.param_type
                   << " mode=" << v.mode
                   << " downscale_ratio=" << v.downscale_ratio
                   << " high_res_mode=" << v.high_res_mode
                   << " high_res_offset=" << v.high_res_offset
                   << " smoothing_offset_mm=" << v.smoothing_offset_mm
                   << " completion_offset_mm=" << v.completion_offset_mm
                   << " min_z_mm=" << v.min_z_mm
                   << " improved_min_z_mm=" << v.improved_min_z_mm << '\n';
    }

    // Prints every field of `id`'s current value, dispatching to the right typed cast - no
    // generic "print any composite option" mechanism by design, so a new id needs a case here too.
    void print_composite_option_value( const rs2::options & opts, rs2_composite_option_id id )
    {
        try
        {
            switch( id )
            {
            case RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP:
                print_decimation_filter_dpp( opts.get_composite_option_as< rs2_decimation_filter_dpp_config >( id ) );
                break;
            case RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP:
                print_temporal_filter_dpp( opts.get_composite_option_as< rs2_temporal_filter_dpp_config >( id ) );
                break;
            case RS2_COMPOSITE_OPTION_ICR_CONTROL:
                print_icr_control( opts.get_composite_option_as< rs2_icr_control >( id ) );
                break;
            default:
                std::cout << "        (no typed printer registered for this composite option id)\n";
                break;
            }
        }
        catch( const std::exception & e )
        {
            std::cout << "        FAILED to read from device: " << e.what() << '\n';
        }
    }
}

int main()
try
{
    rs2::context ctx;
    auto devices = ctx.query_devices();
    std::cout << "Found " << devices.size() << " device(s)\n";

    for( auto && dev : devices )
    {
        std::string dev_name = dev.supports( RS2_CAMERA_INFO_NAME )
            ? dev.get_info( RS2_CAMERA_INFO_NAME ) : "Unknown device";
        std::string dev_sn = dev.supports( RS2_CAMERA_INFO_SERIAL_NUMBER )
            ? dev.get_info( RS2_CAMERA_INFO_SERIAL_NUMBER ) : "N/A";
        std::cout << "\nDevice: " << dev_name << " (S/N " << dev_sn << ")\n";

        bool found_depth_sensor = false;
        for( auto && sensor : dev.query_sensors() )
        {
            if( ! sensor.is< rs2::depth_sensor >() )
                continue;
            found_depth_sensor = true;

            std::string sensor_name = sensor.supports( RS2_CAMERA_INFO_NAME )
                ? sensor.get_info( RS2_CAMERA_INFO_NAME ) : "Depth Sensor";
            std::cout << "  Depth sensor: " << sensor_name << '\n';

            auto embedded_filters = sensor.query_embedded_filters();
            if( embedded_filters.empty() )
            {
                std::cout << "    No embedded filters on this sensor.\n";
                continue;
            }

            for( auto && ef : embedded_filters )
            {
                std::cout << "    Embedded filter: " << rs2_embedded_filter_type_to_string( ef.get_type() ) << '\n';

                auto composite_ids = ef.get_supported_composite_options();
                if( composite_ids.empty() )
                {
                    std::cout << "      Supports composite options: no (scalar rs2_option only)\n";
                    continue;
                }

                std::cout << "      Supports composite options: yes (" << composite_ids.size() << ")\n";
                for( auto id : composite_ids )
                {
                    std::cout << "      - " << composite_option_name( id ) << ":\n";
                    print_composite_option_value( ef, id );
                }
            }
        }

        if( ! found_depth_sensor )
            std::cout << "  No depth sensor on this device.\n";
    }

    return 0;
}
catch( const rs2::error & e )
{
    std::cerr << "FAIL: librealsense error: " << e.what() << std::endl;
    return 1;
}
catch( const std::exception & e )
{
    std::cerr << "FAIL: " << e.what() << std::endl;
    return 1;
}
