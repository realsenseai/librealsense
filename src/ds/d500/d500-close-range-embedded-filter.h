// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/proc/close-range-embedded-filter.h>
#include <src/option.h>
#include <src/uvc-sensor.h>

#include <functional>
#include <memory>


namespace librealsense {

// Single-option (Enable) wrapper around the depth-XU close-range control.
class close_range_xu_option : public option
{
public:
    close_range_xu_option( std::weak_ptr< uvc_sensor > ep );

    void set( float value ) override;
    float query() const override;
    option_range get_range() const override { return { 0.f, 1.f, 1.f, 1.f }; }
    bool is_enabled() const override { return true; }
    const char * get_description() const override
    {
        return "Improved Close Range Depth - enable/disable the depth merge (pre-stream only)";
    }
    void enable_recording( std::function< void( const option & ) > rec ) override { _record = rec; }

private:
    std::weak_ptr< uvc_sensor > _ep;
    std::function< void( const option & ) > _record = []( const option & ) {};
};


// Improved Close Range Depth embedded filter for D5xx devices.
class d500_close_range_embedded_filter : public close_range_embedded_filter
{
public:
    explicit d500_close_range_embedded_filter( std::weak_ptr< uvc_sensor > raw_depth_ep );

    rs2_embedded_filter_type get_type() const override { return RS2_EMBEDDED_FILTER_TYPE_CLOSE_RANGE; }
};

}  // namespace librealsense
