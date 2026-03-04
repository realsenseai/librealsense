// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023-4 RealSense, Inc. All Rights Reserved.

#include <realdds/dds-topic-reader-thread.h>
#include <realdds/dds-topic.h>
#include <realdds/dds-subscriber.h>
#include <realdds/dds-utilities.h>

#include <rsutils/concurrency/thread-utils.h>

#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/topic/Topic.hpp>

#include <fastdds/dds/core/condition/GuardCondition.hpp>
#include <fastdds/dds/core/condition/WaitSet.hpp>

namespace realdds {


dds_topic_reader_thread::dds_topic_reader_thread( std::shared_ptr< dds_topic > const & topic )
    : dds_topic_reader_thread( topic, std::make_shared< dds_subscriber >( topic->get_participant() ) )
{
}


dds_topic_reader_thread::dds_topic_reader_thread( std::shared_ptr< dds_topic > const & topic,
                                                  std::shared_ptr< dds_subscriber > const & subscriber )
    : super( topic, subscriber )
    , _stopped( nullptr )
{
}


dds_topic_reader_thread::~dds_topic_reader_thread()
{
    stop();  // Make sure thread is stopped!
}


void dds_topic_reader_thread::run( qos const & rqos )
{
    if( ! _on_data_available )
        DDS_THROW( runtime_error, "on-data-available must be provided" );

    _reader = DDS_API_CALL( _subscriber->get()->create_datareader( _topic->get(), rqos ) );
    _stopped = std::make_shared< eprosima::fastdds::dds::GuardCondition >();
    
    _th = rsutils::concurrency::create_thread(
        rsutils::concurrency::thread_category_network, "dds-reader",
        [this,
         weak = std::weak_ptr< dds_topic_reader >( shared_from_this() ),  // detect lifetime
         name = _topic->get()->get_name(),
         stopped = _stopped]  // hold a copy so the wait-set is valid even if the reader is destroyed
        {
            eprosima::fastdds::dds::WaitSet wait_set;
            if( auto strong = weak.lock() )
            {
                auto & condition = _reader->get_statuscondition();
                condition.set_enabled_statuses( eprosima::fastdds::dds::StatusMask::data_available()
                                                << eprosima::fastdds::dds::StatusMask::subscription_matched()
                                                << eprosima::fastdds::dds::StatusMask::sample_lost() );
                wait_set.attach_condition( condition );

                wait_set.attach_condition( *stopped );
            }
            // We'll keep locking the object so it cannot destruct mid-callback, and exit out if we detect destruction
            while( auto strong = weak.lock() )
            {
                eprosima::fastdds::dds::ConditionSeq active_conditions;
                wait_set.wait( active_conditions, eprosima::fastrtps::c_TimeInfinite );
                if( stopped->get_trigger_value() )
                    break;

                auto & changed = _reader->get_status_changes();
                if( changed.is_active( eprosima::fastdds::dds::StatusMask::sample_lost() ) )
                {
                    eprosima::fastdds::dds::SampleLostStatus status;
                    _reader->get_sample_lost_status( status );
                    on_sample_lost( _reader, status );
                    if( stopped->get_trigger_value() )
                        break;
                }
                if( changed.is_active( eprosima::fastdds::dds::StatusMask::data_available() ) )
                {
                    on_data_available( _reader );
                    if( stopped->get_trigger_value() )
                        break;
                }
                if( changed.is_active( eprosima::fastdds::dds::StatusMask::subscription_matched() ) )
                {
                    eprosima::fastdds::dds::SubscriptionMatchedStatus status;
                    _reader->get_subscription_matched_status( status );
                    on_subscription_matched( _reader, status );
                    if( stopped->get_trigger_value() )
                        break;
                }
            }
        } );
}


void dds_topic_reader_thread::stop()
{
    if( _th.joinable() )
    {
        _stopped->set_trigger_value( true );
        // If we try to stop from within the thread (e.g., inside on_data_available), join() will terminate!
        // If we detect such a case, then it's also possible our object will get destroyed (we may get here from the
        // dtor) so we detach the thread instead.
        if( _th.get_id() != std::this_thread::get_id() )
            _th.join();
        else
            _th.detach();
    }
    super::stop();
}


}  // namespace realdds
