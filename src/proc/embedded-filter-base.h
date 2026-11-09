// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/embedded-filter-interface.h>
#include <src/core/options-container.h>
#include <src/core/options-watcher.h>


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
        // Composite options are registered by the filter's constructor, so they are known by the time anyone subscribes
        for( auto & id_and_option : _composite_options_by_id )
            _options_watcher.register_composite_option( id_and_option.first, id_and_option.second );
        return _options_watcher.subscribe( std::move( cb ) );
    }

protected:
    options_watcher _options_watcher;
};

}  // namespace librealsense
