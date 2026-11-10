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
    _destructing = true;
    stop();

    // An adopted watcher (a filter) can outlive its driver (the sensor)
    std::lock_guard< std::mutex > lock( _adopted_mutex );
    for( auto & weak : _adopted )
        if( auto child = weak.lock() )
            child->_driver = nullptr;
}

void options_watcher::adopt( std::shared_ptr< options_watcher > const & child )
{
    std::lock_guard< std::mutex > lock( _adopted_mutex );
    _adopted.push_back( child );
    child->_driver = this;
}

bool options_watcher::adopted_have_subscribers() const
{
    std::lock_guard< std::mutex > lock( _adopted_mutex );
    for( auto & weak : _adopted )
        if( auto child = weak.lock() )
            if( child->_on_values_changed.size() > 0 )
                return true;
    return false;
}

void options_watcher::poll_adopted()
{
    std::vector< std::shared_ptr< options_watcher > > children;
    {
        std::lock_guard< std::mutex > lock( _adopted_mutex );
        for( auto & weak : _adopted )
            if( auto child = weak.lock() )
                children.push_back( std::move( child ) );
    }
    for( auto & child : children )
        child->poll_as_adopted();
}

void options_watcher::poll_as_adopted()
{
    if( _on_values_changed.size() == 0 )
    {
        _primed = false;
        return;
    }
    auto changed = update_composite_options();
    if( _primed )
        notify( {}, changed );
    _primed = true;  // the first poll only records the baseline
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
        _composite_options.emplace( id, composite_and_value{ std::move( option ), nullptr } );
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

    if( should_stop() )
        stop();
}

rsutils::subscription options_watcher::subscribe( callback && cb )
{
    rsutils::subscription ret = _on_values_changed.subscribe( std::move( cb ) );

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
    return ( _on_values_changed.size() == 0 && ! adopted_have_subscribers() )
        || ( _options.empty() && _composite_options.empty() ) || _destructing;
}

void options_watcher::start()
{
    if( auto driver = _driver.load() ) // Polled by the driver's thread, which has to be running
    {
        if( driver->should_start() )
            driver->start();
        return;
    }

    if( _running ) // Already started
        return;

    if( _updater.joinable() ) // A previous thread exited after the last unsubscribe
        _updater.join();

    _running = true;
    _updater = std::thread( [this]() {
        update_options();
        update_composite_options();
        thread_loop();
        _running = false;
    } );
}

void options_watcher::stop()
{
    _stopping.notify_all();
    if( _updater.joinable() )
    {
        try
        {
            _updater.join();
        }
        catch( ... )
        {
            // Nothing to do on error
        }
    }
}

void options_watcher::thread_loop()
{
    while( !should_stop() )
    {
        {
            std::unique_lock<std::mutex> lock( _mutex );
            // 1. Block while paused
            _stopping.wait( lock, [this]
            {
                return should_stop() || !_paused.load();
            });
            if (should_stop())
                break;

            // 2. Periodic wait
            _stopping.wait_for( lock, _update_interval, [this]
            {
                return should_stop() || _paused.load();
            });
            // Checking for stop conditions after sleep.
            if( should_stop() )
                break;

            // If still paused, go back to waiting
            // this check is needed - do not remove because:
            // 1. predicate may not be true even if wait_for woke up
            // 2. spurious waking may happen (mostly in linux)
            // 3. the paused flag may become true between the wait_for and here
            if( _paused.load() )
                continue;
        }

        auto updated_options = update_options();
        auto updated_composites = update_composite_options();

        // Checking stop conditions after update, if stop requested no need to notify.
        if( should_stop() )
            break;

        notify( updated_options, updated_composites );
    }
}


options_watcher::options_and_values options_watcher::update_options()
{
    options_and_values updated_options;

    std::lock_guard< std::mutex > lock( _mutex );

    if( should_stop() )
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

options_watcher::composite_ids options_watcher::update_composite_options()
{
    composite_ids updated;

    std::lock_guard< std::mutex > lock( _mutex );

    for( auto & opt : _composite_options )
    {
        if( should_stop() )
            break;
        try
        {
            std::vector< uint8_t > curr_val;
            if( opt.second.sptr->is_enabled() )
                curr_val = opt.second.sptr->get_raw();

            if( ! opt.second.p_last_known_value || *opt.second.p_last_known_value != curr_val )
            {
                opt.second.p_last_known_value = std::make_shared< const std::vector< uint8_t > >( std::move( curr_val ) );
                updated.push_back( opt.first );
            }
        }
        catch( ... )
        {
            // Like scalar options: one that cannot be read right now is reported once, until it can be read again
            if( opt.second.p_last_known_value && ! opt.second.p_last_known_value->empty() )
            {
                opt.second.p_last_known_value = std::make_shared< const std::vector< uint8_t > >();
                updated.push_back( opt.first );
            }
        }
    }

    return updated;
}

void options_watcher::notify( options_and_values const & updated_options, composite_ids const & updated_composites )
{
    if( ! updated_options.empty() || ! updated_composites.empty() )
        _on_values_changed.raise( updated_options, updated_composites );
}

}  // namespace librealsense
