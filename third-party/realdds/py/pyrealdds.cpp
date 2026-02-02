// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#include <realdds/dds-defines.h>
#include <realdds/dds-participant.h>
#include <realdds/topics/device-info-msg.h>
#include <realdds/topics/flexible-msg.h>
#include <realdds/topics/flexible/flexiblePubSubTypes.h>
#include <realdds/topics/compressed-image-msg.h>
#include <realdds/topics/image-msg.h>
#include <realdds/topics/imu-msg.h>
#include <realdds/topics/blob-msg.h>
#include <realdds/topics/ros2/participant-entities-info-msg.h>
#include <realdds/topics/blob/blobPubSubTypes.h>
#include <realdds/topics/ros2/sensor_msgs/msg/CompressedImagePubSubTypes.h>
#include <realdds/topics/ros2/sensor_msgs/msg/ImagePubSubTypes.h>
#include <realdds/topics/ros2/sensor_msgs/msg/ImuPubSubTypes.h>
#include <realdds/topics/ros2/rmw_dds_common/msg/ParticipantEntitiesInfoPubSubTypes.h>
#include <realdds/topics/dds-topic-names.h>
#include <realdds/dds-device-broadcaster.h>
#include <realdds/dds-device-server.h>
#include <realdds/dds-stream-server.h>
#include <realdds/dds-stream-profile.h>
#include <realdds/dds-device-watcher.h>
#include <realdds/dds-device.h>
#include <realdds/dds-stream.h>
#include <realdds/dds-guid.h>
#include <realdds/dds-time.h>
#include <realdds/dds-topic.h>
#include <realdds/dds-topic-reader.h>
#include <realdds/dds-topic-writer.h>
#include <realdds/dds-publisher.h>
#include <realdds/dds-subscriber.h>
#include <realdds/dds-log-consumer.h>
#include <realdds/dds-stream-sensor-bridge.h>
#include <realdds/dds-metadata-syncer.h>
#include <realdds/dds-serialization.h>
#include <realdds/dds-sample.h>
#include <realdds/dds-network-adapter-watcher.h>

#include <rsutils/os/special-folder.h>
#include <rsutils/os/executable-name.h>
#include <rsutils/easylogging/easyloggingpp.h>
#include <rsutils/number/crc32.h>
#include <rsutils/string/from.h>
#include <rsutils/string/nocase.h>
#include <rsutils/json.h>
#include <rsutils/json-config.h>

#include <fastdds/dds/domain/qos/DomainParticipantQos.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastrtps/types/DynamicType.h>
#include <fastdds/rtps/builtin/data/WriterProxyData.h>
#include <fastdds/rtps/builtin/data/ReaderProxyData.h>

#include <rsutils/py/pybind11.h>

using rsutils::json;


#define NAME pyrealdds
#define SNAME "pyrealdds"


namespace {

py::list get_vector3( geometry_msgs::msg::Vector3 const & v )
{
    py::list obj( 3 );
    obj[0] = py::float_( v.x() );
    obj[1] = py::float_( v.y() );
    obj[2] = py::float_( v.z() );
    return std::move( obj );
}


void set_vector3( geometry_msgs::msg::Vector3 & v, std::array< double, 3 > const & l )
{
    v.x( l[0] );
    v.y( l[1] );
    v.z( l[2] );
}


std::string script_name()
{
    // Returns the name of the python script that's currently running us
    return rsutils::os::base_name(
        py::module_::import( "__main__" ).attr( "__file__" ).cast< std::string >() );
}


json load_rs_settings( json const & local_settings )
{
    // Load the realsense configuration file settings
    std::string const filename = rsutils::os::get_special_folder( rsutils::os::special_folder::app_data ) + "realsense-config.json";
    auto config = rsutils::json_config::load_from_file( filename );

    // Load "python"-specific settings
    json settings = rsutils::json_config::load_app_settings( config, "python", "context", "config-file" );

    // Take the "dds" settings only
    settings = settings.nested( "dds" );

    // Patch any script-specific settings
    // NOTE: this is also accessed by pyrealsense2, where a "context" hierarchy is still used
    try
    {
        auto script = script_name();
        if( auto script_settings = config.nested( script, "context", "dds" ) )
            settings.override( script_settings, "config-file/" + script + "/context" );
    }
    catch( std::exception const & e )
    {
        // Expected if we're not in a script
        LOG_DEBUG( "failed to get script name: " << e.what() );
    }

    // We should always have DDS enabled
    if( settings.is_object() )
        settings.erase( "enabled" );

    // Patch the given local settings into the configuration
    settings.override( local_settings, "local settings" );

    return settings;
}


py::list network_adapter_list()
{
    auto const ips = realdds::dds_network_adapter_watcher::current_ips();
    py::list obj( ips.size() );
    int i = 0;
    for( auto & ip : ips )
        obj[i++] = ip;
    return std::move( obj );
}


}  // namespace


