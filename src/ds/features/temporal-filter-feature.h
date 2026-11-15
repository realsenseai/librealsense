// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/feature-interface.h>


namespace librealsense {

class d500_depth_sensor;

// Mirrors close_range_filter_feature exactly (see close-range-filter-feature.h) - wires the
// Temporal Filter DPP composite-option embedded filter (d500_temporal_embedded_filter) onto
// a d500_depth_sensor.
class temporal_filter_feature : public feature_interface
{
public:
    static const feature_id ID;

    explicit temporal_filter_feature( d500_depth_sensor & depth_sensor, bool is_mipi );

    feature_id get_id() const override;
};

}  // namespace librealsense
