// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.
#pragma once


#include <src/core/options-watcher.h>


namespace librealsense {

class raw_sensor_base;

// Used by syntethic sensor and uses the raw_sensor bulk operations.
class synthetic_options_watcher : public options_watcher
{
public:
    synthetic_options_watcher( const std::shared_ptr< raw_sensor_base > & raw_sensor );

protected:
    void update( options_and_values & updated_options, composite_options_and_values & updated_composites ) override;

    std::weak_ptr< raw_sensor_base > _raw_sensor;
};


}  // namespace librealsense
