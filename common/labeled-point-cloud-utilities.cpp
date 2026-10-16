// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#include "float3.h"
#include <map>
#include <vector>
#include <algorithm>
#include <include/librealsense2/h/rs_types.h>
#include <include/librealsense2/hpp/rs_frame.hpp>

#include "labeled-point-cloud-utilities.h"

namespace rs2 
{
    std::map<rs2_point_cloud_label, float3> labeled_point_cloud_utilities::get_label_to_color3f()
    {
        static const float3 ORANGE_COL = { 1.0f, 0.65f, 0.2f };
        static const float3 DARK_PURPLE_COL = { 0.2f, 0.1f, 0.2f };
        static const float3 WHITE_COL = { 1.0f, 1.0f, 1.0f };
        static const float3 GREEN_COL = { 0.0f, 1.0f, 0.0f };
        static const float3 TURQUOISE_COL = { 0.0f, 1.0f, 1.0f };
        static const float3 BLUE_COL = { 0.0f, 0.0f, 1.0f };
        static const float3 PINK_COL = { 1.0f, 0.75f, 0.8f };
        static const float3 GREY_COL = { 0.5f, 0.5f, 0.5f };
        std::map<rs2_point_cloud_label, float3> label_to_color3f;

        label_to_color3f[RS2_POINT_CLOUD_LABEL_UNKNOWN] = ORANGE_COL;
        label_to_color3f[RS2_POINT_CLOUD_LABEL_UNDEFINED] = ORANGE_COL;
        label_to_color3f[RS2_POINT_CLOUD_LABEL_INVALID] = ORANGE_COL;
        label_to_color3f[RS2_POINT_CLOUD_LABEL_GAP] = ORANGE_COL;

        label_to_color3f[RS2_POINT_CLOUD_LABEL_GROUND] = WHITE_COL;
        label_to_color3f[RS2_POINT_CLOUD_LABEL_NEAR_GROUND] = GREEN_COL;
        label_to_color3f[RS2_POINT_CLOUD_LABEL_OVERHEAD] = TURQUOISE_COL;
        label_to_color3f[RS2_POINT_CLOUD_LABEL_ABOVE_CEILING_HEIGHT] = BLUE_COL;
        label_to_color3f[RS2_POINT_CLOUD_LABEL_MASKED] = GREY_COL;
        label_to_color3f[RS2_POINT_CLOUD_LABEL_CLIFF] = DARK_PURPLE_COL;
        label_to_color3f[RS2_POINT_CLOUD_LABEL_OBSTACLE] = PINK_COL;
        label_to_color3f[RS2_POINT_CLOUD_LABEL_OBSTACLE_DANGER] = PINK_COL;   // drawn as obstacles
        label_to_color3f[RS2_POINT_CLOUD_LABEL_OBSTACLE_WARNING] = PINK_COL;  // drawn as obstacles



        return label_to_color3f;
    }
} // rs2 namespace