// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#include "dds-device-impl.h"

#include <realdds/dds-participant.h>
#include <realdds/dds-topic-reader-thread.h>
#include <realdds/dds-topic-writer.h>
#include <realdds/dds-subscriber.h>
#include <realdds/dds-option.h>
#include <realdds/topics/dds-topic-names.h>
#include <realdds/topics/flexible-msg.h>
#include <realdds/dds-guid.h>
#include <realdds/dds-time.h>

#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>

#include <rsutils/time/timer.h>
#include <rsutils/string/from.h>
#include <rsutils/json.h>

#include <cassert>
#include <realdds/dds-embedded-filter.h>

using rsutils::json;


namespace {


json device_settings( std::shared_ptr< realdds::dds_participant > const & participant )
{
    auto settings = participant->settings().nested( "device" );
    if( ! settings )
        // Nothing there: default is empty object
        return json::object();
    if( ! settings.is_object() )
        // Device settings, if they exist, must be an object!
        DDS_THROW( runtime_error, "participant 'device' settings must be an object: " << settings );
    return settings;
}


}


namespace realdds {


void dds_device::impl::set_state( state_t new_state )
{
    if( new_state == _state )
        return;

    if( state_t::OFFLINE == new_state ) // Discovery lost
    {
        // Close dds entities that are not needed when offline, and will be re-created when back online. Avoids traffic when not needed.
        // Note - do not close the control writer, as it gives us our GUID, which we want to keep constant.
        if( _notifications_reader )
        {
            _notifications_reader->stop();
            _notifications_reader.reset();
        }

        // Reset initialization data, we expect to receive it again if connection will be re-established.
        reset();
    }

    if( state_t::INITIALIZING == new_state ) // Discovery restored
    {
        create_notifications_reader();
    }

    if( state_t::READY == new_state )
    {
        if( _metadata_reader )
        {
            auto md_settings = _device_settings.nested( "metadata" );
            if( md_settings.exists() && ! md_settings.is_object() )
            {
                LOG_DEBUG( "[" << debug_name() << "] ... metadata is available but device/metadata is disabled" );
                _metadata_reader.reset();
            }
            else
            {
                LOG_DEBUG( "[" << debug_name() << "] ... metadata is enabled" );
                dds_topic_reader::qos rqos( eprosima::fastdds::dds::BEST_EFFORT_RELIABILITY_QOS );
                rqos.history().depth = 10; // Support receive metadata from multiple streams
                rqos.override_from_json( md_settings );
                _metadata_reader->run( rqos );
            }
        }
        // Remove stream if object not created (only stream options received, not stream header)
        for( auto stream = _streams.begin(); stream != _streams.end(); stream++ )
            if( ! stream->second )
                stream = _streams.erase( stream );  

        LOG_DEBUG( "[" << debug_name() << "] device is ready" );
    }

    _state = new_state;
}


dds_device::impl::impl( std::shared_ptr< dds_participant > const & participant,
                        topics::device_info const & info )
    : _info( info )
    , _participant( participant )
    , _subscriber( std::make_shared< dds_subscriber >( participant ) )
    , _device_settings( device_settings( participant ) )
    , _reply_timeout_ms(
          _device_settings.nested( "control", "reply-timeout-ms" ).default_value< size_t >( 2500 ) )
{
    create_control_writer();
    create_notifications_reader();
}


dds_device::impl::~impl()
{
    if( _notifications_reader )
        _notifications_reader->stop();
    if( _metadata_reader )
        _metadata_reader->stop();
}


void dds_device::impl::reset()
{
    // _info should already be up-to-date
    // _participant doesn't change
    // _subscriber can stay the same
    // _reply_timeout_ms is using same settings

    // notifications/control/metadata topic, since the topic root hasn't changed, are still valid

    // Streams need to be reset
    _server_guid = {};
    _n_streams_expected = 0;
    _streams.clear();
    _stream_header_received.clear();
    _stream_options_received.clear();
    _device_header_received = false;
    _device_options_received = false;
    _options.clear();
    _extrinsics_map.clear();
    if( _metadata_reader )
        _metadata_reader->stop();
    _metadata_reader.reset();
}


dds_guid const & dds_device::impl::guid() const
{
    return _control_writer->guid();
}


std::string dds_device::impl::debug_name() const
{
    return rsutils::string::from() << _info.debug_name() << _participant->print( guid() );
}


void dds_device::impl::on_notification( json && j, dds_sample const & notification_sample )
{
    typedef std::map< std::string,
                      void ( dds_device::impl::* )( json const &,
                                                    dds_sample const & ) >
        notification_handlers;
    static notification_handlers const _notification_handlers{
        { topics::reply::set_option::id, &dds_device::impl::on_set_option },
        { topics::reply::query_option::id, &dds_device::impl::on_set_option },  // Same handling as on_set_option
        { topics::reply::query_options::id, &dds_device::impl::on_query_options },
        { topics::reply::set_filter::id, &dds_device::impl::on_set_filter },
        { topics::reply::query_filter::id, &dds_device::impl::on_query_filter },
        { topics::notification::device_header::id, &dds_device::impl::on_device_header },
        { topics::notification::device_options::id, &dds_device::impl::on_device_options },
        { topics::notification::stream_header::id, &dds_device::impl::on_stream_header },
        { topics::notification::stream_options::id, &dds_device::impl::on_stream_options },
        { topics::notification::log::id, &dds_device::impl::on_log },
        { topics::notification::calibration_changed::id, &dds_device::impl::on_calibration_changed },
    };

    auto const control = j.nested( topics::reply::key::control );
    auto const control_sample = control ? j.nested( topics::reply::key::sample ) : rsutils::json_ref( rsutils::missing_json );

    try
    {
        // First handle the notification
        // An 'id' is mandatory, but if it's a response to a control it can be contained there
        auto id = ( control_sample ? control.get_json() : j ).nested( topics::notification::key::id ).string_ref();
        auto it = _notification_handlers.find( id );
        if( it != _notification_handlers.end() )
            ( this->*( it->second ) )( j, notification_sample );
        _on_notification.raise( id, j );
    }
    catch( std::exception const & e )
    {
        LOG_DEBUG( "[" << debug_name() << "] notification error: " << e.what() << "\n    " << j );
    }
    catch( ... )
    {
        LOG_DEBUG( "[" << debug_name() << "] notification error: unknown exception\n    " << j );
    }

    try
    {
        // Check if this is a reply - maybe someone's waiting on it...
        if( control_sample )
        {
            // ["<prefix>.<entity>", <sequence-number>]
            if( control_sample.size() == 2 && control_sample.is_array() )
            {
                // We have to be the ones who sent the control!
                auto const origin_guid = guid_from_string( control_sample[0].get< std::string >() );
                auto const control_guid = _control_writer->guid();
                if( origin_guid == control_guid )
                {
                    auto const sequence_number = control_sample[1].get< uint64_t >();
                    std::unique_lock< std::mutex > lock( _replies_mutex );
                    auto replyit = _replies.find( sequence_number );
                    if( replyit != _replies.end() )
                    {
                        replyit->second = std::move( j );
                        _replies_cv.notify_all();
                    }
                    else
                    {
                        // Nobody's waiting for it - but we can still log any errors:
                        dds_device::check_reply( j );
                    }
                }
            }
        }
    }
    catch( std::exception const & e )
    {
        LOG_DEBUG( "[" << debug_name() << "] reply error: " << e.what() << "  " << j );
    }
    catch( ... )
    {
        LOG_DEBUG( "[" << debug_name() << "] reply error: unknown exception  " << j );
    }
}

void dds_device::impl::on_set_filter(rsutils::json const& j, dds_sample const&)
{
    if( ! is_ready() )
        return;

    // This is the handler for "set-filter", meaning someone sent a control request to set a
    // filter value. A value will be returned, and it is then updated in the cached values

    std::string explanation;
    if (!dds_device::check_reply(j, &explanation))
        return;

    // We need the original control request as part of the reply, 
    // otherwise we can't know what filter this is for
    auto control = j.nested(topics::reply::key::control);
    if (!control.is_object())
        DDS_THROW(runtime_error, "missing control object");

    std::string const& stream_name = control.nested(topics::control::set_filter::key::stream_name).
        string_ref_or_empty();

    dds_embedded_filters filters;
    if (!stream_name.empty())
    {
        auto stream_it = _streams.find(stream_name);
        if (stream_it == _streams.end())
            DDS_THROW(runtime_error, "stream '" + stream_name + "' not found");
        filters = stream_it->second->embedded_filters();
    }
    auto filter_name_j = j.nested(topics::reply::set_filter::key::name);
    if (!filter_name_j.exists())
        DDS_THROW(runtime_error, "missing name");

    auto filter_params_j = j.nested(topics::reply::set_filter::key::options);
    if (!filter_params_j.exists())
        DDS_THROW(runtime_error, "missing filter_params");

    auto& filter_name = filter_name_j.string_ref();
    for (auto& filter : filters)
    {
        if (filter->get_name() == filter_name)
        {
            filter->set_options(filter_params_j);  // throws!
            return;
        }
    }
    throw std::runtime_error("filter '" + filter_name + "' not found");
}

void dds_device::impl::on_query_filter(json const& j, dds_sample const&)
{
    if( ! is_ready() )
        return;

    // This is the notification for "query-filter", which can get sent as a reply to a control or independently by the
    // device. It takes the same form & handling either way.
    // 
    // E.g.:
    // {
    //  "id": "query-filter",
    //  "name" : "Decimation Filter",
    //  "sample" : ["010faf31ac07879500000000.0203", 13] ,
    //  "stream-name" : "Depth"
    //  "control" : {
    //      "id": "query-filter",
    //      "name" : "Decimation Filter",
    //      "options" : {
    //      "Toggle": 1,
    //      "Magnitude" : 2
    //      }
    //      "stream-name" : "Depth"
    //      }
    //  }

    auto stream_name = j.nested(topics::reply::query_filter::key::stream_name).string_ref();
    auto filter_name = j.nested(topics::reply::query_filter::key::name).string_ref();
    auto filter_options = j.nested(topics::reply::query_filter::key::options);

    for (auto& stream : _streams)
    {
        // Finding the relevant stream
        if (stream.first == stream_name)
        {
            // Finding the filter and set its options
            for (auto& filter : stream.second->embedded_filters())
            {
                if (filter->get_name() == filter_name)
                {
                    filter->set_options(filter_options);
                    return;
                }
            }
        }
    }

    DDS_THROW(runtime_error, "Embedded filter '" + filter_name + "' not found");
}


void dds_device::impl::on_set_option( json const & j, dds_sample const & )
{
    if( ! is_ready() )
        return;

    // This is the handler for "set-option" or "query-option", meaning someone sent a control request to set/get an
    // option value. In either case a value will be returned; we want to update our local copy to reflect it:

    std::string explanation;
    if( ! dds_device::check_reply( j, &explanation ) )
        return;  // we don't care about errors

    // We need the original control request as part of the reply, otherwise we can't know what option this is for
    auto control = j.nested( topics::reply::key::control );
    if( ! control.is_object() )
        DDS_THROW(runtime_error, "missing control object" );

    // Find the relevant (stream) options to update
    dds_options const * options = &_options;
    std::string const & stream_name =  // default = empty = device option
        control.nested( topics::control::set_option::key::stream_name ).string_ref_or_empty();
    if( ! stream_name.empty() )
    {
        auto stream_it = _streams.find( stream_name );
        if( stream_it == _streams.end() )
            DDS_THROW(runtime_error, "stream '" + stream_name + "' not found" );
        options = &stream_it->second->options();
    }

    auto value_j = j.nested( topics::reply::set_option::key::value );
    if( ! value_j.exists() )
        DDS_THROW(runtime_error, "missing value" );

    auto option_name_j = control.nested( topics::control::set_option::key::option_name );
    if( ! option_name_j.is_string() )
        DDS_THROW(runtime_error, "missing option-name" );
    auto & option_name = option_name_j.string_ref();
    for( auto & option : *options )
    {
        if( option->get_name() == option_name )
        {
            option->set_value( value_j );  // throws!
            return;
        }
    }
    DDS_THROW(runtime_error, "option '" + option_name + "' not found" );
}


void dds_device::impl::on_query_options( json const & j, dds_sample const & )
{
    if( ! is_ready() )
        return;

    // This is the notification for "query-options", which can get sent as a reply to a control or independently by the
    // device. It takes the same form & handling either way.
    // 
    // E.g.:
    //   {
    //     "id": "query-options",
    //     "option-values" : {
    //       "IP address": "1.2.3.4",  // device-level
    //       "Color": {
    //         "Exposure": 8.0,
    //       },
    //       "Depth" : {
    //         "Exposure": 20.0
    //       }
    //     }
    //   }

    dds_device::check_reply( j );  // throws

    // This little function is used either for device or stream options
    auto update_option = [this]( dds_options const & options, std::string const & option_name, json const & new_value )
    {
        // Find the option and set its value
        for( auto & option : options )
        {
            if( option->get_name() == option_name )
            {
                option->set_value( new_value );
                return;
            }
        }
        //LOG_DEBUG( "[" << debug_name() << "] option '" << option_name << "': not found" );
        throw std::runtime_error( "option '" + option_name + "' not found" );
    };

    auto option_values = j.nested( topics::reply::query_options::key::option_values );
    if( ! option_values.is_object() )
        throw std::runtime_error( "missing option-values" );

    //LOG_DEBUG( "[" << debug_name() << "] got query-options: " << std::setw( 4 ) << option_values );
    for( auto it = option_values.begin(); it != option_values.end(); ++it )
    {
        if( it->is_object() )
        {
            // Stream name
            auto & stream_name = it.key();
            auto stream_it = _streams.find( stream_name );
            if( stream_it == _streams.end() )
                throw std::runtime_error( "stream '" + stream_name + "' not found" );
            auto & option_names = it.value();
            for( auto option_it = option_names.begin(); option_it != option_names.end(); ++option_it )
                update_option( stream_it->second->options(), option_it.key(), option_it.value() );
        }
        else
        {
            // Device-level option name
            update_option( _options, it.key(), it.value() );
        }
    }
}


void dds_device::impl::on_known_notification( json const & j, dds_sample const & )
{
    // This is a known notification, but we don't want to do anything for it
}


void dds_device::impl::on_log( json const & j, dds_sample const & )
{
    // This is the notification for "log"  (see docs/notifications.md#Logging)
    //     - `entries` is an array containing 1 or more log entries
    auto entries = j.nested( topics::notification::log::key::entries );
    if( ! entries )
        throw std::runtime_error( "log entries not found" );
    if( ! entries.is_array() )
        throw std::runtime_error( "log entries not an array" );
    // Each log entry is a JSON array of `[timestamp, type, text, data]` containing:
    //     - `timestamp`: when the event occurred
    //     - `type`: one of `EWID` (Error, Warning, Info, Debug)
    //     - `text`: any text that needs output
    //     - `data`: optional; an object containing any pertinent information about the event
    size_t x = 0;
    for( auto & entry : entries )
    {
        try
        {
            if( ! entry.is_array() )
                throw std::runtime_error( "not an array" );
            if( entry.size() < 3 || entry.size() > 4 )
                throw std::runtime_error( "bad array length" );
            auto timestamp = entry[0].get< dds_nsec >();
            auto const & stype = entry[1].string_ref();
            if( stype.length() != 1 || ! strchr( "EWID", stype[0] ) )
                throw std::runtime_error( "type not one of 'EWID'" );
            char const type = stype[0];
            auto const & text_s = entry[2].string_ref();
            rsutils::string::slice text( text_s );
            if( text.length() && text.back() == '\n' )
                text = { text.begin(), text.end() - 1 };
            auto const & data = entry.size() > 3 ? entry[3] : rsutils::null_json;

            if( ! _on_device_log.raise( timestamp, type, text_s, data ) )
            {
                if( data.is_null() )
                    LOG_DEBUG( "[" << debug_name() << "][" << timestamp << "][" << type << "] " << text );
                else
                    LOG_DEBUG( "[" << debug_name() << "][" << timestamp << "][" << type << "] " << text << " [" << data << "]" );
            }
        }
        catch( std::exception const & e )
        {
            LOG_DEBUG( "log entry " << x << ": " << e.what() << "\n" << entry );
        }
        ++x;
    }
}


void dds_device::impl::add_profiles_to_json( const realdds::dds_stream_profiles & profiles, rsutils::json & profiles_as_json ) const
{
    for( auto & profile : profiles )
    {
        auto stream = profile->stream();
        if( ! stream )
            DDS_THROW( runtime_error, "profile '" << profile->to_string() << "' is not part of any stream" );
        if( profiles_as_json.nested( stream->name() ) )
            DDS_THROW( runtime_error, "more than one profile found for stream '" << stream->name() << "'" );

        profiles_as_json[stream->name()] = profile->to_json();
    }
}

void dds_device::impl::open( const dds_stream_profiles & profiles )
{
    if( profiles.empty() )
        DDS_THROW( runtime_error, "must provide at least one profile" );

    json profiles_to_open;
    add_profiles_to_json( profiles, profiles_to_open );
    // Not needed, already open streams are kept open by FW
    // add_profiles_to_json( _open_profiles_list, profiles_to_open ); // Add already open profiles to the list

    json j = {
        { topics::control::key::id, topics::control::open_streams::id },
        // D555 initial FW treats reset field as implicitly true, so we explicitly mention it here
        { topics::control::open_streams::key::reset, false }
    };
    if( ! profiles_to_open.empty() )
        j[topics::control::open_streams::key::stream_profiles] = std::move( profiles_to_open );

    json reply;
    write_control_message( j, &reply );

    // If no exception writing to the device then save profiles in open profiles list
    _open_profiles_list.insert( _open_profiles_list.end(), profiles.begin(), profiles.end() );
}

void dds_device::impl::close( const dds_stream_profiles & profiles )
{
    // Remove profiles from open profiles list. Not using erase-remove idiom but for a small number of profiles it does not really matter...
    for( auto & profile : profiles )
    {
        auto it = find( _open_profiles_list.begin(), _open_profiles_list.end(), profile );
        if( it != _open_profiles_list.end() )
            _open_profiles_list.erase( it );
    }

    json keep_open_profiles;
    add_profiles_to_json( _open_profiles_list, keep_open_profiles );

    json j = {
        { topics::control::key::id, topics::control::open_streams::id },
        { topics::control::open_streams::key::reset, true }
    };
    if( ! keep_open_profiles.empty() )
        j[topics::control::open_streams::key::stream_profiles] = std::move( keep_open_profiles );

    json reply;
    write_control_message( j, &reply );
}

void dds_device::impl::set_option_value( const std::shared_ptr< dds_option > & option, json new_value )
{
    if( ! option )
        DDS_THROW( runtime_error, "must provide an option to set" );

    json j = json::object({
        { topics::control::key::id, topics::control::set_option::id },
        { topics::control::set_option::key::option_name, option->get_name() },
        { topics::control::set_option::key::value, new_value }
    });
    if( auto stream = option->stream() )
        j[topics::control::set_option::key::stream_name] = stream->name();

    json reply;
    write_control_message( j, &reply );
    // the reply will contain the new value (which may be different) and will update the cached one
}


json dds_device::impl::query_option_value( const std::shared_ptr< dds_option > & option )
{
    if( ! option )
        DDS_THROW( runtime_error, "must provide an option to query" );

    json j = json::object({
        { topics::control::key::id, topics::control::query_option::id },
        { topics::control::query_option::key::option_name, option->get_name() }
    });
    if( auto stream = option->stream() )
        j[topics::control::query_option::key::stream_name] = stream->name();

    json reply;
    write_control_message( j, &reply );

    return reply.at( topics::reply::query_option::key::value );
}

void dds_device::impl::set_embedded_filter(const std::shared_ptr< dds_embedded_filter >& filter, json options_value)
{
    if (!filter)
        DDS_THROW(runtime_error, "must provide an embedded filter to set");

    json j = json::object({
        { topics::control::key::id, topics::control::set_filter::id },
        { topics::control::set_filter::key::name, filter->get_name() },
        { topics::control::set_filter::key::options, options_value }
    });
    if (auto stream = filter->get_stream())
        j[topics::control::set_filter::key::stream_name] = stream->name();

    json reply;
    write_control_message(j, &reply);
    // the reply will contain the new value (which may be different) and will update the cached one
}

json dds_device::impl::query_embedded_filter(const std::shared_ptr< dds_embedded_filter >& filter)
{
    if (!filter)
        DDS_THROW(runtime_error, "must provide an embedded filter to query");

    json j = json::object({
        { topics::control::key::id, topics::control::query_filter::id },
        { topics::control::query_filter::key::name, filter->get_name() }
    });
    if (auto stream = filter->get_stream())
        j[topics::control::query_filter::key::stream_name] = stream->name();

    json reply;
    write_control_message(j, &reply);

    return reply;
}

void dds_device::impl::write_control_message( json const & j, json * reply )
{
    assert( _control_writer != nullptr );
    auto this_sequence_number = topics::flexible_msg( j ).write_to( *_control_writer );
    if( reply )
    {
        std::unique_lock< std::mutex > lock( _replies_mutex );
        auto & actual_reply = _replies[this_sequence_number];  // create it; initialized to null json
        if( ! _replies_cv.wait_for( lock,
                                    std::chrono::milliseconds( _reply_timeout_ms ),
                                    [&]()
                                    {
                                        if( actual_reply.is_null() )
                                            return false;
                                        return true;
                                    } ) )
        {
            DDS_THROW( runtime_error, "timeout waiting for reply #" << this_sequence_number << ": " << j );
        }
        //LOG_DEBUG( "got reply: " << actual_reply );
        *reply = std::move( actual_reply );
        _replies.erase( this_sequence_number );

        // Throw if there's an error
        dds_device::check_reply( *reply );
    }
}

void dds_device::impl::create_notifications_reader()
{
    if( _notifications_reader )
        return;

    auto topic = topics::flexible_msg::create_topic( _participant, _info.topic_root() + topics::NOTIFICATION_TOPIC_NAME );

    // We have some complicated topic structures. In particular, the metadata topic is created on demand while handling
    // other notifications, which doesn't work well (deadlock) if the notification is not called from another thread. So
    // we need the notification handling on another thread:
    _notifications_reader = std::make_shared< dds_topic_reader_thread >( topic, _subscriber );

    dds_topic_reader::qos rqos( eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS );
    // On discovery writer sends a burst of messages, if history is too small we might lose some of them
    // (even if reliable). Setting depth to cover known use-cases plus some spare
    rqos.history().depth = 24;
    rqos.override_from_json( _device_settings.nested( "notification" ) );

    _notifications_reader->on_data_available(
        [&]()
        {
            topics::flexible_msg notification;
            dds_sample sample;
            while( topics::flexible_msg::take_next( *_notifications_reader, &notification, &sample ) )
            {
                if( ! notification.is_valid() )
                    continue;
                auto j = notification.json_data();
                if( j.is_array() )
                {
                    for( unsigned x = 0; x < j.size(); ++x )
                        on_notification( std::move( j[x] ), sample );
                }
                else
                {
                    on_notification( std::move( j ), sample );
                }
            }
        } );

    _notifications_reader->run( rqos );
}

void dds_device::impl::create_metadata_reader()
{
    if( _metadata_reader ) // We can be called multiple times, once per stream
        return;

    auto topic = topics::flexible_msg::create_topic( _participant, _info.topic_root() + topics::METADATA_TOPIC_NAME );
    _metadata_reader = std::make_shared< dds_topic_reader_thread >( topic, _subscriber );
    _metadata_reader->on_data_available(
        [this]()
        {
            topics::flexible_msg message;
            while( topics::flexible_msg::take_next( *_metadata_reader, &message ) )
            {
                if( message.is_valid() && _on_metadata_available.size() )
                {
                    try
                    {
                        auto sptr = std::make_shared< const json >( message.json_data() );
                        _on_metadata_available.raise( sptr );
                    }
                    catch( std::exception const & e )
                    {
                        LOG_DEBUG( "[" << debug_name() << "] metadata exception: " << e.what() );
                    }
                }
            }
        } );

    // NOTE: the metadata thread is only run() when we've reached the READY state
}

void dds_device::impl::create_control_writer()
{
    if( _control_writer )
        return;

    auto topic = topics::flexible_msg::create_topic( _participant, _info.topic_root() + topics::CONTROL_TOPIC_NAME );
    _control_writer = std::make_shared< dds_topic_writer >( topic );
    dds_topic_writer::qos wqos( eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS );
    wqos.history().depth = 10;  // default is 1
    // If our reply timeout is less than the heartbeat period, we could lose the control message!
    // So we set a short heartbeat time at half the reply timeout...
    wqos.reliable_writer_qos().times.heartbeatPeriod = _reply_timeout_ms / 2000.;
    _control_writer->override_qos_from_json( wqos, _device_settings.nested( "control" ) );
    _control_writer->run( wqos );
}


void dds_device::impl::on_device_header( json const & j, dds_sample const & sample )
{
    if( _state != state_t::INITIALIZING )
        return;

    _device_header_received = true;

    // The server GUID is the server's notification writer's GUID -- that way, we can easily associate all notifications
    // with a server.
    eprosima::fastrtps::rtps::iHandle2GUID( _server_guid, sample.publication_handle );

    _n_streams_expected = j.at( topics::notification::device_header::key::n_streams ).get< size_t >();
    LOG_DEBUG( "[" << debug_name() << "] ... " << topics::notification::device_header::id << ": " << _n_streams_expected << " streams expected" );

    if( auto extrinsics_j = j.nested( topics::notification::device_header::key::extrinsics ) )
    {
        for( auto & ex : extrinsics_j )
        {
            std::string const & from_name = ex[0].string_ref();
            std::string const & to_name = ex[1].string_ref();
            //LOG_DEBUG( "[" << debug_name() << "]     ... got extrinsics from " << from_name << " to " << to_name );
            try
            {
                extrinsics extr = extrinsics::from_json( ex[2] );
                _extrinsics_map[std::make_pair( from_name, to_name )] = std::make_shared< extrinsics >( extr );
            }
            catch( std::exception const & e )
            {
                LOG_ERROR( "[" << debug_name() << "] Invalid extrinsics data from " << from_name << " to " << to_name
                               << ". Error: " << e.what() << ", reading" << ex );
            }
        }
    }

    if( all_initialization_data_received() )
        set_state( state_t::READY );
}


void dds_device::impl::on_device_options( json const & j, dds_sample const & sample )
{
    if( _state != state_t::INITIALIZING )
        return;

    _device_options_received = true;

    if( auto options_j = j.nested( topics::notification::device_options::key::options ) )
    {
        LOG_DEBUG( "[" << debug_name() << "] ... " << topics::notification::device_options::id << ": " << options_j.size() << " options received" );

        for( auto & option_json : options_j )
        {
            auto option = dds_option::from_json( option_json );
            _options.push_back( option );
        }
    }

    if( all_initialization_data_received() )
        set_state( state_t::READY );
}


void dds_device::impl::on_stream_header( json const & j, dds_sample const & sample )
{
    if( _state != state_t::INITIALIZING )
        return;

    auto & stream_type = j.at( topics::notification::stream_header::key::type ).string_ref();
    auto & stream_name = j.at( topics::notification::stream_header::key::name ).string_ref();

    if( _stream_header_received.size() >= _n_streams_expected )
        DDS_THROW( runtime_error, "more streams than expected (" << _n_streams_expected << ") received" );

    if( _stream_header_received[stream_name] )
    {
        LOG_WARNING( "[" << debug_name() << "] stream header for stream '" << stream_name << "' already received. Ignoring..." );
        return;
    }

    auto & stream = _streams[stream_name];
    auto & sensor_name = j.at( topics::notification::stream_header::key::sensor_name ).string_ref();
    dds_stream_profiles profiles;

#define TYPE2STREAM( S, P )                                                                                            \
    if( stream_type == #S )                                                                                            \
    {                                                                                                                  \
        for( auto & profile : j[topics::notification::stream_header::key::profiles] )                                  \
            profiles.push_back( dds_stream_profile::from_json< dds_##P##_stream_profile >( profile ) );                \
        stream = std::make_shared< dds_##S##_stream >( stream_name, sensor_name );                                     \
    }                                                                                                                  \
    else

    TYPE2STREAM( depth, video )
    TYPE2STREAM( ir, video )
    TYPE2STREAM( color, video )
    TYPE2STREAM( motion, motion )
    TYPE2STREAM( confidence, video )
    DDS_THROW( runtime_error, "stream '" << stream_name << "' is of unknown type '" << stream_type << "'" );

#undef TYPE2STREAM

    if( j.at( topics::notification::stream_header::key::metadata_enabled ).get< bool >() )
    {
        create_metadata_reader();
        stream->enable_metadata();  // Call before init_profiles
    }

    size_t default_profile_index = j.at( "default-profile-index" ).get< size_t >();
    if( default_profile_index < profiles.size() )
        stream->init_profiles( profiles, default_profile_index );
    else
        DDS_THROW( runtime_error,
                   "stream '" << stream_name << "' default profile index " << default_profile_index
                              << " is out of bounds" );
    if( strcmp( stream->type_string(), stream_type.c_str() ) != 0 )
        DDS_THROW( runtime_error,
                   "failed to instantiate stream type '" << stream_type << "' (instead, got '" << stream->type_string()
                                                         << "')" );
    _stream_header_received[stream_name] = true;
    std::string expected_streams = _n_streams_expected == 0 ? "unknown" : std::to_string( _n_streams_expected );
    LOG_DEBUG( "[" << debug_name() << "] ... stream " << _streams.size() << "/" << expected_streams << " '" << stream_name
                   << "' received with " << profiles.size() << " profiles"
                   << ( stream->metadata_enabled() ? " and metadata" : "" ) );

    // Handle out of order stream-options message
    init_stream_options_if_possible( stream_name, stream );
    init_stream_filters_if_possible( stream_name, stream );
    init_stream_intrinsics_if_possible( stream_name, stream );

    if( all_initialization_data_received() )
        set_state( state_t::READY );
}


void dds_device::impl::on_stream_options( json const & j, dds_sample const & sample )
{
    if( _state != state_t::INITIALIZING )
        return;

    auto & stream_name = j.at( topics::notification::stream_options::key::stream_name ).string_ref();
    auto & stream = _streams[stream_name];
    if( _stream_options_received[stream_name] )
    {
        LOG_WARNING( "[" << debug_name() << "] stream options for stream '" << stream_name << "' already received. Ignoring..." );
        return;
    }

    // Note - stream object is created when handling stream-header message.
    // We try to handle out of order messages so we keep data in dedicated member than test if object exists before accessing it

    size_t num_of_options = 0;
    if( auto options_j = j.nested( topics::notification::stream_options::key::options ) )
    {
        dds_options options;
        for( auto & option_j : options_j )
        {
            try
            {
                //LOG_DEBUG( "[" << debug_name() << "]     ... " << option_j );
                auto option = dds_option::from_json( option_j );
                options.push_back( option );
            }
            catch( std::exception const& e )
            {
                LOG_ERROR( "[" << debug_name() << "] Invalid option for stream '" << stream_name
                               << "'. Error: " << e.what() << ", reading" << option_j );
            }
        }
        num_of_options = options.size();
        _stream_options_for_init[stream_name] = std::move( options );
        init_stream_options_if_possible( stream_name, stream );
    }

    if (auto embedded_filters_j = j.nested(topics::notification::stream_options::key::embedded_filters))
    {
        dds_embedded_filters embedded_filters;
        for (auto& embedded_filter_j : embedded_filters_j)
        {
            try
            {
                auto embedded_filter = dds_embedded_filter::from_json(embedded_filter_j);
                embedded_filters.push_back(embedded_filter);
            }
            catch (std::exception const& e)
            {
                LOG_ERROR( "[" << debug_name() << "] Invalid embedded filter for stream '" << stream_name
                               << "'. Error: " << e.what() << ", reading" << embedded_filter_j );
            }            
        }
        _stream_filters_for_init[stream_name] = std::move( embedded_filters );
        init_stream_filters_if_possible( stream_name, stream );
    }
    
    _stream_intrinsics_for_init[stream_name] = j.nested( topics::notification::stream_options::key::intrinsics );
    init_stream_intrinsics_if_possible( stream_name, stream );

    _stream_options_received[stream_name] = true;
    LOG_DEBUG( "[" << debug_name() << "] ... stream '" << stream_name << "' received " << num_of_options << " options" );

    if( all_initialization_data_received() )
        set_state( state_t::READY );
}

bool dds_device::impl::all_initialization_data_received() const
{
    return _device_header_received && _device_options_received &&
           _stream_header_received.size() == _n_streams_expected &&
           _stream_options_received.size() == _n_streams_expected;
}

void dds_device::impl::init_stream_options_if_possible( const std::string & stream_name,
                                                        std::shared_ptr< realdds::dds_stream > & stream )
{
    auto opt_it = _stream_options_for_init.find( stream_name );
    if( opt_it != _stream_options_for_init.end() )
    {
        if( stream )
        {
            stream->init_options( opt_it->second );
            _stream_options_for_init.erase( opt_it );
        }
    }
}

void dds_device::impl::init_stream_filters_if_possible( const std::string & stream_name,
                                                        std::shared_ptr< realdds::dds_stream > & stream )
{
    auto filter_it = _stream_filters_for_init.find( stream_name );
    if( filter_it != _stream_filters_for_init.end() )
    {
        if( stream )
        {
            stream->init_embedded_filters( std::move( filter_it->second ) );
            _stream_filters_for_init.erase( filter_it );
        }
    }
}

void dds_device::impl::init_stream_intrinsics_if_possible( const std::string & stream_name,
                                                           std::shared_ptr< realdds::dds_stream > & stream )
{
    auto intr_it = _stream_intrinsics_for_init.find( stream_name );
    if( intr_it != _stream_intrinsics_for_init.end() )
    {
        rsutils::json_ref j_int = intr_it->second;
        if( stream && j_int)
        {
            // Logic moved here from on_stream_options because it depends on stream dynamic type
            if( auto video_stream = std::dynamic_pointer_cast< dds_video_stream >( stream ) )
            {
                try
                {
                    std::set< video_intrinsics > intrinsics;
                    if( j_int.is_array() )
                    {
                        // Multiple resolutions are provided, likely from legacy devices from the adapter
                        for( auto & intr : j_int )
                            intrinsics.insert( video_intrinsics::from_json( intr ) );
                    }
                    else
                    {
                        // Single intrinsics that will get scaled
                        intrinsics.insert( video_intrinsics::from_json( j_int ) );
                    }
                    video_stream->set_intrinsics( intrinsics );
                }
                catch( std::exception const & e )
                {
                    LOG_ERROR( "[" << debug_name() << "] Invalid intrinsics for stream '" << stream_name
                                   << "'. Error: " << e.what() << ", reading" << j_int );
                }
            }
            else if( auto motion_stream = std::dynamic_pointer_cast< dds_motion_stream >( stream ) )
            {
                motion_stream->set_accel_intrinsics( motion_intrinsics::from_json(
                    j_int.at( topics::notification::stream_options::intrinsics::key::accel ) ) );
                motion_stream->set_gyro_intrinsics( motion_intrinsics::from_json(
                    j_int.at( topics::notification::stream_options::intrinsics::key::gyro ) ) );
            }
            _stream_intrinsics_for_init.erase( intr_it );
        }
    }
}

void dds_device::impl::on_calibration_changed( json const & j, dds_sample const & sample )
{
    for( auto const & name_stream : _streams )
    {
        auto & stream = name_stream.second;

        auto j_int = j.nested( stream->name(), topics::notification::calibration_changed::key::intrinsics );
        if( ! j_int )
            continue;  // stream isn't updated

        try
        {
            auto video_stream = std::dynamic_pointer_cast< dds_video_stream >( stream );
            if( ! video_stream )
                DDS_THROW( runtime_error, "not a video stream" );

            auto const & old_intrinsics = video_stream->get_intrinsics();
            std::set< video_intrinsics > new_intrinsics;
            if( j_int.is_array() )
            {
                // Multiple resolutions are provided, likely from legacy devices from the adapter
                if( j_int.size() != old_intrinsics.size() )
                    DDS_THROW( runtime_error, "expecting " << old_intrinsics.size() << " intrinsics; got: " << j_int );
                for( auto & ij : j_int )
                {
                    auto i = video_intrinsics::from_json( ij );
                    auto it = old_intrinsics.find( i );  // uses width & height only
                    if( it == old_intrinsics.end() )
                        DDS_THROW( runtime_error, "intrinsics not found: " << ij );
                    if( ! new_intrinsics.insert( std::move( i ) ).second )
                        DDS_THROW( runtime_error, "width & height specified twice: " << ij );
                }
                LOG_DEBUG( "calibration-changed '" << stream->name() << "': changing " << j_int );
            }
            else
            {
                // Single intrinsics that will get scaled
                auto i = *old_intrinsics.begin();
                i.override_from_json( j_int );
                LOG_DEBUG( "calibration-changed '" << stream->name() << "': changing " << j_int << " --> " << i );
                new_intrinsics.insert( std::move( i ) );
            }

            video_stream->set_intrinsics( std::move( new_intrinsics ) );
            _on_calibration_changed.raise( stream );
        }
        catch( std::exception const & e )
        {
            LOG_ERROR( "calibration-changed '" << stream->name() << "': " << e.what() );
        }
    }
}


}  // namespace realdds
