// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/feature-interface.h>


namespace librealsense {

class d500_depth_sensor;

// Mirrors temporal_filter_feature exactly (see temporal-filter-feature.h) - wires the HKR
// Decimation Filter DPP composite-option embedded filter (decimation_embedded_filter) onto
// a d500_depth_sensor. USB-only; the DDS-connected path has its own, independent, scalar-option
// decimation filter (rs_dds_embedded_decimation_filter) and is untouched by this.
class decimation_filter_feature : public feature_interface
{
public:
    static const feature_id ID;

    explicit decimation_filter_feature( d500_depth_sensor & depth_sensor );

    feature_id get_id() const override;
};

}  // namespace librealsense
