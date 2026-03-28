// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#include <realdds/dds-device-server.h>

#include <realdds/dds-participant.h>
#include <realdds/dds-publisher.h>
#include <realdds/dds-subscriber.h>
#include <realdds/dds-stream-server.h>
#include <realdds/dds-stream-profile.h>
#include <realdds/dds-notification-server.h>
#include <realdds/dds-topic-reader.h>
#include <realdds/dds-device-broadcaster.h>
#include <realdds/dds-utilities.h>
#include <realdds/topics/dds-topic-names.h>
#include <realdds/topics/device-info-msg.h>
#include <realdds/topics/flexible-msg.h>
#include <realdds/dds-topic.h>
#include <realdds/dds-topic-writer.h>
#include <realdds/dds-option.h>
#include <realdds/dds-guid.h>
#include <realdds/dds-sample.h>

#include <rsutils/string/from.h>
#include <rsutils/string/shorten-json-string.h>
#include <rsutils/json.h>
using rsutils::json;
using rsutils::string::slice;
using rsutils::string::shorten_json_string;

using namespace eprosima::fastdds::dds;


namespace realdds {


dds_device_server::dds_device_server( std::shared_ptr< dds_participant > const & participant,
                                      const std::string & topic_root )
    : _publisher( std::make_shared< dds_publisher >( participant ) )
    , _subscriber( std::make_shared< dds_subscriber >( participant ) )
    , _topic_root( topic_root )
    , _control_dispatcher( QUEUE_MAX_SIZE, rsutils::concurrency::thread_category_network, "dds-ctrl-d" )
{
    LOG_DEBUG( "[" << debug_name() << "] device server created" );
    _control_dispatcher.start();
}


dds_guid const & dds_device_server::guid() const
{
    return _notification_server ? _notification_server->guid() : unknown_guid;
}


std::shared_ptr< dds_participant > dds_device_server::participant() const
{
    return _publisher->get_participant();
}


rsutils::string::slice dds_device_server::debug_name() const
{
    return device_name_from_root( _topic_root );
}


dds_device_server::~dds_device_server()
{
    _stream_name_to_server.clear();
    LOG_DEBUG( "[" << debug_name() << "] device server deleted" );
}


static void on_discovery_device_header( size_t const n_streams,
                                        const dds_options & options,
                                        const extrinsics_map & extr,
                                        dds_notification_server & notifications )
{
    auto extrinsics_json = json::array();
    for( auto & ex : extr )
        extrinsics_json.push_back( json::array( { ex.first.first, ex.first.second, ex.second->to_json() } ) );

    json j_device_header{
        { topics::notification::key::id, topics::notification::device_header::id },
        { topics::notification::device_header::key::n_streams, n_streams },
        { topics::notification::device_header::key::extrinsics, std::move( extrinsics_json ) }
    };
    topics::flexible_msg device_header( j_device_header );
    LOG_DEBUG( "device-header " << std::setw( 4 ) << j_device_header << " size " << device_header._data.size() );
    notifications.add_discovery_notification( std::move( device_header ) );

    auto device_options = json::array();
    for( auto & opt : options )
        device_options.push_back( std::move( opt->to_json() ) );
    json j_device_options{ { topics::notification::key::id, topics::notification::device_options::id },
                           { topics::notification::device_options::key::options, std::move( device_options ) } };
    topics::flexible_msg device_options_message( j_device_options );
    LOG_DEBUG( "device-options " << std::setw( 4 ) << j_device_options << " size " << device_options_message._data.size() );
    notifications.add_discovery_notification( std::move( device_options_message ) );
}


static void on_discovery_stream_header( std::shared_ptr< dds_stream_server > const & stream,
                                        dds_notification_server & notifications )
{
    auto profiles = json::array();
    for( auto & sp : stream->profiles() )
        profiles.push_back( std::move( sp->to_json() ) );
    json j_stream_header{
        { topics::notification::key::id, topics::notification::stream_header::id },
        { topics::notification::stream_header::key::type, stream->type_string() },
        { topics::notification::stream_header::key::name, stream->name() },
        { topics::notification::stream_header::key::sensor_name, stream->sensor_name() },
        { topics::notification::stream_header::key::profiles, std::move( profiles ) },
        { topics::notification::stream_header::key::default_profile_index, stream->default_profile_index() },
        { topics::notification::stream_header::key::metadata_enabled, stream->metadata_enabled() },
    };
    topics::flexible_msg stream_header_message( j_stream_header );
    LOG_DEBUG( stream->name() << " stream-header " << std::setw( 4 ) << j_stream_header << " size "
                              << stream_header_message._data.size() );
    notifications.add_discovery_notification( std::move( stream_header_message ) );

    json j_stream_options = json::object( {
        { topics::notification::key::id, topics::notification::stream_options::id },
        { topics::notification::stream_options::key::stream_name, stream->name() },
    } );

    auto & j_options = j_stream_options[topics::notification::stream_options::key::options] = json::array();
    for( auto & opt : stream->options() )
        j_options.push_back( std::move( opt->to_json() ) );

    auto& j_embedded_filters = j_stream_options[topics::notification::stream_options::key::embedded_filters] = json::array();
    for (auto& ef : stream->embedded_filters())
        j_embedded_filters.push_back(std::move(ef->to_json()));

    if( auto video_stream = std::dynamic_pointer_cast< dds_video_stream_server >( stream ) )
    {
        auto & intrinsics = video_stream->get_intrinsics();
        if( intrinsics.empty() )
        {
            // No intrinsics...
        }
        else
        {
            auto & j_intrinsics = j_stream_options[topics::notification::stream_options::key::intrinsics];
            if( 1 == intrinsics.size() )
            {
                // Use an object with a single intrinsic
                j_intrinsics = intrinsics.begin()->to_json();
            }
            else
            {
                // Multiple intrinsics are available
                j_intrinsics = json::array();
                for( auto & i : intrinsics )
                    j_intrinsics.push_back( i.to_json() );
            }
        }
    }
    else if( auto motion_stream = std::dynamic_pointer_cast< dds_motion_stream_server >( stream ) )
    {
        auto & j_intrinsics = j_stream_options[topics::notification::stream_options::key::intrinsics];
        j_intrinsics = json::object( {
            { topics::notification::stream_options::intrinsics::key::accel,
                    motion_stream->get_accel_intrinsics().to_json() },
            { topics::notification::stream_options::intrinsics::key::gyro,
                    motion_stream->get_gyro_intrinsics().to_json() }
        } );
    }

    topics::flexible_msg stream_options_message( j_stream_options );
    LOG_DEBUG( stream->name() << " stream-options " << std::setw( 4 ) << j_stream_options << " size "
                              << stream_options_message._data.size() );
    notifications.add_discovery_notification( std::move( stream_options_message ) );
}


static std::string ros_friendly_topic_name( std::string name )
{
    // Replace all '/' with '_'
    int n = 0;
    for( char & ch : name )
        if( ch == '/' )
            if( ++n > 1 )
                ch = '_';
    // ROS topics start with "rt/" prefix
    name.insert( 0, "rt/", 3 );
    return name;
}


void dds_device_server::init( std::vector< std::shared_ptr< dds_stream_server > > const & streams,
                              const dds_options & options, const extrinsics_map & extr )
{
    if( is_valid() )
        DDS_THROW( runtime_error, "device server '" + _topic_root + "' is already initialized" );

    try
    {
        // Create a notifications server and set discovery notifications
        _notification_server = std::make_shared< dds_notification_server >( _publisher,
                                                                            _topic_root + topics::NOTIFICATION_TOPIC_NAME );

        // If a previous init failed (e.g., one of the streams has no profiles):
        _stream_name_to_server.clear();

        _options = options;
        on_discovery_device_header( streams.size(), options, extr, *_notification_server );
        for( auto & stream : streams )
        {
            std::string topic_name = ros_friendly_topic_name( _topic_root + '/' + stream->name() );
            stream->open( topic_name, _publisher );
            _stream_name_to_server[stream->name()] = stream;
            on_discovery_stream_header( stream, *_notification_server );

            if( stream->metadata_enabled() && ! _metadata_writer )
            {
                auto topic = topics::flexible_msg::create_topic( _publisher->get_participant(),
                                                                 _topic_root + topics::METADATA_TOPIC_NAME );
                _metadata_writer = std::make_shared< dds_topic_writer >( topic, _publisher );
                dds_topic_writer::qos wqos( eprosima::fastdds::dds::BEST_EFFORT_RELIABILITY_QOS );
                wqos.history().depth = 10;  // default is 1
                _metadata_writer->override_qos_from_json( wqos, _subscriber->get_participant()->settings().nested( "device", "metadata" ) );
                _metadata_writer->run( wqos );
            }
        }

        _notification_server->run();

        // Create a control reader and set callback
        if( auto topic = topics::flexible_msg::create_topic( _subscriber->get_participant(), _topic_root + topics::CONTROL_TOPIC_NAME ) )
        {
            _control_reader = std::make_shared< dds_topic_reader >( topic, _subscriber );

            _control_reader->on_data_available( [&]() { on_control_message_received(); } );

            dds_topic_reader::qos rqos( RELIABLE_RELIABILITY_QOS );
            rqos.override_from_json( _subscriber->get_participant()->settings().nested( "device", "control" ) );
            _control_reader->run( rqos );
        }
    }
    catch( std::exception const & )
    {
        _notification_server.reset();
        _stream_name_to_server.clear();
        _control_reader.reset();
        throw;
    }
}


void dds_device_server::broadcast( topics::device_info const & device_info )
{
    if( _broadcaster )
        DDS_THROW( runtime_error, "device server was already broadcast" );
    if( ! _notification_server )
        DDS_THROW( runtime_error, "not initialized" );
    if( device_info.topic_root() != _topic_root )
        DDS_THROW( runtime_error, "device-info topic root does not match" );
    _broadcaster = std::make_shared< dds_device_broadcaster >(
        _publisher,
        device_info,
        [weak_notification_server = std::weak_ptr< dds_notification_server >( _notification_server )]
        {
            // Once we know our broadcast was acknowledged, send out discovery notifications again so any client who had
            // us marked offline can get ready again
            if( auto notification_server = weak_notification_server.lock() )
                notification_server->trigger_discovery_notifications();
        } );
}


bool dds_device_server::broadcast_disconnect( dds_time ack_timeout )
{
    bool got_acks = false;
    if( _broadcaster )
    {
        got_acks = _broadcaster->broadcast_disconnect( ack_timeout );
        _broadcaster.reset();
    }
    return got_acks;
}


void dds_device_server::publish_notification( topics::flexible_msg && notification )
{
    _notification_server->send_notification( std::move( notification ) );
}


void dds_device_server::publish_metadata( json && md )
{
    if( ! _metadata_writer )
        DDS_THROW( runtime_error, "device '" + _topic_root + "' has no stream with enabled metadata" );

    topics::flexible_msg msg( md );
    LOG_DEBUG(
        "publishing metadata: " << shorten_json_string( slice( msg.custom_data< char const >(), msg._data.size() ),
                                                        300 ) );
    std::move( msg ).write_to( *_metadata_writer );
}


bool dds_device_server::has_metadata_readers() const
{
    return _metadata_writer && _metadata_writer->has_readers();
}


struct dds_device_server::control_sample
{
    rsutils::json const json;
    dds_sample const sample;
};


void dds_device_server::on_control_message_received()
{
    typedef std::map< std::string, void ( dds_device_server::* )( control_sample const &, json & ) >
        control_handlers;
    static control_handlers const _control_handlers{
        { topics::control::set_option::id, &dds_device_server::on_set_option },
        { topics::control::query_option::id, &dds_device_server::on_query_option },
        { topics::control::query_options::id, &dds_device_server::on_query_options },
        { topics::control::query_filter::id, &dds_device_server::on_query_filter }
        // These are the ones handle internally -- our owner may handle application-dependent controls
    };

    topics::flexible_msg data;
    dds_sample sample;
    while( topics::flexible_msg::take_next( *_control_reader, &data, &sample ) )
    {
        if( ! data.is_valid() )
            continue;

        _control_dispatcher.invoke(
            [control = control_sample{ data.json_data(), sample }, this]( dispatcher::cancellable_timer )
            {
                auto sample_j = json::array( {
                    rsutils::string::from( realdds::print_raw_guid( control.sample.sample_identity.writer_guid() ) ),
                    control.sample.sample_identity.sequence_number().to64long(),
                } );
                LOG_DEBUG( "[" << debug_name() << "] <----- control " << sample_j << ": " << control.json );
                json reply;
                reply[topics::reply::key::sample] = std::move( sample_j );
                try
                {
                    std::string const & id = control.json.at( topics::control::key::id ).string_ref();  // throws
                    //reply[topics::reply::key::id] = id;  // Not needed: included with the control
                    reply[topics::reply::key::control] = control.json;

                    auto it = _control_handlers.find( id );
                    if( it != _control_handlers.end() )
                    {
                        ( this->*( it->second ) )( control, reply );
                    }
                    else if( ! _control_callback || ! _control_callback( id, control.json, reply ) )
                    {
                        DDS_THROW( runtime_error, "invalid control" );
                    }
                }
                catch( std::exception const & e )
                {
                    reply[topics::reply::key::status] = "error";
                    reply[topics::reply::key::explanation] = e.what();
                }
                LOG_DEBUG( "[" << debug_name() << "] ----->   reply " << reply );
                try
                {
                    publish_notification( reply );
                }
                catch( ... )
                {
                    LOG_ERROR( "failed to send reply" );
                }
            } );
    }
}


void dds_device_server::on_set_option( control_sample const & control, json & reply )
{
    auto & option_name  // mandatory; throws
        = control.json.at( topics::control::set_option::key::option_name ).string_ref();
    std::string const & stream_name  // default is empty, for a device option
        = control.json.nested( topics::control::set_option::key::stream_name ).string_ref_or_empty();

    std::shared_ptr< dds_option > option = get_option( option_name, stream_name );

    json value = control.json.at( topics::control::set_option::key::value );  // mandatory; throws
    if( _set_option_callback )
        _set_option_callback( option, value );  // Let our owner have final say
    // Ensure realdds option is up to date with actual value from callback
    option->set_value( std::move( value ) );
    reply[topics::reply::set_option::key::value] = option->get_value();
}


json dds_device_server::query_option( std::shared_ptr< dds_option > const & option ) const
{
    json value;
    if( _query_option_callback )
    {
        value = _query_option_callback( option );
        // Ensure realdds option is up to date with actual value from callback
        option->set_value( value );
    }
    else
    {
        value = option->get_value();
    }
    return value;
}


void dds_device_server::on_query_option( control_sample const & control, json & reply )
{
    std::string const & stream_name  // optional; empty = device option
        = control.json.nested( topics::control::query_option::key::stream_name ).string_ref_or_empty();
    std::string const & option_name  // mandatory
        = control.json.nested( topics::control::query_option::key::option_name ).string_ref_or_empty();

    // We return a single "value" value in the reply
    std::shared_ptr< dds_option > option = get_option( option_name, stream_name );
    reply[topics::reply::query_option::key::value] = query_option( option );
}


void dds_device_server::on_query_options( control_sample const & control, json & reply )
{
    // We return a stream->option->value mapping in the reply "option-values"
    json & option_values = reply[topics::reply::query_options::key::option_values] = json::object();

    auto fill_option_values = [this]( dds_options const & options, json & values )
    {
        for( auto const & option : options )
            values[option->get_name()] = query_option( option );
    };

    if( auto stream_name_j = control.json.nested( topics::control::query_options::key::stream_name ) )
    {
        if( ! stream_name_j.is_string() )
            DDS_THROW( runtime_error, "stream-name expected as a string" );

        auto & stream_name = stream_name_j.string_ref();
        if( stream_name.empty() )
        {
            // Just want device options, embedded directly in option-values
            fill_option_values( _options, option_values );
        }
        else
        {
            auto it = _stream_name_to_server.find( stream_name );
            if( it == _stream_name_to_server.end() )
                DDS_THROW( runtime_error, "stream '" + stream_name + "' not found" );
            fill_option_values( it->second->options(), option_values[stream_name] );
        }
    }
    else if( auto sensor_name_j = control.json.nested( topics::control::query_options::key::sensor_name ) )
    {
        if( ! sensor_name_j.is_string() )
            DDS_THROW( runtime_error, "sensor-name expected as a string" );

        for( auto & stream_server : _stream_name_to_server )
        {
            auto stream = stream_server.second;
            if( stream->sensor_name() == sensor_name_j.string_ref() )
                fill_option_values( stream->options(), option_values[stream->name()] );
        }
    }
    else
    {
        // User asked for ALL options (device & all streams)
        fill_option_values( _options, option_values );
        for( auto & stream_server : _stream_name_to_server )
        {
            auto stream = stream_server.second;
            fill_option_values( stream->options(), option_values[stream->name()] );
        }
    }
}


std::shared_ptr< dds_option > dds_device_server::find_option( const std::string & option_name,
                                                              const std::string & stream_name ) const
{
    if( stream_name.empty() )
    {
        for( auto & option : _options )
        {
            if( option->get_name() == option_name )
                return option;
        }
    }
    else
    {
        // Find option in owner stream
        auto stream_it = _stream_name_to_server.find( stream_name );
        if( stream_it != _stream_name_to_server.end() )
        {
            for( auto & option : stream_it->second->options() )
            {
                if( option->get_name() == option_name )
                    return option;
            }
        }
    }

    return {};
}


std::shared_ptr< dds_option > dds_device_server::get_option( std::string const & option_name,
                                                             std::string const & stream_name ) const
{
    std::shared_ptr< dds_option > option = find_option( option_name, stream_name );
    if( option )
        return option;

    std::string which;
    if( stream_name.empty() )
        which = "device";
    else
        which = "'" + stream_name + "'";
    DDS_THROW( runtime_error, which + " option '" + option_name + "' not found" );
}

void dds_device_server::on_query_filter(control_sample const& control, rsutils::json& reply)
{
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

    json& filter_options_values = reply[topics::reply::query_filter::key::options] = json::object();

    auto fill_filter_options_values = [this](dds_options const& options, json& values)
        {
            for (auto const& option : options)
                values[option->get_name()] = query_option(option);
        };
    // assigning filter name and stream name in reply, from control message
    auto& filter_name = reply[topics::control::query_filter::key::name];
    filter_name = control.json.nested(topics::control::query_filter::key::name);
    auto& stream_name = reply[topics::control::query_filter::key::stream_name];
    stream_name = control.json.nested(topics::control::query_filter::key::stream_name);

    auto filter_name_str = filter_name.string_ref();
    auto stream_name_str = stream_name.string_ref();
    
    for (auto& stream_server : _stream_name_to_server)
    {
        if (stream_server.first == stream_name_str)
        {
            for (auto& filter : stream_server.second->embedded_filters())
            {
                if (filter->get_name() == filter_name_str)
                {
                    fill_filter_options_values(filter->get_options(), filter_options_values);
                    return;
                }
            }
        }
    }
    throw std::runtime_error("Embedded filter '" + filter_name_str + "' not found within stream: " + stream_name_str);
}

}  // namespace realdds