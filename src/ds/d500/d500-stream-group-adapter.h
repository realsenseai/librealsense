// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "d500-stream-group-transaction.h"

#include <src/platform/stream-profile.h>

#include <vector>


namespace librealsense
{
    // Converts the resolved raw profiles of a D500 dual-color sensor into the
    // canonical stream-group branches understood by firmware. This class is a
    // pure adapter: it does not send HWMC commands or alter sensor lifecycle.
    class d500_dual_color_stream_group_adapter
    {
    public:
        static std::vector< d500_stream_group_profile > build_manifest(
            std::vector< platform::stream_profile > const & selected_profiles,
            std::vector< platform::stream_profile > const & advertised_profiles );
    };
}
