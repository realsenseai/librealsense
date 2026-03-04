// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#include <realdds/dds-notification-server.h>

#include <realdds/dds-participant.h>
#include <realdds/dds-publisher.h>
#include <realdds/dds-stream-server.h>
#include <realdds/dds-utilities.h>
#include <realdds/dds-topic.h>
#include <realdds/dds-topic-writer.h>

#include <fastdds/dds/topic/Topic.hpp>

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/publisher/qos/PublisherQos.hpp>

#include <rsutils/json.h>


using namespace eprosima::fastdds::dds;

namespace realdds {


dds_notification_server::dds_notification_server( std::shared_ptr< dds_publisher > const & publisher,
                                                  const std::string & topic_name )
    : _publisher( publisher )
    , _notifications_loop(
          [this]( dispatcher::cancellable_timer timer )
          {
              if( _active )
              {
                  // We can send 2 types of notifications:
                  // * Initialize notifications that should be sent for every new reader (like sensors & profiles data)
                  // * Instant notifications that can be sent upon an event and is not required to be
                  //   resent to new readers
                  std::unique_lock< std::mutex > lock( _notification_send_mutex );
                  _send_notification_cv.wait( lock,
                                              [this]()
                                              { return ! _active || _send_init_msgs || _new_instant_notification; } );

                  if( _active && _send_init_msgs )
                  {
                      // Note: new discoveries can happen while we're sending them! (though they'll wait on the mutex)
                      _send_init_msgs = false;
                      send_discovery_notifications();
                  }

                  if( _active && _new_instant_notification )
                  {
                      // Send all instant notifications
                      topics::raw::flexible notification;
                      while( _instant_notifications.try_dequeue( &notification ) )
                      {
                          DDS_API_CALL( _writer->get()->write( &notification ) );
                      }
                      _new_instant_notification = false;
                  }
              }
          },
          rsutils::concurrency::thread_category_network, "dds-notif" )
{
    auto topic = topics::flexible_msg::create_topic( publisher->get_participant(), topic_name.c_str() );
    _writer = std::make_shared< dds_topic_writer >( topic, publisher );

    _writer->on_publication_matched( [this]( PublicationMatchedStatus const & info ) {
        if( info.current_count_change == 1 )
        {
            trigger_discovery_notifications();
        }
        else if( info.current_count_change == -1 )
        {
        }
        else
        {
            LOG_ERROR( info.current_count_change << " is not a valid value for on_publication_matched" );
        }
    } );

    dds_topic_writer::qos wqos( RELIABLE_RELIABILITY_QOS ); // the default, but worth mentioning
    // Writer can finish reader-writer handshake and start transmitting the discovery messages before the reader have
    // finished the handshake and can receivethe messages.
    // If history is too small writer will not be able to re-transmit needed samples.
    // Setting history to cover known use-cases plus some spare
    wqos.history().depth = 24;
    _writer->override_qos_from_json( wqos, publisher->get_participant()->settings().nested( "device", "notification" ) );

    _writer->run( wqos );
}


dds_guid const & dds_notification_server::guid() const
{
    return _writer->guid();
}


void dds_notification_server::run()
{
    if( ! _active )
    {
        _active = true;
        _notifications_loop.start();
        // if any discovery happened before we started, _send_init_msgs will be true and we shouldn't even wait...
    }
}


dds_notification_server::~dds_notification_server()
{
    // Mark us inactive and wake up the active object so we can properly stop it
    if( _active )
    {
        _active = false;
        _send_notification_cv.notify_all();
        _notifications_loop.stop();
    }
}


void dds_notification_server::trigger_discovery_notifications()
{
    {
        std::lock_guard< std::mutex > lock( _notification_send_mutex );
        _send_init_msgs = true;
    }
    _send_notification_cv.notify_all();
}


void dds_notification_server::send_notification( topics::flexible_msg && notification )
{
    if( ! is_running() )
        DDS_THROW( runtime_error, "cannot send notification while server isn't running" );

    auto raw_notification = std::move( notification ).to_raw();

    std::unique_lock< std::mutex > lock( _notification_send_mutex );
    if( ! _instant_notifications.enqueue( std::move( raw_notification ) ) )
        LOG_ERROR( "error while trying to enqueue a notification" );
    _new_instant_notification = true;
    _send_notification_cv.notify_all();
}


void dds_notification_server::add_discovery_notification( topics::flexible_msg && notification )
{
    if( is_running() )
        DDS_THROW( runtime_error, "cannot add discovery notification while server is running" );

    _discovery_notifications.push_back( std::move( notification ).to_raw() );
}


void dds_notification_server::send_discovery_notifications()
{
    // Send all initialization notifications
    LOG_DEBUG( "broadcasting discovery notifications" );
    for( auto & notification : _discovery_notifications )
    {
        DDS_API_CALL( _writer->get()->write( &notification ) );
    }
}


}  // namespace realdds
