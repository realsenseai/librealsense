// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/ds/d500/composite-embedded-filter.h>
#include <src/proc/close-range-embedded-filter.h>


namespace librealsense {

// HKR/D5X5 Improved Close Range control, registered via the generic composite-option mechanism -
// see composite-embedded-filter.h for what this alias actually registers, and
// temporal-filter-feature.cpp for the other instantiation.
//
// Addresses the SAME physical XU control (unit 3, selector 0x14) that used to be exposed as
// "Improved Close Range Depth", a scalar enable-only option (since removed) - consolidated onto
// this composite option's all 7 fields. RS2_OPTION_EMBEDDED_FILTER_ENABLED is no longer
// registered for this filter, so a caller assuming every filter has it will throw - accepted.
using icr_embedded_filter
    = composite_embedded_filter< close_range_embedded_filter, RS2_EMBEDDED_FILTER_TYPE_CLOSE_RANGE >;

}  // namespace librealsense
