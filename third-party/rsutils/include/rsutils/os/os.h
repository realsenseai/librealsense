// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#pragma once
#include <string>

namespace rsutils
{
    namespace os
    {
        std::string get_os_name();
        std::string get_platform_name();

        // CPU architecture the binary was built for: "x86_64", "arm64", "x86", "arm", or "unknown".
        std::string cpu_arch();

    }
}
