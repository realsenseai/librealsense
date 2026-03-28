// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#include <realdds/dds-device-watcher.h>
#include <realdds/dds-topic.h>
#include <realdds/dds-topic-reader-thread.h>
#include <realdds/dds-device.h>
#include <realdds/dds-utilities.h>
#include <realdds/topics/dds-topic-names.h>
#include <realdds/topics/flexible-msg.h>
#include <realdds/topics/device-info-msg.h>

#include <rsutils/concurrency/thread-utils.h>
#include <rsutils/json.h>
using rsutils::json;

#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>


using namespace eprosima::fastdds::dds;
using namespace realdds;


dds_device_watcher::dds_device_watcher( std::shared_ptr< dds_participant > const & participant )
    : _device_info_topic(
        new dds_topic_reader_thread( topics::flexible_msg::create_topic( participant, topics::DEVICE_INFO_TOPIC_NAME ) ) )
    , _participant( participant )
{
    _device_info_topic->on_data_available(
        [this]()
        {
            topics::flexible_msg msg;
            dds_sample sample;
            while( topics::flexible_msg::take_next( *_device_info_topic, &msg, &sample ) )
            {
                if( ! msg.is_valid() )
                    continue;

                // NOTE: the GUID we get here is the writer on the device-info, used nowhere again in the system, which
                // can therefore be confusing. It is not the "server GUID" that's stored in the device!
                dds_guid guid;
                eprosima::fastrtps::rtps::iHandle2GUID( guid, sample.publication_handle );

                auto const j = msg.json_data();

                std::string root;
                if( ! j.nested( "topic-root", &json::is_string ).get_ex( root ) )
                {
                    // A topic-root is required, as it uniquely identifies the device across GUIDs, participants, etc.
                    LOG_DEBUG( "device-info from " << _participant->print( guid ) << " is missing a topic-root; ignoring: " << j );
                    continue;
                }

                {
                    std::lock_guard< std::mutex > lock( _devices_mutex );
                    auto it = _device_by_root.find( root );
                    if( it != _device_by_root.end() )
                    {
                        auto & device = it->second;
                        device.last_seen = now();
                        if( j.nested( "stopping", &json::is_boolean ).default_value( false ) )
                        {
                            // This device is stopping for whatever reason (e.g., HW reset); remove it
                            if( device.alive )
                            {
                                LOG_DEBUG( "[" << device.alive->debug_name() << "] device (from " << _participant->print( guid ) << ") is stopping" );
                                device_discovery_lost( device, lock );
                            }
                            continue;
                        }
                        else if( device.alive && guid == device.writer_guid)
                        {
                            // We already know about this device; likely this was a broadcast meant for someone else
                            // If guid mismatches the device was reset without us getting a subscription 
                            // lost callback in time - handle like old device coming back to life
                            continue;
                        }
                        else if( device.alive = device.in_use.lock() )
                        {
                            // Old device coming back to life
                            device.writer_guid = guid;
                            LOG_DEBUG( "[" << device.alive->debug_name() << "] device (from "
                                           << _participant->print( guid ) << ") back to life: " << std::setw( 4 ) << j );
                            topics::device_info device_info = topics::device_info::from_json( j );
                            static_cast< dds_discovery_sink * >( device.alive.get() )
                                ->on_discovery_restored( device_info );
                            if( _on_device_added )
                            {
                                rsutils::concurrency::create_thread(
                                    rsutils::concurrency::thread_category_device_monitoring, "dds-dev-add",
                                    [device = device.alive, on_device_added = _on_device_added]()
                                             { on_device_added( device ); } ).detach();
                            }
                            continue;
                        }
                        else
                        {
                            // Old device that's popped back up; recreate it
                        }
                    }
                }

                LOG_DEBUG( "DDS device (from " << _participant->print( guid ) << ") detected: " << std::setw( 4 ) << j );
                topics::device_info device_info = topics::device_info::from_json( j );

                // Add a new device record into our dds devices map
                std::shared_ptr< dds_device > new_device = std::make_shared< dds_device >( _participant, device_info );
                {
                    std::lock_guard< std::mutex > lock( _devices_mutex );
                    auto & device = _device_by_root[root];
                    device.alive = new_device;
                    device.writer_guid = guid;
                    device.last_seen = now();
                }

                // NOTE: device removals are handled via the writer-removed notification; see on_subscription_matched() below
                if( _on_device_added )
                {
                    rsutils::concurrency::create_thread(
                        rsutils::concurrency::thread_category_device_monitoring, "dds-dev-new",
                        [new_device, on_device_added = _on_device_added]() {  //
                            on_device_added( new_device );
                        } ).detach();
                }
            }
        } );

    _device_info_topic->on_subscription_matched(
        [this]( eprosima::fastdds::dds::SubscriptionMatchedStatus const & status )
        {
            if( status.current_count_change == -1 )
            {
                dds_guid const guid
                    = status.last_publication_handle.operator const eprosima::fastrtps::rtps::GUID_t &();

                std::lock_guard< std::mutex > lock( _devices_mutex );
                auto it = std::find_if( _device_by_root.begin(),
                                        _device_by_root.end(),
                                        [guid]( liveliness_map::value_type const & it )
                                        { return it.second.writer_guid == guid; } );
                // OK if not found: it's likely the broadcaster's writer itself; ignore
                if( it != _device_by_root.end() )
                {
                    auto & device = it->second;
                    if( device.alive )
                    {
                        LOG_DEBUG( "[" << device.alive->debug_name() << "] device (from " << _participant->print( guid )
                                       << ") discovery lost" );
                        device_discovery_lost( device, lock );
                    }
                }
            }
        } );

    if( ! _participant->is_valid() )
        DDS_THROW( runtime_error, "participant was not initialized" );
}


