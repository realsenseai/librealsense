// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.
#pragma once

#include <librealsense2/h/rs_option.h>
#include <src/core/option-interface.h>
#include <src/composite-option-interface.h>
#include <librealsense2/h/rs_composite_option.h>

#include <rsutils/signal.h>
#include <rsutils/concurrency/concurrency.h>
#include <rsutils/json-fwd.h>

#include <map>
#include <chrono>
#include <functional>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>


namespace librealsense {


// Watches registered options value and notifies interested users.
// When a user subscribes to notification the options_watcher will automatically update (query) registered options
// values in set time intervals (creates a thread). If one or more of the values have changed the watcher will notify
// through the callback subscription.
class options_watcher
{
public:
    struct option_and_value
    {
        std::shared_ptr< option > sptr;
        std::shared_ptr< const rsutils::json > p_last_known_value;
    };

    using options_and_values = std::map< rs2_option, option_and_value >;
    using composite_ids = std::vector< rs2_composite_option_id >;
    using callback = std::function< void( options_and_values const &, composite_ids const & ) >;

public:
    options_watcher( std::chrono::milliseconds update_interval = std::chrono::milliseconds( 1000 ) );
    ~options_watcher();

    void register_option( rs2_option id, std::shared_ptr< option > option );
    void unregister_option( rs2_option id );

    // Composite options are compared by their raw payload; a change is reported by id only
    void register_composite_option( rs2_composite_option_id id, std::shared_ptr< composite_option_interface > option );

    rsutils::subscription subscribe( callback && cb );

    // Let this watcher's thread also poll the composite options of 'child', so one thread (and one bulk-operation
    // scope, see synthetic_options_watcher) serves both. The child runs no thread of its own.
    void adopt( std::shared_ptr< options_watcher > const & child );

    void set_update_interval( std::chrono::milliseconds update_interval ) { _update_interval = update_interval; }

    inline void pause() { _paused.store(true); }
    inline void unpause() { _paused.store(false);
                          _stopping.notify_all();}

protected:
    bool should_start() const;
    bool should_stop() const;
    void start();
    void stop();
    void thread_loop();
    virtual options_and_values update_options();
    composite_ids update_composite_options();
    void notify( options_and_values const & updated_options, composite_ids const & updated_composites );
    void poll_adopted();    // run by the driving thread, inside its bulk-operation scope
    void poll_as_adopted(); // composite options only: adopted watchers have no scalar options
    bool adopted_have_subscribers() const;

    options_and_values _options;
    struct composite_and_value
    {
        std::shared_ptr< composite_option_interface > sptr;
        std::shared_ptr< const std::vector< uint8_t > > p_last_known_value;
    };
    std::map< rs2_composite_option_id, composite_and_value > _composite_options;
    rsutils::signal< options_and_values const &, composite_ids const & > _on_values_changed;
    std::chrono::milliseconds _update_interval;
    std::thread _updater;
    std::mutex _mutex;
    std::condition_variable _stopping;
    std::atomic_bool _destructing;
    std::atomic_bool _paused;
    std::atomic_bool _running{ false };

    std::atomic< options_watcher * > _driver{ nullptr };  // the watcher whose thread polls this one, if adopted
    bool _primed = false;                                  // adopted only: baseline recorded, so changes are reported
    mutable std::mutex _adopted_mutex;
    std::vector< std::weak_ptr< options_watcher > > _adopted;
};


}  // namespace librealsense
