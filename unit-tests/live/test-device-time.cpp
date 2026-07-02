// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

//#test:device D400*

#include "live-common.h"

#include <cmath>
#include <librealsense2/h/rs_device.h>
#include <librealsense2/rs_device.hpp>


TEST_CASE("device hardware time is positive", "[live][device-time]")
{
    auto dev = find_first_device_or_exit();

    SECTION("C++ API")
    {
        double const device_time_ms = dev.get_device_time_ms();

        REQUIRE(std::isfinite(device_time_ms));
        REQUIRE(device_time_ms > 0.0);
    }

    SECTION("C API")
    {
        rs2_error* error = nullptr;

        double const device_time_ms =
            rs2_get_device_time_ms(dev.get(), &error);

        REQUIRE(error == nullptr);
        REQUIRE(std::isfinite(device_time_ms));
        REQUIRE(device_time_ms > 0.0);
    }
}
