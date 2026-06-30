// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#pragma once

#include "embedded-filter-base.h"
#include "core/extension.h"


namespace librealsense {

class temporal_embedded_filter : public embedded_filter_base
{
};

MAP_EXTENSION( RS2_EXTENSION_TEMPORAL_EMBEDDED_FILTER, librealsense::temporal_embedded_filter );

}  // namespace librealsense
