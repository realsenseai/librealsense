// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.


#include <src/synthetic-options-watcher.h>
#include <src/sensor.h>


namespace librealsense {


synthetic_options_watcher::synthetic_options_watcher( const std::shared_ptr< raw_sensor_base > & raw_sensor )
    : _raw_sensor( raw_sensor )
{
}

void synthetic_options_watcher::update( options_and_values & updated_options,
                                        composite_options_and_values & updated_composites )
{
    std::shared_ptr< raw_sensor_base > strong = _raw_sensor.lock();
    if( ! strong )
        return;
    try
    {
        strong->prepare_for_bulk_operation();
        options_watcher::update( updated_options, updated_composites );
        strong->finished_bulk_operation();
    }
    catch( const std::exception & ex )
    {
        LOG_ERROR( "Error when updating options: " << ex.what() );
    }
    catch( ... )
    {
        LOG_ERROR( "Unknown error when updating options!" );
    }
}

}  // namespace librealsense
