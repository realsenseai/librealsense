// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <imgui.h>

namespace rs2
{
    namespace assistant_detail
    {
        // Fakes a soft drop-shadow (ImGui has no native one) by drawing a few progressively larger,
        // more transparent rounded rects into the background draw list, behind the real window.
        void draw_soft_shadow(ImVec2 pos, ImVec2 size, float rounding);
    }
}
