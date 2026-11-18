// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#include <src/core/options-watcher.h>
#include <proc/synthetic-stream.h>
#include <rsutils/json.h>

using rsutils::json;


namespace librealsense {


options_watcher::options_watcher( std::chrono::milliseconds update_interval )
    : _update_interval( update_interval )
    , _destructing( false )
    , _paused( false )
{
}

options_watcher::~options_watcher()
{
    {
        std::lock_guard< std::mutex > lock( _mutex );  // so the updater cannot miss it between its check and its wait
        _destructing = true;
    }
    _stopping.notify_all();
    if( _updater.joinable() )
        _updater.join();
}

void options_watcher::register_option( rs2_option id, std::shared_ptr< option > option )
{
    {
        std::lock_guard< std::mutex > lock( _mutex );
        _options[id] = { option };
    }

    if( should_start() )
        start();
}

void options_watcher::register_composite_option( rs2_composite_option_id id, std::shared_ptr< composite_option_interface > option )
{
    {
        std::lock_guard< std::mutex > lock( _mutex );
        _composite_options.emplace( id, composite_option_and_value{ std::move( option ), nullptr } );
    }

    if( should_start() )
        start();
}

void options_watcher::unregister_option( rs2_option id )
{
    {
        std::lock_guard< std::mutex > lock( _mutex );
        _options.erase( id );
    }

    _stopping.notify_all();  // the updater goes idle if this was the last option
}

rsutils::subscription options_watcher::subscribe( callback && cb )
{
    rsutils::subscription ret = _on_values_changed.subscribe( std::move( cb ) );

    if( should_start() )
        start();

    return ret;
}

rsutils::subscription options_watcher::subscribe_composite( composite_callback && cb )
{
    rsutils::subscription ret = _on_composite_values_changed.subscribe( std::move( cb ) );

    if( should_start() )
        start();

    return ret;
}

bool options_watcher::should_start() const
{
    return ! should_stop();
}

bool options_watcher::should_stop() const
{
    return ( _on_values_changed.size() == 0 && _on_composite_values_changed.size() == 0 )
        || ( _options.empty() && _composite_options.empty() ) || _destructing;
}

void options_watcher::start()
{
    // The updater lives as long as the watcher, idling while there is nothing to poll
    std::lock_guard< std::mutex > lock( _mutex );
    if( ! _updater.joinable() )
        _updater = std::thread( [this]() { thread_loop(); } );
    _stopping.notify_all();
}

void options_watcher::thread_loop()
{
    while( ! _destructing )
    {
        {
            // Idle until there is something to poll and the device is free to be queried (not in reset or FW update)
            std::unique_lock< std::mutex > lock( _mutex );
            _stopping.wait( lock, [this] { return _destructing || ( ! should_stop() && ! _paused.load() ); } );
        }
        if( _destructing )
            break;

        // A fresh baseline after each idle period, so changes made while nobody was subscribed are not reported
        options_and_values updated_options;
        composite_options_and_values updated_composites;
        update( updated_options, updated_composites );

        while( ! should_stop() && ! _paused.load() )
        {
            {
                std::unique_lock< std::mutex > lock( _mutex );
                _stopping.wait_for( lock, _update_interval, [this] { return should_stop() || _paused.load(); } );
            }
            // Spurious wakeups and a pause that began after the wait are covered by the loop condition
            if( should_stop() || _paused.load() )
                break;

            update( updated_options, updated_composites );

            // Checking stop conditions after update, if stop requested no need to notify.
            if( should_stop() )
                break;

            notify( updated_options, updated_composites );
        }
    }
}


void options_watcher::update( options_and_values & updated_options, composite_options_and_values & updated_composites )
{
    updated_options = update_options();
    updated_composites = update_composite_options();
}

options_watcher::options_and_values options_watcher::update_options()
{
    options_and_values updated_options;

    std::lock_guard< std::mutex > lock( _mutex );

    if( should_stop() || _paused.load() )  // a pause can land between the caller's check and this lock
        return updated_options;

    for( auto & opt : _options )
    {
        try
        {
            json curr_val;
            if( opt.second.sptr->is_enabled() )
                curr_val = opt.second.sptr->get_value();

            if( ! opt.second.p_last_known_value || *opt.second.p_last_known_value != curr_val )
            {
                opt.second.p_last_known_value = std::make_shared< const json >( std::move( curr_val ) );
                updated_options[opt.first] = opt.second;
            }
        }
        catch( ... )
        {
            // Some options cannot be queried all the time (i.e. streaming only) - so if we HAD a value, it needs to be
            // removed!
            if( opt.second.p_last_known_value && ! opt.second.p_last_known_value->is_null() )
            {
                opt.second.p_last_known_value = std::make_shared< const json >();
                updated_options[opt.first] = opt.second;
            }
        }

        // Checking stop conditions after each query to ensure stop when requested.
        if( should_stop() )
            break;
    }

    return updated_options;
}

options_watcher::composite_options_and_values options_watcher::update_composite_options()
{
    composite_options_and_values updated;

    std::lock_guard< std::mutex > lock( _mutex );

    for( auto & opt : _composite_options )
    {
        if( should_stop() || _paused.load() )
            break;
        try
        {
            std::vector< uint8_t > curr_val;
            if( opt.second.sptr->is_enabled() )
                curr_val = opt.second.sptr->get_raw();

            if( ! opt.second.p_last_known_value || *opt.second.p_last_known_value != curr_val )
            {
                opt.second.p_last_known_value = std::make_shared< const std::vector< uint8_t > >( std::move( curr_val ) );
                updated[opt.first] = opt.second;
            }
        }
        catch( ... )
        {
            // Like scalar options: one that cannot be read right now is reported once, until it can be read again
            if( opt.second.p_last_known_value && ! opt.second.p_last_known_value->empty() )
            {
                opt.second.p_last_known_value = std::make_shared< const std::vector< uint8_t > >();
                updated[opt.first] = opt.second;
            }
        }
    }

    return updated;
}

void options_watcher::notify( options_and_values const & updated_options,
                              composite_options_and_values const & updated_composites )
{
    if( ! updated_options.empty() )
        _on_values_changed.raise( updated_options );
    if( ! updated_composites.empty() )
        _on_composite_values_changed.raise( updated_composites );
}

}  // namespace librealsense
