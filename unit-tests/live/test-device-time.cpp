// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

//#test:device ANY

#include "live-common.h"

#include <cmath>
#include <memory>
#include <librealsense2/rs.hpp>
#include <librealsense2/rs.h>


TEST_CASE("C++ device hardware time is positive",
    "[live][device-time][cpp]")
{
    auto dev = find_first_device_or_exit();

    double const device_time_ms = dev.get_device_time_ms();

    REQUIRE(std::isfinite(device_time_ms));
    REQUIRE(device_time_ms > 0.0);
}


TEST_CASE("C device hardware time is positive",
    "[live][device-time][c]")
{
    rs2_error* error = nullptr;

    using context_ptr =
        std::unique_ptr<rs2_context, decltype(&rs2_delete_context)>;

    using device_list_ptr =
        std::unique_ptr<rs2_device_list,
        decltype(&rs2_delete_device_list)>;

    using device_ptr =
        std::unique_ptr<rs2_device, decltype(&rs2_delete_device)>;

    context_ptr context(
        rs2_create_context(RS2_API_VERSION, &error),
        rs2_delete_context);

    REQUIRE(error == nullptr);
    REQUIRE(context != nullptr);

    device_list_ptr devices(
        rs2_query_devices(context.get(), &error),
        rs2_delete_device_list);

    REQUIRE(error == nullptr);
    REQUIRE(devices != nullptr);

    int const device_count =
        rs2_get_device_count(devices.get(), &error);

    REQUIRE(error == nullptr);
    REQUIRE(device_count > 0);

    device_ptr device(
        rs2_create_device(devices.get(), 0, &error),
        rs2_delete_device);

    REQUIRE(error == nullptr);
    REQUIRE(device != nullptr);

    double const device_time_ms =
        rs2_get_device_time_ms(device.get(), &error);

    REQUIRE(error == nullptr);
    REQUIRE(std::isfinite(device_time_ms));
    REQUIRE(device_time_ms > 0.0);
}