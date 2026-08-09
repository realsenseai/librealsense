// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "assistant-ui-utils.h"

namespace rs2
{
    namespace assistant_detail
    {
        void draw_soft_shadow(ImVec2 pos, ImVec2 size, float rounding)
        {
            auto* dl = ImGui::GetBackgroundDrawList();
            const int layers = 4;
            const float y_offset = 3.f; // shadow falls slightly downward, like a resting card
            for (int i = layers; i >= 1; --i)
            {
                float spread = i * 3.f;
                ImU32 col = ImColor(0.f, 0.f, 0.f, 0.05f * (layers - i + 1));
                dl->AddRectFilled({ pos.x - spread, pos.y - spread + y_offset },
                    { pos.x + size.x + spread, pos.y + size.y + spread + y_offset }, col, rounding + spread);
            }
        }
    }
}
