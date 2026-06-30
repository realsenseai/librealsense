// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022-5 RealSense, Inc. All Rights Reserved.

#include <realdds/dds-device-server.h>
#include <realdds/dds-stream-server.h>
#include <realdds/dds-participant.h>
#include <realdds/dds-option.h>
#include <realdds/dds-log-consumer.h>
#include <realdds/topics/device-info-msg.h>
#include <realdds/topics/dds-topic-names.h>
#include <realdds/topics/ros2/participant-entities-info-msg.h>
#include <realdds/topics/ros2/parameter-events-msg.h>
#include <realdds/dds-publisher.h>
#include <realdds/dds-topic-writer.h>

#include "lrs-device-watcher.h"
#include "lrs-device-controller.h"

#include <common/cli.h>

#include <rsutils/os/executable-name.h>
#include <rsutils/os/special-folder.h>
#include <rsutils/easylogging/easyloggingpp.h>
#include <rsutils/json.h>
#include <rsutils/json-config.h>
#include <rsutils/concurrency/control-c-handler.h>
#include <rsutils/concurrency/delayed.h>
#include <rsutils/string/from.h>

#include <string>
#include <iostream>
#include <map>
#include <set>

using namespace realdds;
using rsutils::json;


std::string build_node_name( std::string const & rs2_camera_name, std::string const & serial_number )
{
    // Build the ROS2 node name, without a namespace
    // example: D435_11223344
    // NOTE: the serial number here may be the device update ID or the actual device serial number
    // We don't need the prefix in the path; accept both the legacy "Intel RealSense " and current "RealSense "
    // (e.g. names coming from playback of older recordings) so node-names/topic-roots stay stable.
    constexpr char const * DEVICE_NAME_PREFIXES[] = { "Intel RealSense ", "RealSense " };
    std::string model_name = rs2_camera_name;
    for( char const * prefix : DEVICE_NAME_PREFIXES )
    {
        size_t const cch = strlen( prefix );
        if( model_name.length() > cch && 0 == strncmp( model_name.data(), prefix, cch ) )
        {
            model_name.erase( 0, cch );
            break;
        }
    }
    for( auto it = model_name.begin(); it != model_name.end(); ++it )
        if( *it == ' ' )
            *it = '_';  // e.g., 'D4xx Recovery'
    return model_name + '_' + serial_number;
}


std::string get_topic_root( std::string const & rs2_camera_name, std::string const & serial_number )
{
    // Build device root path (we use a device model only name like DXXX)
    // example: realsense/D435_11223344
    return realdds::topics::ROOT + build_node_name( rs2_camera_name, serial_number );
}


topics::device_info rs2_device_to_info( rs2::device const & dev )
{
    json j;

    // Name is mandatory
    std::string const name = dev.get_info( RS2_CAMERA_INFO_NAME );
    j[realdds::topics::device_info::key::name] = name;

    if( dev.supports( RS2_CAMERA_INFO_SERIAL_NUMBER ) )
        j[realdds::topics::device_info::key::serial] = dev.get_info( RS2_CAMERA_INFO_SERIAL_NUMBER );
    if( dev.supports( RS2_CAMERA_INFO_PRODUCT_LINE ) )
        j["product-line"] = dev.get_info( RS2_CAMERA_INFO_PRODUCT_LINE );
    if( dev.supports( RS2_CAMERA_INFO_CAMERA_LOCKED ) )
        j["locked"] = ( strcmp( dev.get_info( RS2_CAMERA_INFO_CAMERA_LOCKED ), "YES" ) == 0 );

    // FW update ID is a must for DFU; all cameras should have one
    std::string const serial_number = dev.get_info( RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID );
    j["fw-update-id"] = serial_number;
    if( auto update_device = rs2::update_device( dev ) )
    {
        j[realdds::topics::device_info::key::recovery] = true;
        if( dev.supports( RS2_CAMERA_INFO_PRODUCT_ID ) )
            // Append the ID so we have it
            j[realdds::topics::device_info::key::name] = name + " [" + dev.get_info( RS2_CAMERA_INFO_PRODUCT_ID ) + "]";
    }

    if( dev.supports( RS2_CAMERA_INFO_FIRMWARE_VERSION ) )
        j["fw-version"] = dev.get_info( RS2_CAMERA_INFO_FIRMWARE_VERSION );

    // Build device topic root path
    j[realdds::topics::device_info::key::topic_root] = get_topic_root( name, serial_number );

    return topics::device_info::from_json( j );
}