bool dds_device_watcher::is_stopped() const
{
    return ! _device_info_topic->is_running();
}


void dds_device_watcher::start()
{
    stop();
    if( ! _device_info_topic->is_running() )
        init();
    LOG_DEBUG( "DDS device watcher started on '" << _participant->get()->get_qos().name() << "' "
                                                 << realdds::print_guid( _participant->guid() ) );
}

void dds_device_watcher::stop()
{
    if( ! is_stopped() )
    {
        _device_info_topic->stop();
        //_callback_inflight.wait_until_empty();
        LOG_DEBUG( "DDS device watcher stopped" );
    }
}


dds_device_watcher::~dds_device_watcher()
{
    stop();
}


void dds_device_watcher::init()
{
    if( ! _device_info_topic->is_running() )
        _device_info_topic->run( dds_topic_reader::qos() );
}


void dds_device_watcher::device_discovery_lost( device_liveliness & device, std::lock_guard< std::mutex > & )
{
    if( auto disconnected_device = device.alive )
    {
        device.in_use = disconnected_device;
        device.alive.reset();  // no longer alive; in_use will track whether it's being used
        static_cast< dds_discovery_sink * >( disconnected_device.get() )->on_discovery_lost();

        // rest must happen outside the mutex
        rsutils::concurrency::create_thread(
            rsutils::concurrency::thread_category_device_monitoring, "dds-dev-rm",
            [disconnected_device, on_device_removed = _on_device_removed]
            {
                if( on_device_removed )
                    on_device_removed( disconnected_device );
                // If we're holding the device, it will get destroyed here, from another thread.
                // Not sure why, but if we delete outside this thread (in the listener callback), it
                // will cause some sort of invalid state in DDS. The thread will get killed and we won't get
                // any notification of the remote participant getting removed... and the process will even
                // hang on exit.
            } ).detach();
    }
}


bool dds_device_watcher::foreach_device(
    std::function< bool( std::shared_ptr< dds_device > const & ) > fn ) const
{
    std::lock_guard< std::mutex > lock( _devices_mutex );
    for( auto & root_device : _device_by_root )
    {
        auto & is = root_device.second;
        if( auto device = is.alive )
            if( ! fn( device ) )
                return false;
    }
    return true;
}


bool dds_device_watcher::is_device_broadcast( std::shared_ptr< dds_device > const & dev ) const
{
    auto & root = dev->device_info().topic_root();
    std::lock_guard< std::mutex > lock( _devices_mutex );
    auto it = _device_by_root.find( root );
    if( it == _device_by_root.end() )
        return false;
    return !! it->second.alive;
}

