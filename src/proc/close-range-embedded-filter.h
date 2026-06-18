// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "embedded-filter-base.h"
#include "core/extension.h"


namespace librealsense {

class close_range_embedded_filter : public embedded_filter_base
{
};

MAP_EXTENSION( RS2_EXTENSION_CLOSE_RANGE_EMBEDDED_FILTER, librealsense::close_range_embedded_filter );

}  // namespace librealsense