static json load_settings( json const & local_settings )
{
    // Load the realsense configuration file settings
    std::string const filename = rsutils::os::get_special_folder( rsutils::os::special_folder::app_data ) + RS2_CONFIG_FILENAME;
    auto config = rsutils::json_config::load_from_file( filename );

    // Take just the 'context' part
    config = rsutils::json_config::load_settings( config, "context", "config-file" );

    // Take the "dds" settings only
    config = config.nested( "dds" );

    // We should always have DDS enabled
    if( config.is_object() )
        config.erase( "enabled" );

    // Patch the given local settings into the configuration
    config.override( local_settings, "local settings" );

    return config;
}


int main( int argc, char * argv[] )
try
{
    using cli = rs2::cli_no_dds;  // no --eth, --no-eth, --eth-only, --domain-id
    cli::value< dds_domain_id > domain_arg( "domain-id", "0-232", 0, "Select domain ID to publish on" );
    cli cmd( "librealsense rs-dds-adapter tool: use USB devices as network devices" );
    auto settings = cmd  // in order we want listed:
        .arg( domain_arg )
        .process( argc, argv );

    // Configure the same logger as librealsense
    rsutils::configure_elpp_logger( cmd.debug_arg.isSet() );
    // Intercept DDS messages and redirect them to our own logging mechanism
    eprosima::fastdds::dds::Log::ClearConsumers();
    eprosima::fastdds::dds::Log::RegisterConsumer( realdds::log_consumer::create() );

    if( cmd.debug_arg.isSet() )
        eprosima::fastdds::dds::Log::SetVerbosity( eprosima::fastdds::dds::Log::Info );
    else
        eprosima::fastdds::dds::Log::SetVerbosity( eprosima::fastdds::dds::Log::Error );

    dds_domain_id domain = -1;  // from settings; default to 0
    if( domain_arg.isSet() )
    {
        domain = domain_arg.getValue();
        if( domain < 0 || domain > 232 )
        {
            std::cerr << "Invalid domain value, enter a value in the range [0, 232]" << std::endl;
            return EXIT_FAILURE;
        }
    }

    std::cout << "Starting RS DDS Adapter on domain " << domain << " ..." << std::endl;

    // Create a DDS participant
    auto participant = std::make_shared< dds_participant >();
    {
        rsutils::json dds_settings = rsutils::json::object();
        dds_settings = load_settings( dds_settings );
        participant->init( domain, "rs-dds-adapter", std::move( dds_settings ) );
    }

    // Create a ROS2 "context" (not really, but good enough for now)
    std::shared_ptr< realdds::dds_topic_writer > discovery_info_writer;
    auto publisher = std::make_shared< realdds::dds_publisher >( participant );
    if( auto topic = realdds::topics::ros2::participant_entities_info_msg::create_topic(
        participant,
        topics::ros2::DISCOVERY_INFO ) )
    {
        // The ros_discovery_info topic is where we'll make our cameras visible to ROS2: each camera will be a ROS2 node
        // that will publish its own set of parameters etc.
        discovery_info_writer = std::make_shared< dds_topic_writer >( topic, publisher );
        dds_topic_writer::qos wqos( eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS,
                                    eprosima::fastdds::dds::TRANSIENT_LOCAL_DURABILITY_QOS );
        discovery_info_writer->override_qos_from_json( wqos,
            participant->settings().nested( "device", "ros-discovery-info" ) );
        discovery_info_writer->run( wqos );
    }

    struct device_handler
    {
        topics::device_info info;
        std::shared_ptr< dds_device_server > server;
        std::shared_ptr< tools::lrs_device_controller > controller;
    };
    std::map< rs2::device, device_handler > device_handlers_list;

    std::cout << "Start listening to RS devices.." << std::endl;

    // Create a RealSense context
    settings["dds"] = false;                  // We want non-DDS (local) devices only
    settings["format-conversion"] = "basic";  // Conversion of raw sensor formats will be done by the receiver
    rs2::context ctx( settings.dump() );

    // Set up the ros_discovery_info broadcaster
    rsutils::concurrency::delayed broadcast_ros_discovery_info(
        [&]
        {
            realdds::topics::ros2::participant_entities_info_msg msg;

            // The RMW context gid is initialized, at least with the FastRTPS implementation, from the participant's GUID (see
            // init_context_impl()), so the GID of the "ParticipantEntitiesInfo" is simply the participant GUID.
            msg.set_gid( participant->guid() );

            std::string const node_namespace = realdds::topics::ros2::NAMESPACE;
            for( auto const & dev2handler : device_handlers_list )
            {
                auto & device = dev2handler.second;
                if( device.info.serial_number().empty() )
                    continue;  // E.g., recovery device
                std::string node_name = build_node_name( device.info.name(), device.info.serial_number() );
                topics::ros2::participant_entities_info_msg::node node( node_namespace, node_name );
                device.controller->fill_ros2_node_entities( node );
                msg.add( std::move( node ) );
            }

            msg.write_to( *discovery_info_writer );
        } );

    // And a writer for the ROS2 /parameter_events topics
    std::shared_ptr< realdds::dds_topic_writer > parameter_events_writer;
    std::string const
        parameters_events_topic_name = rsutils::string::from()
                                    << topics::ros2::ROOT
                                    << topics::ros2::PARAMETER_EVENTS_NAME;
    if( auto topic = realdds::topics::ros2::parameter_events_msg::create_topic(
        participant,
        parameters_events_topic_name.c_str() ) )
    {
        // The parameter_events topic is where we'll publish any new/changed/deleted events
        parameter_events_writer = std::make_shared< dds_topic_writer >( topic, publisher );
        dds_topic_writer::qos wqos( eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS );
        parameter_events_writer->override_qos_from_json( wqos,
            participant->settings().nested( "device", "ros-parameter-events" ) );
        parameter_events_writer->run( wqos );
    }

    // Run the LRS device watcher
    tools::lrs_device_watcher dev_watcher( ctx );
    dev_watcher.run(
        // Handle a device connection
        [&]( rs2::device dev ) {

            auto dev_info = rs2_device_to_info( dev );

            // Create a dds-device-server for this device
            auto dds_device_server
                = std::make_shared< realdds::dds_device_server >( participant, dev_info.topic_root() );
 
            // Create a lrs_device_manager for this device
            auto lrs_device_controller = std::make_shared< tools::lrs_device_controller >( dev, dds_device_server );

            if( ! dev_info.serial_number().empty() )
                lrs_device_controller->initialize_ros2_node_entities(
                    build_node_name( dev_info.name(), dev_info.serial_number() ),
                    parameter_events_writer );

            // Finally, we're ready to broadcast it
            dds_device_server->broadcast( dev_info );

            // Keep a pair of device controller and server per RS device
            device_handlers_list.emplace( dev, device_handler{ dev_info, dds_device_server, lrs_device_controller } );

            if( ! dev_info.serial_number().empty() )
                broadcast_ros_discovery_info.call_in( std::chrono::milliseconds( 200 ) );
        },
        // Handle a device disconnection
        [&]( rs2::device dev ) {
            // Remove the dds-server for this device
            auto const & handler = device_handlers_list.at( dev );

            device_handlers_list.erase( dev );

            broadcast_ros_discovery_info.call_in( std::chrono::milliseconds( 200 ) );
        } );

    {
        rsutils::concurrency::control_c_handler control_c;
        control_c.wait();
    }
    std::cout << "Shutting down rs-dds-adapter..." << std::endl;

    // Broadcast we are shutting down
    device_handlers_list.clear();
    broadcast_ros_discovery_info.call_now();
    discovery_info_writer->wait_for_acks( realdds::dds_time{ 1, 0 } );

    return EXIT_SUCCESS;
}
catch( const rs2::error & e )
{
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args()
              << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch( const std::exception & e )
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
