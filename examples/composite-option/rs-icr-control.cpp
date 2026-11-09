// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

// Improved Close Range (ICR) control: how to read and write the composite option with rs2_icr_filter_dpp_config.
// Finds the first connected depth sensor exposing an ICR embedded filter, then:
//   1. reads the current settings and the supported ranges,
//   2. writes new settings (read-modify-write, so fields left alone keep their value),
//   3. reads them back and restores the original settings.
// rs2_icr_filter_dpp_config only holds user-facing settings; the firmware payload layout is private to the SDK.

#include <librealsense2/rs.hpp>
#include <librealsense2/h/rs_icr_filter_dpp.h>

#include <iostream>
#include <memory>

namespace
{
    const rs2_composite_option_id ICR = RS2_COMPOSITE_OPTION_ICR_CONTROL;

    void print( const char * title, const rs2_icr_filter_dpp_config & v )
    {
        std::cout << title << '\n'
                  << "  enable          = " << v.enable << '\n'
                  << "  filter_type     = " << v.filter_type << "  (0 = Downscale, 1 = Lookup shift)\n"
                  << "  downscale_ratio = " << v.downscale_ratio << "  (1 = x2, 2 = x4)\n"
                  << "  shift_mode      = " << v.shift_mode << "  (0 = 126px, 1 = 64px, 2 = manual)\n"
                  << "  shift_pixels    = " << v.shift_pixels << '\n'
                  << "  threshold_mode  = " << v.threshold_mode << "  (0 = zero range, 1 = MinZ, 2 = manual)\n"
                  << "  threshold_mm    = " << v.threshold_mm << '\n';
    }

    // The first embedded filter of any connected depth sensor that supports the ICR control
    std::shared_ptr< rs2::embedded_filter > find_icr_filter()
    {
        rs2::context ctx;
        for( auto && dev : ctx.query_devices() )
            for( auto && sensor : dev.query_sensors() )
                if( sensor.is< rs2::depth_sensor >() )
                    for( auto && filter : sensor.query_embedded_filters() )
                        if( filter.supports_composite_option( ICR ) )
                            return std::make_shared< rs2::embedded_filter >( filter );
        return nullptr;
    }
}

int main()
try
{
    auto found = find_icr_filter();
    if( ! found )
    {
        std::cout << "No connected device exposes the Improved Close Range control\n";
        return 0;
    }

    rs2::embedded_filter & icr = *found;

    // 1. Get - the current settings, and the {min, max, step, default} bounds
    const auto original = icr.get_composite_option_as< rs2_icr_filter_dpp_config >( ICR );
    print( "Current settings:", original );

    const auto range = icr.get_composite_option_range_as< rs2_icr_filter_dpp_range >( ICR );
    std::cout << "Supported shift_pixels: [" << range.min.shift_pixels << ", " << range.max.shift_pixels
              << "], default " << range.def.shift_pixels << '\n';

    // 2. Set - start from what the device reports, change only what is needed, send it all at once.
    //    The device rejects enabling ICR on Depth profiles it cannot process, and changing
    //    filter_type / downscale_ratio while Depth or Infrared streams - an rs2::error is thrown.
    auto cfg = original;
    cfg.enable = 1;
    cfg.filter_type = 0;      // Downscale
    cfg.downscale_ratio = 1;  // x2
    icr.set_composite_option_from( ICR, cfg );

    // 3. Get again to see what the device applied (it may adjust values), then restore
    print( "Settings after set:", icr.get_composite_option_as< rs2_icr_filter_dpp_config >( ICR ) );

    icr.set_composite_option_from( ICR, original );
    std::cout << "Original settings restored\n";
    return 0;
}
catch( const rs2::error & e )
{
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args()
              << "):\n    " << e.what() << std::endl;
    return 1;
}
catch( const std::exception & e )
{
    std::cerr << e.what() << std::endl;
    return 1;
}
