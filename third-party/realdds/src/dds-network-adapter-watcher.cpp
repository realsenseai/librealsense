// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#include <realdds/dds-network-adapter-watcher.h>
#include <rsutils/shared-ptr-singleton.h>
#include <rsutils/signal.h>
#include <rsutils/concurrency/thread-utils.h>
#include <rsutils/easylogging/easyloggingpp.h>
#include <rsutils/time/stopwatch.h>

#include <fastrtps/utils/IPFinder.h>
using eprosima::fastrtps::rtps::IPFinder;


namespace realdds {


using ip_set = dds_network_adapter_watcher::ip_set;


namespace detail {


class network_adapter_watcher_singleton
{
    std::shared_ptr< rsutils::os::network_adapter_watcher > _adapter_watcher;

    ip_set _ips;

    std::thread _th;
    rsutils::time::stopwatch _time_since_update;

    using public_signal
        = rsutils::public_signal< network_adapter_watcher_singleton, ip_set const & /*new*/, ip_set const & /*old*/ >;

public:
    public_signal callbacks;

    network_adapter_watcher_singleton()
        : _adapter_watcher( std::make_shared< rsutils::os::network_adapter_watcher >(
            [this]()
            {
                // Adapters have changed, but we won't see the effects for some time (several seconds)
                // But we don't want to wait here, so we start another thread
                _time_since_update.reset();
                if( ! _th.joinable() )
                {
                    _th = rsutils::concurrency::create_thread(
                        rsutils::concurrency::thread_category_network, "dds-ip-wait",
                        [this, weak = std::weak_ptr< rsutils::os::network_adapter_watcher >( _adapter_watcher )]
                        {
                            LOG_DEBUG( "waiting for IP changes" );
                            ip_set new_ips, old_ips;
                            while( auto strong = weak.lock() )
                            {
                                if( _time_since_update.get_elapsed() > std::chrono::seconds( 15 ) )
                                    break;
                                std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
                                new_ips.clear();
                                old_ips.clear();
                                update_ips( &new_ips, &old_ips );
                                if( new_ips.size() || old_ips.size() )
                                    callbacks.raise( new_ips, old_ips );
                            }
                            LOG_DEBUG( "done waiting for IP changes" );
                        } );
                }
            } ) )
    {
        update_ips();
    }

    ~network_adapter_watcher_singleton()
    {
        _adapter_watcher.reset();  // signal the thread to finish
        try
        {
            if (_th.joinable())
                _th.join();
        }
        catch (std::exception& e)
        {
            LOG_DEBUG("Network adapter watcher termination failed: " << e.what());
        }
    }

    void update_ips( ip_set * p_new_ips = nullptr, ip_set * p_old_ips = nullptr )
    {
        auto ips = dds_network_adapter_watcher::current_ips();

        // Implement a set-difference function
        // We can use the fact that a set sorts its items to efficiently do this
        // (see https://en.cppreference.com/w/cpp/algorithm/set_difference)
        auto new_it = ips.begin();
        auto const new_end = ips.end();
        auto old_it = _ips.begin();
        auto const old_end = _ips.end();

        while( new_it != new_end )
        {
            if( old_it == old_end  || *new_it < *old_it )
            {
                // New IP
                LOG_DEBUG( "+adapter " << *new_it );
                if( p_new_ips )
                    p_new_ips->insert( *new_it );
                ++new_it;
                continue;
            }

            if( *old_it < *new_it )
            {
                // Old IP
                LOG_DEBUG( "-adapter " << *old_it );
                if( p_old_ips )
                    p_old_ips->insert( *old_it );
                ++old_it;
                continue;
            }

            // *new_it == *old_it --> both new and old have this IP
            ++new_it;
            ++old_it;
        }
        while( old_it != old_end )
        {
            // Old IP
            LOG_DEBUG( "-adapter " << *old_it );
            if( p_old_ips )
                p_old_ips->insert( *old_it );
            ++old_it;
        }

        _ips = std::move( ips );
    }
};

static rsutils::shared_ptr_singleton< network_adapter_watcher_singleton > the_adapter_watcher;


}  // namespace detail


dds_network_adapter_watcher::dds_network_adapter_watcher( callback && cb )
    : _singleton( detail::the_adapter_watcher.instance() )  // keep it alive
    , _subscription( _singleton->callbacks.subscribe(
          [cb]( ip_set const & new_ips, ip_set const & old_ips ) { cb(); } ) )
{
    // As long as someone keeps a pointer to a dds_network_adapter_watcher, the singleton will be kept alive and it will
    // watch for changes; as soon as all instances disappear, the singleton will disappear and the watch should stop.
}


/*static*/ ip_set dds_network_adapter_watcher::current_ips()
{
    std::vector< IPFinder::info_IP > local_interfaces;
    IPFinder::getIPs( &local_interfaces, false );
    ip_set ips;
    for( auto const & ip : local_interfaces )
    {
        if( ip.type != IPFinder::IP4 && ip.type != IPFinder::IP4_LOCAL )
            continue;
        ips.insert( ip.name );
    }
    return ips;
}


}  // namespace realdds
