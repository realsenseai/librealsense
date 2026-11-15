// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/feature-interface.h>


namespace librealsense {

class d500_device;
class d500_depth_sensor;

// Wires the HKR Improved Close Range (MinZ) Control composite-option embedded filter (hdrd_embedded_filter) onto a
// d500_depth_sensor, and rejects enabling it on Depth profiles the FW cannot process.
class hdrd_filter_feature : public feature_interface
{
public:
    static const feature_id ID;

    hdrd_filter_feature( d500_device & owner, d500_depth_sensor & depth_sensor, bool is_mipi );

    feature_id get_id() const override;
};

}  // namespace librealsense
