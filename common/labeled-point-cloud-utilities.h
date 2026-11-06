// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023-2024 RealSense, Inc. All Rights Reserved.

#pragma once

#include <include/librealsense2/h/rs_types.h>
#include <common/float3.h>

namespace rs2
{
    class labeled_point_cloud_utilities
    {
    public:
        static std::map<rs2_point_cloud_label, float3> get_label_to_color3f();

        // FW LPCL output (D500OgLpclAlgo.cpp): invalid points are written as 404.0F on every axis
        static constexpr float invalid_value = 404.f;
        // Depth saturated at 65535 mm, in meters
        static constexpr float saturated_depth = 65.5f;
    };
    
}
