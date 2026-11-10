// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.


#include <src/synthetic-options-watcher.h>
#include <src/sensor.h>


namespace librealsense {


synthetic_options_watcher::synthetic_options_watcher( const std::shared_ptr< raw_sensor_base > & raw_sensor )
    : _raw_sensor( raw_sensor )
{
}

synthetic_options_watcher::options_and_values synthetic_options_watcher::update_options()
{
    options_and_values updated_options;

    std::shared_ptr< raw_sensor_base > strong = _raw_sensor.lock();
    if( ! strong )
        return updated_options;
    try
    {
        strong->prepare_for_bulk_operation();
        updated_options = options_watcher::update_options();
        poll_adopted();  // the embedded filters' composite options share this power cycle
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

    return updated_options;
}

}  // namespace librealsense
