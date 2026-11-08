// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.
#pragma once

#include <cstdint>


namespace librealsense {


#pragma pack( push, 1 )
struct hid_data
{
    int32_t x;
    int32_t y;
    int32_t z;
};


struct hid_mipi_data
{
    uint8_t typeID;
    uint8_t skip1;
    uint64_t hwTs;
    int16_t x;
    int16_t y;
    int16_t z;
    uint64_t hwTs2;
    uint64_t skip2;
};


struct hid_mipi_data_32
{
    uint8_t typeID;
    uint8_t skip1;
    uint64_t hwTs;
    int32_t x;
    int32_t y;
    int32_t z;
    uint64_t hwTs2;
    uint64_t skip2;
};
#pragma pack( pop )


// hid_mipi_data(_32)::typeID values
enum : uint8_t
{
    hid_mipi_accel = 1,
    hid_mipi_gyro = 2,
};


}  // namespace librealsense
