// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#pragma once

#include "stream-profile-interface.h"


namespace librealsense
{
    class inference_stream_profile_interface : public virtual stream_profile_interface
    {
        // Empty — marker interface for inference stream profiles
    };

    MAP_EXTENSION( RS2_EXTENSION_INFERENCE_PROFILE, librealsense::inference_stream_profile_interface );

}  // namespace librealsense
