// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#pragma once

#include <realdds/dds-defines.h>

#include <realdds/topics/ros2/std_msgs/msg/String.h>

#include <string>
#include <memory>


namespace std_msgs {
namespace msg {
class StringPubSubType;
}  // namespace msg
}  // namespace std_msgs


namespace realdds {


class dds_participant;
class dds_topic;
class dds_topic_reader;
class dds_topic_writer;


namespace topics {


class string_msg
{
    std_msgs::msg::String _raw;

public:
    using type = std_msgs::msg::StringPubSubType;

    string_msg() = default;

    // Disable copy
    string_msg( const string_msg & ) = delete;
    string_msg & operator=( const string_msg & ) = delete;

    // Move is OK
    string_msg( string_msg && ) = default;
    string_msg( std_msgs::msg::String && );
    string_msg & operator=( string_msg && ) = default;
    string_msg & operator=( std_msgs::msg::String && );

    bool is_valid() const { return ! _raw.data().empty(); }
    void invalidate() { _raw.data( std::string() ); }

    std_msgs::msg::String & raw() { return _raw; }
    std_msgs::msg::String const & raw() const { return _raw; }

    std::string const & data() const { return _raw.data(); }
    void set_data( std::string new_data ) { _raw.data( std::move( new_data ) ); }

    static std::shared_ptr< dds_topic > create_topic( std::shared_ptr< dds_participant > const & participant,
                                                      char const * topic_name );

    // This helper method will take the next sample from a reader.
    //
    // Returns true if successful. Make sure you still check is_valid() in case the sample info isn't!
    // Returns false if no more data is available.
    // Will throw if an unexpected error occurs.
    //
    static bool take_next( dds_topic_reader &,
                           string_msg * output,
                           dds_sample * optional_sample = nullptr );

    // Returns some unique (to the writer) identifier for the sample that was sent, or 0 if unsuccessful
    dds_sequence_number write_to( dds_topic_writer & ) const;
};


}  // namespace topics
}  // namespace realdds