PYBIND11_MODULE(NAME, m) {
    m.doc() = R"pbdoc(
        RealSense DDS Server Python Bindings
    )pbdoc";
    m.attr( "__version__" ) = "0.1";  // RS2_API_VERSION_STR;

    // Configure the same logger as librealsense, and default to only errors by default...
    rsutils::configure_elpp_logger();
    // And set the DDS logger similarly
    eprosima::fastdds::dds::Log::ClearConsumers();
    eprosima::fastdds::dds::Log::RegisterConsumer( realdds::log_consumer::create() );
    eprosima::fastdds::dds::Log::SetVerbosity( eprosima::fastdds::dds::Log::Error );

    m.def( "debug",
           &rsutils::configure_elpp_logger,
           py::arg( "enable" ),
           py::arg( "nested-string" ) = "",
           py::arg( "logger" ) = LIBREALSENSE_ELPP_ID );

    m.def(
        "json_dump",  // pretty print, using our own output; also available in pyrsutils - here for convenience
        []( rsutils::json const & j, size_t indent )
        {
            std::ostringstream os;
            if( indent )
                os << std::setw( indent );
            os << j;
            return os.str();
        },
        py::arg( "json" ),
        py::arg( "indent" ) = 4 );

    using realdds::dds_guid;
    py::class_< dds_guid >( m, "guid" )
        .def( py::init<>() )
        .def_static( "from_string",
                     []( std::string const & raw_guid ) { return realdds::guid_from_string( raw_guid ); } )
        .def( "__bool__", []( dds_guid const & self ) { return self != realdds::unknown_guid; } )
        .def( "__str__",
              []( dds_guid const & self )
              { return rsutils::string::from( realdds::print_guid( self, realdds::unknown_guid, true ) ).str(); } )
        .def( "__repr__",
              []( dds_guid const & self ) { return rsutils::string::from( realdds::print_raw_guid( self ) ).str(); } )
        // Following two (hash and ==) are needed if we want to be able to use guids as dictionary keys
        .def( "__hash__",
              []( dds_guid const & self )
              {
                  return std::hash< std::string >{}(
                      rsutils::string::from( realdds::print_raw_guid( self ) ) );
              } )
        .def( py::self == py::self );

    using realdds::dds_participant;
    using eprosima::fastrtps::types::ReturnCode_t;


    using realdds::dds_time;
    using realdds::dds_nsec;
    py::class_< dds_time >( m, "time" )
        .def( py::init<>() )
        .def( py::init< int32_t, uint32_t >() )  // sec, nsec
        .def( py::init< long double >() )        // inexact (1.001 -> 1.000999999)
        .def( py::init( []( dds_nsec ns ) { return realdds::time_from( ns ); } ) )           // exact
        .def_readwrite( "seconds", &dds_time::seconds )
        .def_readwrite( "nanosec", &dds_time::nanosec )
        .def( "to_ns", &dds_time::to_ns )
        .def_static( "from_ns", []( dds_nsec ns ) { return realdds::time_from( ns ); } )
        .def_static( "from_double", []( long double d ) { return realdds::dds_time( d ); } )
        .def( "to_double", py::overload_cast< dds_time const & >( &realdds::time_to_double ) )
        .def( "__repr__", []( dds_time const & self ) -> std::string { return rsutils::string::from( realdds::time_to_string( self ) ); } )
        .def( pybind11::self == pybind11::self )
        .def( pybind11::self != pybind11::self );


    // We need a timestamp function that returns timestamps in the same domain as the sample-info timestamps
    using realdds::timestr;
    m.def( "now", []() { return realdds::now(); } );

    py::enum_< timestr::no_suffix_t >( m, "no_suffix_t" );
    m.attr( "no_suffix" ) = timestr::no_suffix;
    py::enum_< timestr::rel_t >( m, "rel_t" );
    m.attr( "rel" ) = timestr::rel;
    py::enum_< timestr::abs_t >( m, "abs_t" );
    m.attr( "abs" ) = timestr::abs;

    m.def( "timestr", []( dds_nsec t ) { return timestr( t ).to_string(); } );
    m.def( "timestr", []( dds_nsec t, timestr::no_suffix_t ) { return timestr( t, timestr::no_suffix ).to_string(); } );
    m.def( "timestr", []( dds_nsec dt, timestr::rel_t ) { return timestr( dt, timestr::rel ).to_string(); } );
    m.def( "timestr", []( dds_nsec dt, timestr::rel_t, timestr::no_suffix_t ) { return timestr( dt, timestr::rel, timestr::no_suffix ).to_string(); } );
    m.def( "timestr", []( dds_nsec t1, dds_nsec t2 ) { return timestr( t1, t2 ).to_string(); } );
    m.def( "timestr", []( dds_nsec t1, dds_nsec t2, timestr::no_suffix_t ) { return timestr( t1, t2, timestr::no_suffix ).to_string(); } );

    m.def( "timestr", []( dds_time t, dds_time start, timestr::no_suffix_t ) { return timestr( t, start, timestr::no_suffix ).to_string(); } );
    m.def( "timestr", []( dds_time t, dds_nsec start, timestr::no_suffix_t ) { return timestr( t, start, timestr::no_suffix ).to_string(); } );
    m.def( "timestr", []( dds_nsec t, dds_time start, timestr::no_suffix_t ) { return timestr( t, start, timestr::no_suffix ).to_string(); } );
    m.def( "timestr", []( dds_time t, timestr::no_suffix_t ) { return timestr( t, timestr::no_suffix ).to_string(); } );

    m.def( "timestr", []( dds_time t, dds_time start ) { return timestr( t, start ).to_string(); } );
    m.def( "timestr", []( dds_time t, dds_nsec start ) { return timestr( t, start ).to_string(); } );
    m.def( "timestr", []( dds_nsec t, dds_time start ) { return timestr( t, start ).to_string(); } );
    m.def( "timestr", []( dds_time t ) { return timestr( t ).to_string(); } );


    m.def( "network_adapter_list", &network_adapter_list );


    py::class_< dds_participant::listener,
                std::shared_ptr< dds_participant::listener >  // handled with a shared_ptr
                >
        listener( m, "participant.listener" );
    listener  // no ctor: use participant.create_listener()
        .def( FN_FWD( dds_participant::listener,
                      on_writer_added,
                      ( dds_guid guid, char const * topic_name ),
                      ( eprosima::fastrtps::rtps::WriterProxyData const & data ),
                      callback( data.guid(), data.topicName().c_str() ); ) )
        .def( FN_FWD( dds_participant::listener,
                      on_writer_removed,
                      ( dds_guid guid, char const * topic_name ),
                      ( dds_guid guid, char const * topic_name ),
                      callback( guid, topic_name ); ) )
        .def( FN_FWD( dds_participant::listener,
                      on_reader_added,
                      ( dds_guid guid, char const * topic_name ),
                      ( eprosima::fastrtps::rtps::ReaderProxyData const & data ),
                      callback( data.guid(), data.topicName().c_str() ); ) )
        .def( FN_FWD( dds_participant::listener,
                      on_reader_removed,
                      ( dds_guid guid, char const * topic_name ),
                      ( dds_guid guid, char const * topic_name ),
                      callback( guid, topic_name ); ) )
        .def( FN_FWD( dds_participant::listener,
                      on_participant_added,
                      ( dds_guid guid, char const * name ),
                      ( dds_guid guid, char const * name ),
                      callback( guid, name ); ) )
        .def( FN_FWD( dds_participant::listener,
                      on_participant_removed,
                      ( dds_guid guid, char const * name ),
                      ( dds_guid guid, char const * name ),
                      callback( guid, name ); ) )
        .def( FN_FWD( dds_participant::listener,
                      on_type_discovery,
                      ( std::string const & topic_name, std::string const & type_name ),
                      ( char const * topic_name, eprosima::fastrtps::types::DynamicType_ptr dyn_type ),
                      callback( topic_name, dyn_type->get_name() ); ) );

    m.def( "load_rs_settings", &load_rs_settings, "local-settings"_a = json::object() );
    m.def( "script_name", &script_name );

    py::class_< dds_participant,
                std::shared_ptr< dds_participant >  // handled with a shared_ptr
                >
        participant( m, "participant" );
    participant.def( py::init<>() )
        .def( "init",
              []( dds_participant & self, json const & local_settings, realdds::dds_domain_id domain_id )
                    { self.init( domain_id, script_name(), local_settings ); },
              "local-settings"_a = json::object(), "domain-id"_a = -1 )
        .def( "init",
              py::overload_cast< realdds::dds_domain_id, std::string const &, json const & >( &dds_participant::init ),
              "domain-id"_a, "participant-name"_a, "local-settings"_a = json::object() )
        .def( "is_valid", &dds_participant::is_valid )
        .def( "guid", &dds_participant::guid )
        .def( "create_guid", &dds_participant::create_guid )
        .def( "__bool__", &dds_participant::is_valid )
        .def( "name", &dds_participant::name )
        .def_static( "name_from_guid", []( dds_guid const & guid ) { return dds_participant::name_from_guid( guid ); } )
        .def( "names", []( dds_participant const & self ) { return self.get()->get_participant_names(); } )
        .def( "settings", &dds_participant::settings )
        .def( "__repr__",
              []( const dds_participant & self ) {
                  std::ostringstream os;
                  os << "<" SNAME ".participant";
                  if( ! self.is_valid() )
                  {
                      os << " NULL";
                  }
                  else
                  {
                      os << " \"" << self.name() << "\"";
                      os << " " << realdds::print_guid( self.guid() );
                  }
                  os << ">";
                  return os.str();
              } )
        .def( "create_listener", []( dds_participant & self ) { return self.create_listener(); } );

    // The 'message' submodule will contain all the known message types in realdds.
    auto message = m.def_submodule( "message", "all the messages that " SNAME " can work with" );

    // The 'topics' submodule will contain pointers to the topic names:
    auto topics = m.def_submodule( "topics", "all the topics that " SNAME " knows" );
    topics.attr( "separator" ) = realdds::topics::SEPARATOR;
    topics.attr( "root" ) = realdds::topics::ROOT;
    topics.attr( "device_info" ) = realdds::topics::DEVICE_INFO_TOPIC_NAME;
    topics.attr( "device_notification" ) = realdds::topics::NOTIFICATION_TOPIC_NAME;
    topics.attr( "device_control" ) = realdds::topics::CONTROL_TOPIC_NAME;
    topics.attr( "device_metadata" ) = realdds::topics::METADATA_TOPIC_NAME;
    topics.attr( "device_dfu" ) = realdds::topics::DFU_TOPIC_NAME;

    auto ros2_topics = topics.def_submodule( "ros2", "all the ROS2 topics" );
    ros2_topics.attr( "discovery_info" ) = realdds::topics::ros2::DISCOVERY_INFO;

    using realdds::distortion_model;
    py::enum_< distortion_model >( m, "distortion_model" )
        .value( "none", distortion_model::none )
        .value( "brown", distortion_model::brown )
        .value( "inverse_brown", distortion_model::inverse_brown )
        .value( "modified_brown", distortion_model::modified_brown );

    using realdds::distortion_parameters;
    py::class_< distortion_parameters >( m, "distortion_parameters" )
        .def( py::init<>() )
        .def_readwrite( "model", &distortion_parameters::model )
        .def_readwrite( "coeffs", &distortion_parameters::coeffs )
        .def( "__repr__",
              []( distortion_parameters const & self ) -> std::string
              { return rsutils::string::from() << "<" SNAME ".distortion_parameters " << self << ">"; } );

    py::class_< rsutils::number::float2 >( m, "float2" )
        .def( py::init<>() )
        .def( py::init< float, float >() )
        .def_readwrite( "x", &rsutils::number::float2::x )
        .def_readwrite( "y", &rsutils::number::float2::y )
        .def( "length", &rsutils::number::float2::length )
        .def( "normalized", &rsutils::number::float2::normalized )
        .def( "__repr__",
              []( rsutils::number::float2 const & self ) -> std::string
              { return rsutils::string::from() << self.x << ',' << self.y; } );

    using realdds::video_intrinsics;
    py::class_< video_intrinsics, std::shared_ptr< video_intrinsics > >( m, "video_intrinsics" )
        .def( py::init<>() )
        .def_readwrite( "width", &video_intrinsics::width )
        .def_readwrite( "height", &video_intrinsics::height )
        .def_readwrite( "principal_point", &video_intrinsics::principal_point )
        .def_readwrite( "focal_length", &video_intrinsics::focal_length )
        .def_readwrite( "distortion", &video_intrinsics::distortion )
        .def( "__repr__",
              []( video_intrinsics const & self ) -> std::string
              { return rsutils::string::from() << "<" SNAME ".video_intrinsics " << self << ">"; } )
        .def( "to_json", &video_intrinsics::to_json )
        .def( "scaled_to", &video_intrinsics::scaled_to )
        .def( "is_valid", &video_intrinsics::is_valid );

    using realdds::motion_intrinsics;
    py::class_< motion_intrinsics, std::shared_ptr< motion_intrinsics > >( m, "motion_intrinsics" )
        .def( py::init<>() )
        .def_readwrite( "data", &motion_intrinsics::data )
        .def_readwrite( "noise_variances", &motion_intrinsics::noise_variances )
        .def_readwrite( "bias_variances", &motion_intrinsics::bias_variances );

    using realdds::extrinsics;
    py::class_< extrinsics, std::shared_ptr< extrinsics > >( m, "extrinsics" )
        .def( py::init<>() )
        .def_readwrite( "rotation", &extrinsics::rotation )
        .def_readwrite( "translation", &extrinsics::translation );

    using realdds::dds_topic;
    py::class_< dds_topic, std::shared_ptr< dds_topic > >( m, "topic" )
        .def( "get_participant", &dds_topic::get_participant )
        .def( "name", []( dds_topic const & self ) { return self->get_name(); } )
        .def( "__repr__", []( dds_topic const & self ) {
            std::ostringstream os;
            os << "<" SNAME ".topic \"" << self->get_name() << "\"";
            os << ">";
            return os.str();
        } );

    using durability = eprosima::fastdds::dds::DurabilityQosPolicyKind;
    py::enum_< durability >( m, "durability" )
        .value( "volatile", eprosima::fastdds::dds::DurabilityQosPolicyKind::VOLATILE_DURABILITY_QOS )
        .value( "transient_local", eprosima::fastdds::dds::DurabilityQosPolicyKind::TRANSIENT_LOCAL_DURABILITY_QOS )
        .value( "transient", eprosima::fastdds::dds::DurabilityQosPolicyKind::TRANSIENT_DURABILITY_QOS );
    using reliability = eprosima::fastdds::dds::ReliabilityQosPolicyKind;
    py::enum_< reliability >( m, "reliability" )
        .value( "reliable", eprosima::fastdds::dds::ReliabilityQosPolicyKind::RELIABLE_RELIABILITY_QOS )
        .value( "best_effort", eprosima::fastdds::dds::ReliabilityQosPolicyKind::BEST_EFFORT_RELIABILITY_QOS );
    using history_kind = eprosima::fastdds::dds::HistoryQosPolicyKind;
    py::enum_< history_kind >( m, "history_kind" )
        .value( "keep_last", eprosima::fastdds::dds::HistoryQosPolicyKind::KEEP_LAST_HISTORY_QOS )
        .value( "keep_all", eprosima::fastdds::dds::HistoryQosPolicyKind::KEEP_ALL_HISTORY_QOS );
    using history_policy = eprosima::fastdds::dds::HistoryQosPolicy;
    py::class_< history_policy >( m, "history_policy" )  //
        .def_readwrite( "kind", &history_policy::kind )
        .def_readwrite( "depth", &history_policy::depth )
        .def( "__repr__",
              []( history_policy const & self )
              {
                  std::ostringstream os;
                  os << "[";
                  switch( self.kind )
                  {
                  case history_kind::KEEP_ALL_HISTORY_QOS:
                      os << "all";
                      break;
                  case history_kind::KEEP_LAST_HISTORY_QOS:
                      os << "last";
                      break;
                  default:
                      os << (int)self.kind;
                      break;
                  }
                  os << ' ';
                  os << self.depth;
                  os << "]";
                  return os.str();
              } );

    using reader_qos = realdds::dds_topic_reader::qos;
    py::class_< reader_qos >( m, "reader_qos" )  //
        .def_property( "history",
            []( reader_qos & self ) -> history_policy & { return self.history(); },
            []( reader_qos & self, eprosima::fastdds::dds::HistoryQosPolicy const & policy ) { self.history( policy ); } )
        .def( "__repr__", []( reader_qos const & self ) {
            std::ostringstream os;
            os << "<" SNAME ".reader_qos";
            os << ">";
            return os.str();
        } );

    using realdds::dds_topic_reader;
    py::class_< dds_topic_reader, std::shared_ptr< dds_topic_reader > >( m, "topic_reader" )
        .def( py::init< std::shared_ptr< dds_topic > const & >() )
        .def( FN_FWD( dds_topic_reader, on_data_available, (dds_topic_reader *), (), callback( self ); ) )
        .def( FN_FWD( dds_topic_reader,
                      on_subscription_matched,
                      (dds_topic_reader *, int),
                      ( eprosima::fastdds::dds::SubscriptionMatchedStatus const & status ),
                      callback( self, status.current_count_change ); ) )
        .def( FN_FWD( dds_topic_reader,
                      on_sample_lost,
                      (dds_topic_reader *, int, int),
                      (eprosima::fastdds::dds::SampleLostStatus const & status),
                      callback( self, status.total_count, status.total_count_change ); ) )
        .def( "topic", &dds_topic_reader::topic )
        .def( "run", &dds_topic_reader::run )
        .def( "wait_for_writers", &dds_topic_reader::wait_for_writers )
        .def( "stop", &dds_topic_reader::stop, 
              // GIL release needed: stop will wait and cause a hang if we're inside a callback
              py::call_guard< py::gil_scoped_release >() )
        .def( "qos", []() { return reader_qos(); } )
        .def( "qos", []( reliability r, durability d ) { return reader_qos( r, d ); } );

    using writer_qos = realdds::dds_topic_writer::qos;
    py::class_< writer_qos >( m, "writer_qos" )  //
        .def_property_readonly( "flow_controller",
                                []( writer_qos const & self ) -> std::string
                                { return self.publish_mode().flow_controller_name; } )
        .def( "__repr__", []( writer_qos const & self ) {
            std::ostringstream os;
            os << "<" SNAME ".writer_qos";
            os << self;
            os << ">";
            return os.str();
        } );

    using realdds::dds_publisher;
    py::class_< dds_publisher, std::shared_ptr< dds_publisher > >( m, "publisher" )
        .def( py::init< std::shared_ptr< dds_participant > const & >() );

    using realdds::dds_subscriber;
    py::class_< dds_subscriber, std::shared_ptr< dds_subscriber > >( m, "subscriber" )
        .def( py::init< std::shared_ptr< dds_participant > const & >() );

    using realdds::dds_topic_writer;
    py::class_< dds_topic_writer, std::shared_ptr< dds_topic_writer > >( m, "topic_writer" )
        .def( py::init< std::shared_ptr< dds_topic > const & >() )
        .def( "guid", &dds_topic_writer::guid )
        .def( FN_FWD( dds_topic_writer,
                      on_publication_matched,
                      (dds_topic_writer *, int),
                      ( eprosima::fastdds::dds::PublicationMatchedStatus const & status ),
                      callback( self, status.current_count_change ); ) )
        .def( "topic", &dds_topic_writer::topic )
        .def( "override_qos_from_json", &dds_topic_writer::override_qos_from_json )
        .def( "run", &dds_topic_writer::run )
        .def( "has_readers", &dds_topic_writer::has_readers )
        .def( "wait_for_readers", &dds_topic_writer::wait_for_readers )
        .def( "wait_for_acks", &dds_topic_writer::wait_for_acks )
        .def_static( "qos", []() { return writer_qos(); } )
        .def_static( "qos", []( reliability r, durability d ) { return writer_qos( r, d ); } );


    // The actual types are declared as functions and not classes: the py::init<> inheritance rules are pretty strict
    // and, coupled with shared_ptr usage, are very hard to get around. This is much simpler...
    using realdds::topics::device_info;
    using realdds::topics::flexible_msg;

    typedef std::shared_ptr< dds_topic > flexible_msg_create_topic( std::shared_ptr< dds_participant > const &,
                                                                    char const * );

    py::class_< device_info >( message, "device_info" )
        .def( py::init<>() )
        .def_static( "create_topic", static_cast< flexible_msg_create_topic * >( &flexible_msg::create_topic ) )
        .def_property( "name", &device_info::name, &device_info::set_name )
        .def_property( "topic_root", &device_info::topic_root, &device_info::set_topic_root )
        .def_property( "serial", &device_info::serial_number, &device_info::set_serial_number )
        .def_static( "from_json", &device_info::from_json )
        .def( "to_json", &device_info::to_json )
        .def( "__repr__",
              []( device_info const & self ) {
                  std::ostringstream os;
                  os << "<" SNAME ".device_info";
                  os << " " << self.to_json();
                  os << ">";
                  return os.str();
              } );

    using realdds::topics::flexible_msg;
    py::enum_< flexible_msg::data_format >( message, "flexible_format" )
        .value( "json", flexible_msg::data_format::JSON )
        .value( "cbor", flexible_msg::data_format::CBOR )
        .value( "custom", flexible_msg::data_format::CUSTOM );

    using eprosima::fastrtps::rtps::SampleIdentity;
    py::class_< SampleIdentity >( message, "sample_identity" )  //
        .def( "__repr__",
              []( SampleIdentity const & self )
              {
                  std::ostringstream os;
                  os << realdds::print_guid( self.writer_guid(), realdds::unknown_guid, true );
                  os << '.';
                  os << self.sequence_number();
                  return os.str();
              } );

    using realdds::dds_sample;
    py::class_< dds_sample >( message, "sample_info" )  //
        .def( py::init<>() )
        .def( "identity", []( dds_sample const & self ) { return self.sample_identity; } )
        .def( "source_timestamp", []( dds_sample const & self ) { return self.source_timestamp.to_ns(); } )
        .def( "reception_timestamp", []( dds_sample const & self ) { return self.reception_timestamp.to_ns(); } )
        .def( "__repr__",
              []( dds_sample const & self )
              {
                  std::ostringstream os;
                  os << "<sample #" << self.sample_identity.sequence_number();
                  os << " @ " << realdds::time_to_string( self.reception_timestamp );
                  os << " from " << realdds::print_guid( self.sample_identity.writer_guid(), realdds::unknown_guid, true );
                  os << ">";
                  return os.str();
              } );


    py::class_< flexible_msg >( message, "flexible" )
        .def( py::init<>() )
        .def( py::init( []( py::dict const & dict, uint32_t version )
                        { return flexible_msg( py_to_json( dict ), version ); } ),
              "json"_a,
              "version"_a = 0 )
        .def( py::init( []( std::string const & json_string ) {
            return flexible_msg( flexible_msg::data_format::JSON, json::parse( json_string ) );
        } ) )
        .def_static( "create_topic", static_cast< flexible_msg_create_topic * >( &flexible_msg::create_topic ) )
        .def_readwrite( "data_format", &flexible_msg::_data_format )
        .def_readwrite( "version", &flexible_msg::_version )
        .def_readwrite( "data", &flexible_msg::_data )
        .def( "invalidate", &flexible_msg::invalidate )
        .def( "is_valid", &flexible_msg::is_valid )
        .def( "__bool__", &flexible_msg::is_valid )
        .def( "__repr__",
              []( flexible_msg const & self ) {
                  std::ostringstream os;
                  os << "<" SNAME ".message.flexible ";
                  switch( self._data_format )
                  {
                  case flexible_msg::data_format::JSON:
                      os << "JSON";
                      break;
                  case flexible_msg::data_format::CBOR:
                      os << "CBOR";
                      break;
                  case flexible_msg::data_format::CUSTOM:
                      os << "CUSTOM";
                      break;
                  default:
                      os << "UNKNOWN(" << (int)self._data_format << ")";
                      break;
                  }
                  os << ' ';
                  if( ! self.is_valid() )
                      os << "INVALID";
                  else
                  {
                      os << self._data.size();
                      switch( self._data_format )
                      {
                      case flexible_msg::data_format::JSON:
                          os << ' ';
                          os << std::string( (char const *)self._data.data(), self._data.size() );
                          break;
                      default:
                          break;
                      }
                  }
                  os << ">";
                  return os.str();
              } )
        .def_static(
            "take_next",
            []( dds_topic_reader & reader, dds_sample * sample ) {
                auto actual_type = reader.topic()->get()->get_type_name();
                if( actual_type != flexible_msg::type().getName() )
                    throw std::runtime_error( "can't initialize raw::flexible from " + actual_type );
                flexible_msg data;
                if( ! flexible_msg::take_next( reader, &data, sample ) )
                    assert( ! data.is_valid() );
                return data;
            },
            py::arg( "reader" ), py::arg( "sample" ) = nullptr,
            py::call_guard< py::gil_scoped_release >() )
        .def( "json_data", &flexible_msg::json_data )
        .def( "json_string",
              []( flexible_msg const & self )
              { return std::string( (char const *)self._data.data(), self._data.size() ); } )
        .def( "write_to",
              []( flexible_msg & self, dds_topic_writer & writer ) { std::move( self ).write_to( writer ); } );


    using image_msg = realdds::topics::image_msg;
    py::class_< image_msg, std::shared_ptr< image_msg > >( message, "image" )
        .def( py::init<>() )
        .def_static( "create_topic", &image_msg::create_topic )
        .def_property(
            "data",
            []( image_msg const & self ) { return py::memoryview::from_memory( self.raw().data().data(), self.raw().data().size() ); },
            []( image_msg & self, std::vector< uint8_t > bytes ) { self.raw().data( std::move( bytes ) ); } )
        .def_property( "width", &image_msg::width, &image_msg::set_width )
        .def_property( "height", &image_msg::height, &image_msg::set_height )
        .def_property( "step", &image_msg::step, &image_msg::set_step )
        .def_property( "timestamp", &image_msg::timestamp, &image_msg::set_timestamp )
        .def_property_readonly( "is_bigendian", &image_msg::is_bigendian )
        .def_property( "encoding", &image_msg::encoding, &image_msg::set_encoding )
        .def_property( "frame_id", &image_msg::frame_id, &image_msg::set_frame_id )
        .def( "__bool__", &image_msg::is_valid )
        .def( "__repr__",
              []( image_msg const & self )
              {
                  std::ostringstream os;
                  os << "<" SNAME ".message.image";
                  if( self.is_valid() )
                  {
                      if( ! self.frame_id().empty() )
                          os << " " << self.frame_id();
                      if( ! self.encoding().empty() )
                          os << " '" << self.encoding() << "'";
                      os << " " << self.width() << 'x' << self.height();
                      if( ! self.step() )
                          os << " STEP 0";
                      else if( self.step() % self.width() )
                          os << " STEP " << self.step();
                      else if( self.raw().data().size() % self.step() )
                          os << " SIZE " << self.raw().data().size();
                      //else
                      //    os << ' ' << ( self.raw().data().size() / ( self.width() * self.height() ) ) << " Bpp";
                      if( self.is_bigendian() )
                          os << " BE";
                  }
                  os << " @ " << realdds::time_to_string( self.timestamp() );
                  os << ">";
                  return os.str();
              } )
        .def_static(
            "take_next",
            []( dds_topic_reader & reader, dds_sample * sample )
            {
                auto actual_type = reader.topic()->get()->get_type_name();
                if( actual_type != image_msg::type().getName() )
                    throw std::runtime_error( "can't initialize raw::image from " + actual_type );
                image_msg data;
                if( ! image_msg::take_next( reader, &data, sample ) )
                    assert( ! data.is_valid() );
                return data;
            },
            py::arg( "reader" ),
            py::arg( "sample" ) = nullptr,
            py::call_guard< py::gil_scoped_release >() )
        /*.def("write_to", &image_msg::write_to, py::call_guard< py::gil_scoped_release >())*/;

    using compressed_image_msg = realdds::topics::compressed_image_msg;
    py::class_< compressed_image_msg, std::shared_ptr< compressed_image_msg > >( message, "compressed_image" )
        .def( py::init<>() )
        .def_static( "create_topic", &compressed_image_msg::create_topic )
        .def_property(
            "data",
            []( compressed_image_msg const & self ) { return py::memoryview::from_memory( self.raw().data().data(), self.raw().data().size() ); },
            []( compressed_image_msg & self, std::vector< uint8_t > bytes ) { self.raw().data( std::move( bytes ) ); } )
        .def_property( "format", &compressed_image_msg::format, &compressed_image_msg::set_format )
        .def_property( "timestamp", &compressed_image_msg::timestamp, &compressed_image_msg::set_timestamp )
        .def_property( "frame_id", &compressed_image_msg::frame_id, &compressed_image_msg::set_frame_id )
        .def( "__bool__", &compressed_image_msg::is_valid )
        .def( "__repr__",
              []( compressed_image_msg const & self )
              {
                  std::ostringstream os;
                  os << "<" SNAME ".message.compressed_image";
                  if( self.is_valid() )
                  {
                      if( ! self.frame_id().empty() )
                          os << " " << self.frame_id();
                      if( ! self.format().empty() )
                          os << " '" << self.format() << "'";
                  }
                  os << " @ " << realdds::time_to_string( self.timestamp() );
                  os << ">";
                  return os.str();
              } )
        .def_static(
            "take_next",
            []( dds_topic_reader & reader, dds_sample * sample )
            {
                auto actual_type = reader.topic()->get()->get_type_name();
                if( actual_type != compressed_image_msg::type().getName() )
                    throw std::runtime_error( "can't initialize raw::compressed_image from " + actual_type );
                compressed_image_msg data;
                if( ! compressed_image_msg::take_next( reader, &data, sample ) )
                    assert( ! data.is_valid() );
                return data;
            },
            py::arg( "reader" ),
            py::arg( "sample" ) = nullptr,
            py::call_guard< py::gil_scoped_release >() )
        /*.def("write_to", &compressed_image_msg::write_to, py::call_guard< py::gil_scoped_release >())*/;

    using participant_entities_info_msg = realdds::topics::ros2::participant_entities_info_msg;
    py::class_< participant_entities_info_msg, std::shared_ptr< participant_entities_info_msg > >( message, "participant_entities_info" )
        .def( py::init<>() )
        .def_static( "create_topic", &participant_entities_info_msg::create_topic )
        .def_property( "gid", &participant_entities_info_msg::gid, &participant_entities_info_msg::set_gid )
        .def( "__bool__", &participant_entities_info_msg::is_valid )
        .def( "__repr__",
              []( participant_entities_info_msg const & self )
              {
                  std::ostringstream os;
                  os << "<" SNAME ".message.participant_entities_info";
                  if( self.is_valid() )
                  {
                      os << " " << realdds::print_guid( self.gid(), realdds::unknown_guid, true );
                      if( ! self.nodes().empty() )
                          os << " ";
                      for( auto & node : self.nodes() )
                      {
                          os << "[";
                          if( auto len = node.node_namespace().size() )
                          {
                              os << node.node_namespace();
                              if( node.node_namespace().c_str()[len-1] != realdds::topics::SEPARATOR )
                                  os << realdds::topics::SEPARATOR;
                          }
                          os << node.node_name();
                          os << " R " << node.reader_gid_seq().size();
                          os << " W " << node.writer_gid_seq().size();
                          os << "]";
                      }
                  }
                  os << ">";
                  return os.str();
              } )
        .def_static(
            "take_next",
            []( dds_topic_reader & reader, dds_sample * sample )
            {
                auto actual_type = reader.topic()->get()->get_type_name();
                if( actual_type != participant_entities_info_msg::type().getName() )
                    throw std::runtime_error( "can't initialize raw::image from " + actual_type );
                participant_entities_info_msg data;
                if( !participant_entities_info_msg::take_next( reader, &data, sample ) )
                    assert( ! data.is_valid() );
                return data;
            },
            py::arg( "reader" ),
            py::arg( "sample" ) = nullptr,
            py::call_guard< py::gil_scoped_release >() )
        /*.def("write_to", &participant_entities_info_msg::write_to, py::call_guard< py::gil_scoped_release >())*/;


    using blob_msg = realdds::topics::blob_msg;
    py::class_< blob_msg, std::shared_ptr< blob_msg > >( message, "blob" )
        .def( py::init<>() )
        .def( py::init(
            []( py::bytes const & bytes )
            {
                auto info = py::buffer( bytes ).request();
                auto data = reinterpret_cast< uint8_t const * >( info.ptr );
                size_t length = static_cast< size_t >( info.size );
                return blob_msg( std::vector< uint8_t >( data, data + length ) );
            } ) )
        .def_static(
            "create_topic",
            static_cast< std::shared_ptr< dds_topic > ( * )( std::shared_ptr< dds_participant > const &,
                                                             std::string const & ) >( &blob_msg::create_topic ) )
        .def( "data", []( blob_msg const & self ) { return self.data(); } )
        .def( "size", []( blob_msg const & self ) { return self.data().size(); } )
        .def( "crc", []( blob_msg const & self ) { return rsutils::number::calc_crc32( self.data().data(), self.data().size() ); } )
        .def( "__bool__", &blob_msg::is_valid )
        .def( "__repr__",
              []( blob_msg const & self )
              {
                  std::ostringstream os;
                  os << "<" SNAME ".blob_msg";
                  os << ' ' << self.data().size();
                  os << ' ' << rsutils::number::calc_crc32( self.data().data(), self.data().size() );
                  os << ">";
                  return os.str();
              } )
        .def_static(
            "take_next",
            []( dds_topic_reader & reader, dds_sample * sample )
            {
                auto actual_type = reader.topic()->get()->get_type_name();
                if( actual_type != blob_msg::type().getName() )
                    throw std::runtime_error( "can't initialize blob from " + actual_type );
                blob_msg data;
                if( ! blob_msg::take_next( reader, &data, sample ) )
                    assert( ! data.is_valid() );
                return data;
            },
            py::arg( "reader" ),
            py::arg( "sample" ) = nullptr,
            py::call_guard< py::gil_scoped_release >() )
        .def( "write_to", &blob_msg::write_to, py::call_guard< py::gil_scoped_release >() );


    using imu_msg = realdds::topics::imu_msg;
    py::class_< imu_msg, std::shared_ptr< imu_msg > >( message, "imu" )
        .def( py::init<>() )
        .def_static( "create_topic", &imu_msg::create_topic )
        .def_property(
            "gyro_data",
            []( imu_msg const & self ) { return get_vector3( self.gyro_data() ); },
            []( imu_msg & self, std::array< double, 3 > const & xyz ) { set_vector3( self.gyro_data(), xyz ); } )
        .def_property(
            "accel_data",
            []( imu_msg const & self ) { return get_vector3( self.accel_data() ); },
            []( imu_msg & self, std::array< double, 3 > const & xyz ) { set_vector3( self.accel_data(), xyz ); } )
        .def_property(
            "timestamp",
            []( imu_msg const & self ) { return self.timestamp(); },
            []( imu_msg & self, dds_time time ) { self.timestamp( time ); } )
        .def( "__bool__", &imu_msg::is_valid )
        .def( "__repr__",
              []( imu_msg const & self )
              {
                  std::ostringstream os;
                  os << "<" SNAME ".imu_msg " << self.to_string() << ">";
                  return os.str();
              } )
        .def_static(
            "take_next",
            []( dds_topic_reader & reader, dds_sample * sample )
            {
                auto actual_type = reader.topic()->get()->get_type_name();
                if( actual_type != imu_msg::type().getName() )
                    throw std::runtime_error( "can't initialize raw::imu from " + actual_type );
                imu_msg data;
                if( ! imu_msg::take_next( reader, &data, sample ) )
                    assert( ! data.is_valid() );
                return data;
            },
            py::arg( "reader" ),
            py::arg( "sample" ) = nullptr,
            py::call_guard< py::gil_scoped_release >() )
        .def( "write_to", &imu_msg::write_to, py::call_guard< py::gil_scoped_release >() );


    using realdds::dds_device_broadcaster;
    py::class_< dds_device_broadcaster, std::shared_ptr< dds_device_broadcaster > >( m, "device_broadcaster" )
        .def( py::init<>( []( std::shared_ptr< dds_publisher > const & publisher, device_info const & device_info )
                          { return std::make_shared< dds_device_broadcaster >( publisher, device_info, nullptr ); } ) );

    using realdds::dds_option;
    py::class_< dds_option, std::shared_ptr< dds_option > >( m, "option" )
        .def_static( "from_json", &dds_option::from_json )
        .def( "get_name", &dds_option::get_name )
        .def( "get_default_value", &dds_option::get_default_value )
        .def( "get_description", &dds_option::get_description )
        .def( "value_type", &dds_option::value_type )
        .def( "is_read_only", &dds_option::is_read_only )
        .def( "is_optional", &dds_option::is_optional )
        .def( "stream", &dds_option::stream )
        .def( "get_value", &dds_option::get_value )
        .def( "set_value", &dds_option::set_value )
        .def( "get_minimum_value", &dds_option::get_minimum_value )
        .def( "get_maximum_value", &dds_option::get_maximum_value )
        .def( "get_stepping", &dds_option::get_stepping )
        .def( "to_json", []( dds_option const & self ) { return self.to_json(); } )
        .def( "__repr__",
              []( dds_option const & self )
              {
                  std::ostringstream os;
                  os << '<' << self.get_name();
                  os << " " << self.get_value();
                  os << "  |";
                  if( self.is_optional() )
                      os << " optional";
                  if( self.is_read_only() )
                      os << " r/o";
                  if( self.get_default_value().exists() )
                      os << " default=" << self.get_default_value();
                  if( ! self.get_minimum_value().is_null() )
                      os << " min=" << self.get_minimum_value();
                  if( ! self.get_maximum_value().is_null() )
                      os << " max=" << self.get_maximum_value();
                  if( ! self.get_stepping().is_null() )
                      os << " step=" << self.get_stepping();
                  os << ">";
                  return os.str();
              } );

    using realdds::dds_embedded_filter;
    py::class_< dds_embedded_filter, std::shared_ptr< dds_embedded_filter > > embedded_filter_base(m, "embedded_filter");
    embedded_filter_base //
        .def_static("from_json", &dds_embedded_filter::from_json)
        .def("init", &dds_embedded_filter::init)
        .def("init_options", &dds_embedded_filter::init_options)
        .def("set_options", &dds_embedded_filter::set_options)
        .def("to_json", &dds_embedded_filter::to_json)
        .def("get_name", &dds_embedded_filter::get_name);

    using realdds::dds_decimation_filter;
    py::class_< dds_decimation_filter, dds_embedded_filter, std::shared_ptr< dds_decimation_filter > > decimation_embedded_filter(m, "decimation_embedded_filter");
    decimation_embedded_filter //
        .def_static("from_json", &dds_decimation_filter::from_json)
        .def(py::init<>());

    using realdds::dds_temporal_filter;
    py::class_< dds_temporal_filter, dds_embedded_filter, std::shared_ptr< dds_temporal_filter > > temporal_embedded_filter(m, "temporal_embedded_filter");
    temporal_embedded_filter //
        .def_static("from_json", &dds_temporal_filter::from_json)
        .def(py::init<>());

    using realdds::dds_video_encoding;
    py::class_< dds_video_encoding > video_encoding( m, "video_encoding" );
    video_encoding  //
        .def( py::init<>() )
        .def( py::init< std::string const & >() )
        .def( "is_valid", &dds_video_encoding::is_valid )
        .def( "to_rs2", &dds_video_encoding::to_rs2 )
        .def( "from_rs2", &dds_video_encoding::from_rs2 )
        .def( py::self == py::self )
        .def( py::self != py::self )
        .def( "__bool__", &dds_video_encoding::is_valid )
        .def( "__repr__", []( dds_video_encoding const & self ) { return self.to_string(); } );
    video_encoding.attr( "z16" ) = dds_video_encoding( "16UC1" );
    video_encoding.attr( "y8" ) = dds_video_encoding( "mono8" );
    video_encoding.attr( "y16" ) = dds_video_encoding( "mono16" );
    video_encoding.attr( "byr2" ) = dds_video_encoding( "BYR2" );
    video_encoding.attr( "yuyv" ) = dds_video_encoding( "yuv422_yuy2" );
    video_encoding.attr( "uyvy" ) = dds_video_encoding( "uyvy" );
    video_encoding.attr( "rgb" ) = dds_video_encoding( "rgb8" );
    video_encoding.attr( "y12i" ) = dds_video_encoding( "Y12I" );

    using realdds::dds_stream_profile;
    py::class_< dds_stream_profile, std::shared_ptr< dds_stream_profile > > stream_profile_base( m, "stream_profile" );
    stream_profile_base
        .def( "frequency", &dds_stream_profile::frequency )
        .def( "stream", &dds_stream_profile::stream )
        .def( "to_string", &dds_stream_profile::to_string )
        .def( "details_to_string", &dds_stream_profile::details_to_string )
        .def( "to_json", []( dds_stream_profile const & self ) { return self.to_json(); } )
        .def( "__repr__", []( dds_stream_profile const & self ) {
            std::ostringstream os;
            std::string self_as_string = self.to_string();  // <video 0xUID ...>
            std::string type;
            auto stream = self.stream();
            if( stream )
                type = std::string( stream->type_string() ) + '_';
            os << "<" SNAME "." << type << "stream_profile " << ( self_as_string.c_str() + 1 );
            return os.str();
        } );

    using realdds::dds_video_stream_profile;
    py::class_< dds_video_stream_profile, std::shared_ptr< dds_video_stream_profile > >( m, "video_stream_profile", stream_profile_base )
        .def( py::init< int16_t, dds_video_encoding, uint16_t, uint16_t >() )
        .def( "format", &dds_video_stream_profile::encoding )
        .def( "width", &dds_video_stream_profile::width )
        .def( "height", &dds_video_stream_profile::height );

    using realdds::dds_motion_stream_profile;
    py::class_< dds_motion_stream_profile, std::shared_ptr< dds_motion_stream_profile > >( m, "motion_stream_profile", stream_profile_base )
        .def( py::init< int16_t >() );

    using realdds::dds_stream_base;
    py::class_< dds_stream_base, std::shared_ptr< dds_stream_base > > stream_base( m, "stream_base" );
    stream_base
        .def( "name", &dds_stream_base::name )
        .def( "sensor_name", &dds_stream_base::sensor_name )
        .def( "type_string", &dds_stream_base::type_string )
        .def( "profiles", &dds_stream_base::profiles )
        .def( "enable_metadata", &dds_stream_base::enable_metadata )
        .def( "init_profiles", &dds_stream_base::init_profiles )
        .def( "init_options", &dds_stream_base::init_options )
        .def( "init_embedded_filters", &dds_stream_base::init_embedded_filters)
        .def( "default_profile_index", &dds_stream_base::default_profile_index )
        .def( "default_profile", &dds_stream_base::default_profile )
        .def( "options", &dds_stream_base::options )
        .def( "is_open", &dds_stream_base::is_open )
        .def( "is_streaming", &dds_stream_base::is_streaming )
        .def( "get_topic", &dds_stream_base::get_topic );

    using realdds::dds_stream_server;
    py::class_< dds_stream_server, std::shared_ptr< dds_stream_server > > stream_server_base( m, "stream_server", stream_base );
    stream_server_base  //
        .def( "on_readers_changed", &dds_stream_server::on_readers_changed )
        .def( "open", &dds_stream_server::open )
        .def( "stop_streaming", &dds_stream_server::stop_streaming )
        .def( "__repr__",
              []( dds_stream_server const & self )
              {
                  std::ostringstream os;
                  os << "<" SNAME "." << self.type_string() << "_stream_server \"" << self.name() << "\"";
                  os << " " << self.profiles().size() << "[" << self.default_profile_index() << "] profiles";
                  os << '>';
                  return os.str();
              } );

    using realdds::dds_video_stream_server;
    py::class_< dds_video_stream_server, std::shared_ptr< dds_video_stream_server > >
        video_stream_server_base( m, "video_stream_server", stream_server_base );
    video_stream_server_base
        .def( "set_intrinsics", &dds_video_stream_server::set_intrinsics )
        .def( "get_intrinsics", &dds_video_stream_server::get_intrinsics )
        .def( "start_streaming",
              []( dds_video_stream_server & self, dds_video_encoding encoding, int width, int height ) {
                  self.start_streaming( { encoding, height, width } );
              } )
        .def( "publish_image", &dds_video_stream_server::publish_image );

    using realdds::dds_depth_stream_server;
    py::class_< dds_depth_stream_server, std::shared_ptr< dds_depth_stream_server > >( m, "depth_stream_server", video_stream_server_base )
        .def( py::init< std::string const &, std::string const & >(), "stream_name"_a, "sensor_name"_a );

    using realdds::dds_ir_stream_server;
    py::class_< dds_ir_stream_server, std::shared_ptr< dds_ir_stream_server > >( m, "ir_stream_server", video_stream_server_base )
        .def( py::init< std::string const&, std::string const& >(), "stream_name"_a, "sensor_name"_a );

    using realdds::dds_color_stream_server;
    py::class_< dds_color_stream_server, std::shared_ptr< dds_color_stream_server > >( m, "color_stream_server", video_stream_server_base )
        .def( py::init< std::string const&, std::string const& >(), "stream_name"_a, "sensor_name"_a );

    using realdds::dds_confidence_stream_server;
    py::class_< dds_confidence_stream_server, std::shared_ptr< dds_confidence_stream_server > >( m, "confidence_stream_server", video_stream_server_base )
        .def( py::init< std::string const&, std::string const& >(), "stream_name"_a, "sensor_name"_a );

    using realdds::dds_motion_stream_server;
    py::class_< dds_motion_stream_server, std::shared_ptr< dds_motion_stream_server > >( m, "motion_stream_server", stream_server_base )
        .def( py::init< std::string const &, std::string const & >(), "stream_name"_a, "sensor_name"_a )
        .def( "set_gyro_intrinsics", &dds_motion_stream_server::set_gyro_intrinsics )
        .def( "set_accel_intrinsics", &dds_motion_stream_server::set_accel_intrinsics )
        .def( "start_streaming", &dds_motion_stream_server::start_streaming );

    // To have the python code be able to modify json objects in callbacks, we need to somehow refer to the original
    // json object as changes to translated dict will not automatically get picked up!
    // We do this thru the [] operator:
    //     def callback( json ):
    //         print( json['value'] )            # calls __getitem__
    //         json['value'] = { 'more': True }  # calls __setitem__
    struct json_ref { json & j; };
    py::class_< json_ref, std::shared_ptr< json_ref > >( m, "json_ref" )
        .def( "__getitem__", []( json_ref const & jr, std::string const & key ) { return jr.j.at( key ); } )
        .def( "__setitem__", []( json_ref & jr, std::string const & key, json const & value ) { jr.j[key] = value; } );

    using realdds::dds_device_server;
    py::class_< dds_device_server, std::shared_ptr< dds_device_server > >( m, "device_server" )
        .def( py::init< std::shared_ptr< dds_participant > const&, std::string const& >() )
        .def( "init", &dds_device_server::init )
        .def( "guid", &dds_device_server::guid )
        .def( "streams",
              []( dds_device_server const & self )
              {
                  std::vector< std::shared_ptr< dds_stream_server > > streams;
                  for( auto const & name2stream : self.streams() )
                      streams.push_back( name2stream.second );
                  return streams;
              } )
        .def(
            "publish_notification",
            []( dds_device_server & self, json const & j ) { self.publish_notification( j ); },
            py::call_guard< py::gil_scoped_release >() )
        .def( "publish_metadata", &dds_device_server::publish_metadata, py::call_guard< py::gil_scoped_release >() )
        .def( "broadcast", &dds_device_server::broadcast )
        .def( "broadcast_disconnect", &dds_device_server::broadcast_disconnect, py::arg( "ack-timeout" ) = dds_time() )
        .def( FN_FWD( dds_device_server, on_set_option,
                      (dds_device_server *, std::shared_ptr< realdds::dds_option > const &, json_ref &&),
                      ( std::shared_ptr< realdds::dds_option > const & option, json & value ),
                      callback( self, option, json_ref{ value } ); ) )
        .def( FN_FWD_R( dds_device_server, on_control,
                        false,
                        (dds_device_server *, std::string const &, py::object &&, json_ref &&),
                        ( std::string const & id, json const & control, json & reply ),
                        return callback( self, id, json_to_py( control ), json_ref{ reply } ); ) );

    using realdds::dds_stream;
    py::class_< dds_stream, std::shared_ptr< dds_stream > > stream_client_base( m, "stream", stream_base );
    stream_client_base  //
        .def( "open", &dds_stream::open )
        .def( "close", &dds_stream::close )
        .def( "is_open", &dds_stream::is_open )
        .def( "start_streaming", &dds_stream::start_streaming )
        .def( "stop_streaming", &dds_stream::stop_streaming )
        .def( "__repr__", []( dds_stream const & self ) {
            std::ostringstream os;
            os << "<" SNAME "." << self.type_string() << "_stream \"" << self.name() << "\"";
            os << " " << self.profiles().size() << "[" << self.default_profile_index() << "] profiles";
            os << '>';
            return os.str();
        } );

    using realdds::dds_video_stream;
    py::class_< dds_video_stream, std::shared_ptr< dds_video_stream > >
        video_stream_client_base( m, "video_stream", stream_client_base );
    video_stream_client_base  //
        .def( "set_intrinsics", &dds_video_stream::set_intrinsics )
        .def( "get_intrinsics", &dds_video_stream::get_intrinsics )
        .def( FN_FWD( dds_video_stream,
                      on_data_available,
                      ( dds_video_stream *, std::vector< uint8_t > && image_buffer, dds_time && image_time, dds_sample && ),
                      ( std::vector< uint8_t > && i, dds_time && t, dds_sample && sample ),
                      callback( self, std::move( i ), std::move( t ), std::move( sample ) ); ) );

    using realdds::dds_depth_stream;
    py::class_< dds_depth_stream, std::shared_ptr< dds_depth_stream > >( m, "depth_stream", video_stream_client_base )
        .def( py::init< std::string const &, std::string const & >(), "stream_name"_a, "sensor_name"_a );

    using realdds::dds_ir_stream;
    py::class_< dds_ir_stream, std::shared_ptr< dds_ir_stream > >( m, "ir_stream", video_stream_client_base )
        .def( py::init< std::string const &, std::string const & >(), "stream_name"_a, "sensor_name"_a );

    using realdds::dds_color_stream;
    py::class_< dds_color_stream, std::shared_ptr< dds_color_stream > >( m, "color_stream", video_stream_client_base )
        .def( py::init< std::string const &, std::string const & >(), "stream_name"_a, "sensor_name"_a );

    using realdds::dds_confidence_stream;
    py::class_< dds_confidence_stream, std::shared_ptr< dds_confidence_stream > >( m, "confidence_stream", video_stream_client_base )
        .def( py::init< std::string const &, std::string const & >(), "stream_name"_a, "sensor_name"_a );

    using realdds::dds_motion_stream;
    py::class_< dds_motion_stream, std::shared_ptr< dds_motion_stream > >
        motion_stream_client_base( m, "motion_stream", stream_client_base );
    motion_stream_client_base
        .def( "set_gyro_intrinsics", &dds_motion_stream::set_gyro_intrinsics )
        .def( "set_accel_intrinsics", &dds_motion_stream::set_accel_intrinsics )
        .def( FN_FWD( dds_motion_stream,
                      on_data_available,
                      ( dds_motion_stream *, imu_msg &&, dds_sample && ),
                      ( imu_msg && i, dds_sample && sample ),
                      callback( self, std::move( i ), std::move( sample ) ); ) );


    using subscription = rsutils::subscription;
    py::class_< subscription,
                std::shared_ptr< subscription >  // handled with a shared_ptr
                >( m, "subscription" )
        .def( "cancel", &subscription::cancel )
        .def( "__bool__", []( subscription const & self ) { return self.is_active(); } )
        .def( "__repr__",
              []( subscription const & self )
              {
                  std::ostringstream os;
                  os << "<" SNAME ".subscription";
                  if( ! self.is_active() )
                      os << " cancelled";
                  os << ">";
                  return os.str();
              } );


    using realdds::dds_device;
    py::class_< dds_device,
        std::shared_ptr< dds_device >  // handled with a shared_ptr
    >( m, "device" )
        .def( py::init< std::shared_ptr< dds_participant > const &, device_info const & >() )
        .def( "device_info", &dds_device::device_info )
        .def( "participant", &dds_device::participant )
        .def( "server_guid", &dds_device::server_guid )
        .def( "guid", &dds_device::guid )
        .def( "is_ready", &dds_device::is_ready )
        .def( "is_online", &dds_device::is_online )
        .def( "is_offline", &dds_device::is_offline )
        .def( "wait_until_ready",
              &dds_device::wait_until_ready,
              py::call_guard< py::gil_scoped_release >(),
              "timeout-ms"_a = 5000, py::arg_v( "allow_partial_capabilities", false, "False" ) )
        .def( "wait_until_online",
              &dds_device::wait_until_online,
              py::call_guard< py::gil_scoped_release >(),
              "timeout-ms"_a = 5000 )
        .def( "wait_until_offline",
              &dds_device::wait_until_offline,
              py::call_guard< py::gil_scoped_release >(),
              "timeout-ms"_a = 5000 )
        .def( "on_metadata_available",
              []( dds_device & self, std::function< void( dds_device &, py::object && ) > callback )
              {
                  return std::make_shared< subscription >( self.on_metadata_available(
                      [&self, callback]( std::shared_ptr< const json > const & pj )
                      { FN_FWD_CALL( dds_device, "on_metadata_available", callback( self, json_to_py( *pj ) ); ) } ) );
              } )
        .def( "on_device_log",
              []( dds_device & self, std::function< void( dds_device &, dds_nsec, char, std::string const &, py::object && ) > callback )
              {
                  return std::make_shared< subscription >( self.on_device_log(
                      [&self, callback]( dds_nsec timestamp, char type, std::string const & text, json const & data )
                      { FN_FWD_CALL( dds_device, "on_device_log", callback( self, timestamp, type, text, json_to_py( data ) ); ) } ) );
              } )
        .def( "on_notification",
              []( dds_device & self, std::function< void( dds_device &, std::string const &, py::object && ) > callback )
              {
                  return std::make_shared< subscription >( self.on_notification(
                      [&self, callback]( std::string const & id, json const & data )
                      { FN_FWD_CALL( dds_device, "on_notification", callback( self, id, json_to_py( data ) ); ) } ) );
              } )
        .def( "n_streams", &dds_device::number_of_streams )
        .def( "streams",
              []( dds_device const & self ) {
                  std::vector< std::shared_ptr< dds_stream > > streams;
                  self.foreach_stream(
                      [&]( std::shared_ptr< dds_stream > const & stream ) { streams.push_back( stream ); } );
                  return streams;
              } )
        .def( "options",
              []( dds_device const & self ) {
                  std::vector< std::shared_ptr< dds_option > > options;
                  self.foreach_option( [&]( std::shared_ptr< dds_option > const & option ) { options.push_back( option ); } );
                  return options;
              } )
        .def( "set_option_value", &dds_device::set_option_value )
        .def( "query_option_value", &dds_device::query_option_value )
        .def( "set_filter_value", &dds_device::set_embedded_filter)
        .def( "query_filter_value", &dds_device::query_embedded_filter)
        .def(
            "send_control",
            []( dds_device & self, json const & j, bool wait_for_reply )
            {
                json reply;
                self.send_control( j, wait_for_reply ? &reply : nullptr );
                return reply;
            },
            py::arg( "json" ), py::arg( "wait-for-reply" ) = false,
            py::call_guard< py::gil_scoped_release >() )
        .def_static( "check_reply", &dds_device::check_reply )
        .def( "__repr__", []( dds_device const & self ) {
            std::ostringstream os;
            os << "<" SNAME ".device";
            os << " " << self.participant()->print( self.guid() );
            if( ! self.device_info().name().empty() )
                os << " \"" << self.device_info().name() << "\"";
            os << " @ " << self.device_info().debug_name();
            os << ">";
            return os.str();
        } );

    using realdds::dds_device_watcher;
    py::class_< dds_device_watcher >( m, "device_watcher" )
        .def( py::init< std::shared_ptr< dds_participant > const & >() )
        .def( "start", &dds_device_watcher::start )
        .def( "stop", &dds_device_watcher::stop )
        .def( "is_stopped", &dds_device_watcher::is_stopped )
        .def( FN_FWD( dds_device_watcher,
                      on_device_added,
                      ( dds_device_watcher const *, std::shared_ptr< dds_device > const & ),
                      ( std::shared_ptr< dds_device > const & dev ),
                      callback( self, dev ); ) )
        .def( FN_FWD( dds_device_watcher,
                      on_device_removed,
                      ( dds_device_watcher const *, std::shared_ptr< dds_device > const & ),
                      ( std::shared_ptr< dds_device > const & dev ),
                      callback( self, dev ); ) )
        .def( "devices",
              []( dds_device_watcher const & self )
              {
                  std::vector< std::shared_ptr< dds_device > > devices;
                  self.foreach_device(
                      [&]( std::shared_ptr< dds_device > const & dev )
                      {
                          devices.push_back( dev );
                          return true;
                      } );
                  return devices;
              } )
        .def( "is_device_broadcast", &dds_device_watcher::is_device_broadcast );

    using realdds::dds_stream_sensor_bridge;
    py::class_< dds_stream_sensor_bridge >( m, "stream_sensor_bridge" )
        .def( py::init<>() )
        .def( FN_FWD( dds_stream_sensor_bridge,
                      on_start_sensor,
                      (std::string const &, std::vector< std::shared_ptr< realdds::dds_stream_profile > > const &),
                      ( std::string const & sensor_name,
                        std::vector< std::shared_ptr< realdds::dds_stream_profile > > const & profiles ),
                      callback( sensor_name, profiles ); ) )
        .def( FN_FWD( dds_stream_sensor_bridge,
                      on_stop_sensor,
                      (std::string const &),
                      ( std::string const & sensor_name ),
                      callback( sensor_name ); ) )
        .def( FN_FWD( dds_stream_sensor_bridge,
                      on_readers_changed,
                      (std::shared_ptr< dds_stream_server > const &, int),
                      ( std::shared_ptr< dds_stream_server > const & server, int n_readers ),
                      callback( server, n_readers ); ) )
        .def( FN_FWD( dds_stream_sensor_bridge,
                      on_error,
                      (std::string const &),
                      ( std::string const & error_string ),
                      callback( error_string ); ) )
        .def( "init", &dds_stream_sensor_bridge::init )
        .def( "reset", &dds_stream_sensor_bridge::reset, py::arg( "by_force" ) = false )
        .def( "open", &dds_stream_sensor_bridge::open )
        .def( "close", &dds_stream_sensor_bridge::close )
        .def( "add_implicit_profiles", &dds_stream_sensor_bridge::add_implicit_profiles )
        .def( "commit", &dds_stream_sensor_bridge::commit )
        .def( "is_streaming", &dds_stream_sensor_bridge::is_streaming );

    // Custom syncer with its own frame management over image_msg
    struct dds_metadata_syncer : realdds::dds_metadata_syncer
    {
        typedef std::shared_ptr< image_msg > frame_type;  // the base case for image_msg (see above)

    private:
        typedef realdds::dds_metadata_syncer super;

        static void frame_releaser( void * frame )
        {
            // frame is really a pointer to a shared object that we create on the heap
            delete static_cast< frame_type * >( frame );
        }

    public:
        dds_metadata_syncer()
        {
            on_frame_release( frame_releaser );
            start();
        }

        ~dds_metadata_syncer()
        {
            stop();
        }

        void enqueue_frame( key_type key, frame_type const & img )
        {
            // We need to keep the image alive and somehow point to it at the same time
            super::enqueue_frame( key, hold( new std::shared_ptr< image_msg >( img ) ) );
        }

        frame_type get_frame( frame_holder const & fh ) const
        {
            return *static_cast< frame_type * >( fh.get() );
        }
    };

    py::class_< dds_metadata_syncer > metadata_syncer( m, "metadata_syncer" );
    metadata_syncer  //
        .def( py::init<>() )
        .def( FN_FWD( dds_metadata_syncer,
                      on_frame_ready,
                      ( dds_metadata_syncer::frame_type, json const & ),
                      ( dds_metadata_syncer::frame_holder && fh, std::shared_ptr< const json > const & metadata ),
                      callback( self->get_frame( fh ), metadata ? *metadata : json() ); ) )
        .def( FN_FWD( dds_metadata_syncer,
                      on_metadata_dropped,
                      ( dds_metadata_syncer::key_type, json const & ),
                      ( dds_metadata_syncer::key_type key, std::shared_ptr< const json > const & metadata ),
                      callback( key, metadata ? *metadata : json() ); ) )
        .def( "enqueue_frame", &dds_metadata_syncer::enqueue_frame )
        .def( "enqueue_metadata",
              []( dds_metadata_syncer & self, dds_metadata_syncer::key_type key, json const & j )
              { self.enqueue_metadata( key, std::make_shared< const json >( j ) ); } );
    metadata_syncer.attr( "max_frame_queue_size" ) = dds_metadata_syncer::max_frame_queue_size;
    metadata_syncer.attr( "max_md_queue_size" ) = dds_metadata_syncer::max_md_queue_size;
}
