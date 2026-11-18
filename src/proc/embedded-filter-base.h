// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/embedded-filter-interface.h>
#include <src/core/options-container.h>
#include <src/core/options-watcher.h>
#include <src/librealsense-exception.h>

#include <memory>


namespace librealsense {

// Shared scaffolding for concrete embedded-filter base classes (decimation, temporal, close range).
// Provides the options watcher and the register_options_changed_callback wiring so each filter
// type does not have to repeat it.
class embedded_filter_base
    : public embedded_filter_interface
    , public options_container
{
public:
    virtual ~embedded_filter_base() = default;

    rsutils::subscription register_options_changed_callback( options_watcher::callback && cb ) override
    {
        return _options_watcher.subscribe( std::move( cb ) );
    }

    // The composite options are polled by the sensor's watcher; without one (DDS) by this filter's own watcher
    void use_sensor_watcher( std::weak_ptr< options_watcher > sensor_watcher )
    {
        _sensor_watcher = std::move( sensor_watcher );
        _uses_sensor_watcher = true;
    }

    void watch_own_composite_options()
    {
        for( auto id : get_supported_composite_options() )
            _options_watcher.register_composite_option( id, get_composite_option_handler( id ) );
    }

    rsutils::subscription register_composite_options_changed_callback( options_watcher::composite_callback && cb ) override
    {
        if( ! _uses_sensor_watcher )
            return _options_watcher.subscribe_composite( std::move( cb ) );

        auto watcher = _sensor_watcher.lock();
        if( ! watcher )
            throw wrong_api_call_sequence_exception( "The sensor of this embedded filter no longer exists" );

        // The sensor's watcher reports the composites of all its filters: pass on only this filter's
        auto ids = get_supported_composite_options();
        return watcher->subscribe_composite(
            [ids, cb = std::move( cb )]( options_watcher::composite_options_and_values const & all )
            {
                options_watcher::composite_options_and_values mine;
                for( auto id : ids )
                {
                    auto it = all.find( id );
                    if( it != all.end() )
                        mine.insert( *it );
                }
                if( ! mine.empty() )
                    cb( mine );
            } );
    }

protected:
    options_watcher _options_watcher;

private:
    std::weak_ptr< options_watcher > _sensor_watcher;
    bool _uses_sensor_watcher = false;
};

}  // namespace librealsense
