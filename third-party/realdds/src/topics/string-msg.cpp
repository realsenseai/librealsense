// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include <realdds/topics/string-msg.h>
#include <realdds/topics/ros2/std_msgs/msg/StringPubSubTypes.h>

#include <realdds/dds-topic.h>
#include <realdds/dds-topic-reader.h>
#include <realdds/dds-topic-writer.h>
#include <realdds/dds-utilities.h>

#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/topic/Topic.hpp>


namespace realdds {
namespace topics {


string_msg::string_msg( std_msgs::msg::String && rhs )
    : _raw( std::move( rhs ) )
{
}


string_msg & string_msg::operator=( std_msgs::msg::String && rhs )
{
    _raw = std::move( rhs );
    return *this;
}


/*static*/ std::shared_ptr< dds_topic >
string_msg::create_topic( std::shared_ptr< dds_participant > const & participant, char const * topic_name )
{
    return std::make_shared< dds_topic >( participant,
                                          eprosima::fastdds::dds::TypeSupport( new string_msg::type ),
                                          topic_name );
}


/*static*/ bool
string_msg::take_next( dds_topic_reader & reader, string_msg * output, dds_sample * sample )
{
    string_msg output_;
    if( ! output )
        output = &output_;  // use the local copy if the user hasn't provided their own
    dds_sample sample_;
    if( ! sample )
        sample = &sample_;  // use the local copy if the user hasn't provided their own
    auto status = reader->take_next_sample( &output->raw(), sample );
    if( status == ReturnCode_t::RETCODE_OK )
    {
        // Only samples for which valid_data is true should be accessed
        // valid_data indicates that the instance is still ALIVE and the `take` return an
        // updated sample
        if( ! sample->valid_data )
            output->invalidate();

        return true;
    }
    if( status == ReturnCode_t::RETCODE_NO_DATA )
    {
        // This is an expected return code and is not an error
        return false;
    }
    DDS_API_CALL_THROW( "string_msg::take_next", status );
}


dds_sequence_number string_msg::write_to( dds_topic_writer & writer ) const
{
    eprosima::fastrtps::rtps::WriteParams params;
    bool success = DDS_API_CALL(
        writer.get()->write( const_cast< std_msgs::msg::String * >( &_raw ), params ) );
    if( ! success )
    {
        LOG_ERROR( "Error writing message" );
        return 0;
    }
    return params.sample_identity().sequence_number().to64long();
}


}  // namespace topics
}  // namespace realdds
