// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.
#pragma once

#include <librealsense2/h/rs_option.h>
#include <src/core/option-interface.h>

#include <rsutils/signal.h>
#include <rsutils/concurrency/concurrency.h>
#include <rsutils/json-fwd.h>

#include <map>
#include <chrono>
#include <functional>
#include <memory>
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
    using callback = std::function< void( options_and_values const & ) >;

public:
    options_watcher( std::chrono::milliseconds update_interval = std::chrono::milliseconds( 1000 ) );
    ~options_watcher();

    void register_option( rs2_option id, std::shared_ptr< option > option );
    void unregister_option( rs2_option id );

    rsutils::subscription subscribe( callback && cb );

    void set_update_interval( std::chrono::milliseconds update_interval ) { _update_interval = update_interval; }

    inline void pause() { _paused.store(true, std::memory_order_relaxed); }
    inline void unpause() { _paused.store(false, std::memory_order_relaxed);
                          _stopping.notify_all();}

protected:
    bool should_start() const;
    bool should_stop() const;
    void start();
    void stop();
    void thread_loop();
    virtual options_and_values update_options();
    void notify( options_and_values const & updated_options );

    options_and_values _options;
    rsutils::signal< options_and_values const & > _on_values_changed;
    std::chrono::milliseconds _update_interval;
    std::thread _updater;
    std::mutex _mutex;
    std::condition_variable _stopping;
    std::atomic_bool _destructing;
    std::atomic_bool _paused;
};


}  // namespace librealsense
