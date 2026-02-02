// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023-4 RealSense, Inc. All Rights Reserved.

#include <realdds/dds-serialization.h>
#include <realdds/dds-utilities.h>
#include <realdds/dds-time.h>
#include <realdds/dds-guid.h>

#include <realdds/dds-participant.h>
#include <fastdds/dds/domain/DomainParticipant.hpp>

#include <fastdds/rtps/writer/WriterDiscoveryInfo.h>
#include <fastdds/rtps/reader/ReaderDiscoveryInfo.h>

#include <fastdds/dds/domain/qos/DomainParticipantQos.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/rtps/transport/UDPTransportDescriptor.h>

#include <rsutils/string/from.h>
#include <rsutils/string/nocase.h>
#include <rsutils/json.h>
#include <rsutils/ios/word-format.h>
#include <rsutils/ios/field.h>
#include <rsutils/ios/indent.h>

using rsutils::json;
using rsutils::ios::field;


#define field_changed( Inst, Field, Default ) \
    ( Inst.Field != Default.Field ? "*" : "" )

// Takes a member of a struct 'Inst.M' and outputs "<name> <value>[*]" where the '*' is there if the value differs from
// the default (contained in a variable _def).
// 
#define dump_field_as( Inst, Field, Converter )                                                                        \
        field::separator << rsutils::ios::dash_case( #Field )                                                          \
     << field::value << Converter( Inst.Field )                                                                        \
     << field_changed( Inst, Field, _def )
#define dump_field( Inst, Field )                                                                                      \
    dump_field_as( Inst, Field, /* no converter */ )


namespace eprosima {
namespace fastdds {
namespace dds {


std::ostream & operator<<( std::ostream & os, ReliabilityQosPolicyKind kind )
{
    switch( kind )
    {
    case eprosima::fastdds::dds::BEST_EFFORT_RELIABILITY_QOS:
        return os << "best-effort";
    case eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS:
        return os << "reliable";
    }
    return os << (int)kind;
}


std::ostream & operator<<( std::ostream & os, ReliabilityQosPolicy const & qos )
{
    return os << field::separator << qos.kind;
}


std::ostream & operator<<( std::ostream & os, DurabilityQosPolicyKind kind )
{
    switch( kind )
    {
    case eprosima::fastdds::dds::VOLATILE_DURABILITY_QOS:
        return os << "volatile";
    case eprosima::fastdds::dds::TRANSIENT_LOCAL_DURABILITY_QOS:
        return os << "transient-local";
    case eprosima::fastdds::dds::TRANSIENT_DURABILITY_QOS:
        return os << "transient";
    case eprosima::fastdds::dds::PERSISTENT_DURABILITY_QOS:
        return os << "persistent";
    }
    return os << (int)kind;
}


std::ostream & operator<<( std::ostream & os, DurabilityQosPolicy const & qos )
{
    return os << field::separator << qos.kind;
}


std::ostream & operator<<( std::ostream & os, HistoryQosPolicyKind kind )
{
    switch( kind )
    {
    case eprosima::fastdds::dds::KEEP_LAST_HISTORY_QOS:
        return os << "keep-last";
    case eprosima::fastdds::dds::KEEP_ALL_HISTORY_QOS:
        return os << "keep-all";
    }
    return os << (int) kind;
}


std::ostream & operator<<( std::ostream & os, HistoryQosPolicy const & qos )
{
    os << field::separator << "kind" << field::value << qos.kind;
    os << field::separator << "depth" << field::value << qos.depth;
    return os;
}


std::ostream & operator<<( std::ostream & os, LivelinessQosPolicyKind kind )
{
    switch( kind )
    {
    case eprosima::fastdds::dds::AUTOMATIC_LIVELINESS_QOS:
        return os << "automatic";
    case eprosima::fastdds::dds::MANUAL_BY_PARTICIPANT_LIVELINESS_QOS:
        return os << "by-participant";
    case eprosima::fastdds::dds::MANUAL_BY_TOPIC_LIVELINESS_QOS:
        return os << "by-topic";
    }
    return os << (int) kind;
}


std::ostream & operator<<( std::ostream & os, LivelinessQosPolicy const & qos )
{
    os << qos.kind;
    if( qos.lease_duration != eprosima::fastrtps::c_TimeInfinite )
        os << "/" << rsutils::string::from( qos.lease_duration.to_ns() / 1e9 ) << 's';
    if( qos.kind == eprosima::fastdds::dds::AUTOMATIC_LIVELINESS_QOS
        && qos.announcement_period != eprosima::fastrtps::c_TimeInfinite )
        os << "/" << rsutils::string::from( qos.announcement_period.to_ns() / 1e9 ) << 's';
    return os;
}


std::ostream & operator<<( std::ostream & os, DataSharingQosPolicy const & qos )
{
    return os;
}


std::ostream & operator<<( std::ostream & os, RTPSEndpointQos const & qos )
{
    return os;
}


std::ostream & operator<<( std::ostream & os, TransportConfigQos const & qos )
{
    for( auto const & transport : qos.user_transports )
    {
        if( auto udp = std::dynamic_pointer_cast< rtps::UDPTransportDescriptor >( transport ) )
            os << field::separator << "udp" << field::group() << *udp;
        else if( auto socket
                 = std::dynamic_pointer_cast< rtps::SocketTransportDescriptor >( transport ) )
            os << field::separator << "socket" << field::group() << *socket;
        else
            os << field::separator << "unknown" << field::group() << *transport;
    }
    TransportConfigQos const _def;
    os << dump_field( qos, use_builtin_transports );
    os << dump_field( qos, listen_socket_buffer_size );
    os << dump_field( qos, send_socket_buffer_size );
    return os;
}


std::ostream & operator<<( std::ostream & os, WireProtocolConfigQos const & qos )
{
    WireProtocolConfigQos _def;
    os << dump_field( qos, participant_id );
    if( qos.prefix != realdds::dds_guid_prefix() )
        os << dump_field_as( qos, prefix, realdds::print_raw_guid_prefix );
    os << field::separator << "builtin" << field::group() << qos.builtin;

    //fastrtps::rtps::PortParameters port;
    //fastrtps::rtps::ThroughputControllerDescriptor throughput_controller;
    //rtps::LocatorList default_unicast_locator_list;
    //rtps::LocatorList default_multicast_locator_list;
    //rtps::ExternalLocators default_external_unicast_locators;
    //bool ignore_non_matching_locators = false;
    return os;
}


std::ostream & operator<<( std::ostream & os, ParticipantResourceLimitsQos const & qos )
{
    ParticipantResourceLimitsQos _def;

    os << field::separator << "locators" << field::group() << qos.locators;
    //ResourceLimitedContainerConfig participants;
    //ResourceLimitedContainerConfig readers;
    //ResourceLimitedContainerConfig writers;
    //SendBuffersAllocationAttributes send_buffers;
    //VariableLengthDataLimits data_limits;
    //fastdds::rtps::ContentFilterProperty::AllocationConfiguration content_filter;
    return os;
}


std::ostream & operator<<( std::ostream & os, PropertyPolicyQos const & qos )
{
    for( auto & prop : qos.properties() )
        os << field::separator << prop.name() << field::value << prop.value();
    return os;
}


std::ostream & operator<<( std::ostream & os, DomainParticipantQos const & qos )
{
    os << "'" << qos.name() << "'";
    os << rsutils::ios::indent();
    if( ! qos.flow_controllers().empty() )
    {
        field::group g;
        os << field::separator << "flow-controllers" << g;
        for( auto const & controller : qos.flow_controllers() )
            os << field::separator << *controller;
    }
    //EntityFactoryQosPolicy entity_factory_;
    os << field::separator << "allocation" << field::group() << qos.allocation();
    os << field::separator << "transport" << field::group() << qos.transport();
    os << field::separator << "wire-protocol" << field::group() << qos.wire_protocol();
    if( ! qos.properties().properties().empty() )
        os << field::separator << "properties" << field::group() << qos.properties();
    return os << rsutils::ios::unindent();
}


std::ostream & operator<<( std::ostream & os, DataWriterQos const & qos )
{
    os << /*field::separator << "reliability" << field::group() <<*/ qos.reliability();
    if( ! ( qos.durability() == eprosima::fastdds::dds::DurabilityQosPolicy() ) )
        os << /*field::separator << "durability" << field::group() <<*/ qos.durability();
    if( ! ( qos.liveliness() == eprosima::fastdds::dds::LivelinessQosPolicy() ) )
        os << field::separator << "liveliness" << field::value << qos.liveliness();
    if( ! ( qos.history() == eprosima::fastdds::dds::HistoryQosPolicy() ) )
        os << field::separator << "history" << field::group() << qos.history();
    if( qos.publish_mode().flow_controller_name
        && qos.publish_mode().flow_controller_name != fastdds::rtps::FASTDDS_FLOW_CONTROLLER_DEFAULT )
        os << field::separator << "flow-control" << field::value << "'" << qos.publish_mode().flow_controller_name << "'";
    return os;

    //DurabilityServiceQosPolicy durability_service_;
    //DeadlineQosPolicy deadline_;
    //LatencyBudgetQosPolicy latency_budget_;
    //DestinationOrderQosPolicy destination_order_;
    //HistoryQosPolicy history_;
    //ResourceLimitsQosPolicy resource_limits_;
    //TransportPriorityQosPolicy transport_priority_;
    //LifespanQosPolicy lifespan_;
    //UserDataQosPolicy user_data_;
    //OwnershipQosPolicy ownership_;
    //OwnershipStrengthQosPolicy ownership_strength_;
    //WriterDataLifecycleQosPolicy writer_data_lifecycle_;
    //PublishModeQosPolicy publish_mode_;
    //DataRepresentationQosPolicy representation_;
    //PropertyPolicyQos properties_;
    //RTPSReliableWriterQos reliable_writer_qos_;
    //RTPSEndpointQos endpoint_;
    //WriterResourceLimitsQos writer_resource_limits_;
    //fastrtps::rtps::ThroughputControllerDescriptor throughput_controller_;
    //DataSharingQosPolicy data_sharing_;
}


}  // namespace dds
namespace rtps {


std::ostream & operator<<( std::ostream & os, TransportDescriptorInterface const & transport )
{
    os << field::separator << "max-initial-peers-range" << field::value << transport.max_initial_peers_range();
    os << field::separator << "max-message-size" << field::value << transport.max_message_size();
    return os;
}


std::ostream & operator<<( std::ostream & os, SocketTransportDescriptor const & socket )
{
    os << field::separator << "send-buffer-size" << field::value << socket.sendBufferSize;
    os << field::separator << "receive-buffer-size" << socket.receiveBufferSize;
    os << field::separator << "time-to-live" << unsigned( socket.TTL );
    if( ! socket.interfaceWhiteList.empty() )
    {
        os << field::separator << "whitelist";
        for( auto & ip : socket.interfaceWhiteList )
            os << ' ' << ip;
    }
    return operator<<( os, static_cast< TransportDescriptorInterface const & >( socket ) );
}


std::ostream & operator<<( std::ostream & os, UDPTransportDescriptor const & udp )
{
    os << field::separator << "non-blocking-send" << field::value << udp.non_blocking_send;
    return operator<<( os, static_cast< SocketTransportDescriptor const & >( udp ) );
}


std::ostream & operator<<( std::ostream & os, FlowControllerSchedulerPolicy scheduler )
{
    switch( scheduler )
    {
    case FlowControllerSchedulerPolicy::FIFO:
        os << "FIFO";
        break;
    case FlowControllerSchedulerPolicy::ROUND_ROBIN:
        os << "ROUND-ROBIN";
        break;
    case FlowControllerSchedulerPolicy::HIGH_PRIORITY:
        os << "HIGH-PRIORITY";
        break;
    case FlowControllerSchedulerPolicy::PRIORITY_WITH_RESERVATION:
        os << "PRIORITY-WITH-RESERVATION";
        break;
    }
    return os;
}


void to_json( json & j, FlowControllerSchedulerPolicy scheduler )
{
    j = ( rsutils::string::from() << scheduler ).str();
}


void from_json( json const & j, FlowControllerSchedulerPolicy & scheduler )
{
    if( ! j.is_string() )
        throw rsutils::json::type_error::create( 317, "scheduler should be a string", &j );
    auto str = j.string_ref();
    if( str == "FIFO" )
        scheduler = FlowControllerSchedulerPolicy::FIFO;
    else if( str == "HIGH-PRIORITY" )
        scheduler = FlowControllerSchedulerPolicy::HIGH_PRIORITY;
    else if( str == "ROUND-ROBIN" )
        scheduler = FlowControllerSchedulerPolicy::ROUND_ROBIN;
    else if( str == "PRIORITY-WITH-RESERVATION" )
        scheduler = FlowControllerSchedulerPolicy::PRIORITY_WITH_RESERVATION;
    else
        throw json::type_error::create( 317, "unknown scheduler value '" + str + "'", &j );
}


std::ostream & operator<<( std::ostream & os, FlowControllerDescriptor const & controller )
{
    if( ! controller.name )
    {
        os << 'null';
    }
    else
    {
        os << "'" << controller.name << "'";
        os << " " << controller.scheduler;
        os << " " << controller.max_bytes_per_period << " bytes every " << controller.period_ms << " ms";
    }
    return os;
}


}  // namespace rtps
}  // namespace fastdds
}  // namespace eprosima


namespace eprosima {
namespace fastrtps {


// Allow j["key"] = qos.lease_duration;
void to_json( json & j, Duration_t const & duration )
{
    if( duration == c_TimeInfinite )
        j = "infinite";
    else if( duration == c_TimeInvalid )
        j = "invalid";
    else
        j = realdds::time_to_double( duration );
}


// Allow j.get< eprosima::fastrtps::Duration_t >();
void from_json( json const & j, Duration_t & duration )
{
    if( j.is_string() )
    {
        auto & s = j.string_ref();
        if( rsutils::string::nocase_equal( s, "infinite" ) )
            duration = c_TimeInfinite;
        else if( rsutils::string::nocase_equal( s, "invalid" ) )
            duration = c_TimeInvalid;
        else
            throw json::type_error::create( 317, "unknown duration value '" + s + "'", &j );
    }
    else
        duration = realdds::dds_time( j.get< double >() );
}


namespace rtps {


std::ostream & operator<<( std::ostream & os, RemoteLocatorsAllocationAttributes const & qos )
{
    RemoteLocatorsAllocationAttributes _def;

    os << dump_field( qos, max_unicast_locators );
    os << dump_field( qos, max_multicast_locators );
    return os;
}


std::ostream & operator<<( std::ostream & os, WriterProxyData const & info )
{
    os << "'" << info.topicName() << "' " << field::group() << realdds::print_writer_info( info );
    return os;
}


std::ostream & operator<<( std::ostream & os, ReaderProxyData const & info )
{
    os << "'" << info.topicName() << "' " << field::group() << realdds::print_reader_info( info );
    return os;
}


std::ostream & operator<<( std::ostream & os, DiscoverySettings const & qos )
{
    DiscoverySettings const _def;
    //DiscoveryProtocol_t discoveryProtocol = DiscoveryProtocol_t::SIMPLE;
    //bool use_SIMPLE_EndpointDiscoveryProtocol = true;
    //bool use_STATIC_EndpointDiscoveryProtocol = false;
    os << dump_field_as( qos, leaseDuration, json );
    os << field::separator << "announcement-period" << field::value << json( qos.leaseDuration_announcementperiod )
       << field_changed( qos, leaseDuration_announcementperiod, _def );
    //InitialAnnouncementConfig initial_announcements;
    //SimpleEDPAttributes m_simpleEDP;
    //PDPFactory m_PDPfactory{};
    //Duration_t discoveryServer_client_syncperiod = { 0, 450 * 1000000 }; // 450 milliseconds
    //eprosima::fastdds::rtps::RemoteServerList_t m_DiscoveryServers;
    //ParticipantFilteringFlags_t ignoreParticipantFlags = ParticipantFilteringFlags::NO_FILTER;
    return os;
}


std::ostream & operator<<( std::ostream & os, BuiltinAttributes const & qos )
{
    BuiltinAttributes _def;
    os << field::separator << "discovery-config" << field::group() << qos.discovery_config;
    //bool use_WriterLivelinessProtocol = true;
    //TypeLookupSettings typelookup_config;
    //LocatorList_t metatrafficUnicastLocatorList;
    //LocatorList_t metatrafficMulticastLocatorList;
    //fastdds::rtps::ExternalLocators metatraffic_external_unicast_locators;
    //LocatorList_t initialPeersList;
    //MemoryManagementPolicy_t readerHistoryMemoryPolicy =
    //    MemoryManagementPolicy_t::PREALLOCATED_WITH_REALLOC_MEMORY_MODE;
    //uint32_t readerPayloadSize = BUILTIN_DATA_MAX_SIZE;
    //MemoryManagementPolicy_t writerHistoryMemoryPolicy =
    //    MemoryManagementPolicy_t::PREALLOCATED_WITH_REALLOC_MEMORY_MODE;
    //uint32_t writerPayloadSize = BUILTIN_DATA_MAX_SIZE;
    //uint32_t mutation_tries = 100u;
    os << dump_field( qos, avoid_builtin_multicast );
    return os;
}


}  // namespace rtps
}  // namespace fastrtps
}  // namespace eprosima


namespace realdds {


std::ostream & operator<<( std::ostream & os, print_writer_info const & pwi )
{
    auto & info = pwi.info;
    os << /*field::separator << "type" <<*/ field::value << info.typeName();
    os << /*field::separator << "reliability" << field::group() <<*/ info.m_qos.m_reliability;
    if( ! ( info.m_qos.m_durability == eprosima::fastdds::dds::DurabilityQosPolicy() ) )
        os << /*field::separator << "durability" << field::group() <<*/ info.m_qos.m_durability;
    if( ! ( info.m_qos.m_liveliness == eprosima::fastdds::dds::LivelinessQosPolicy() ) )
        os << field::separator << "liveliness" << field::value << info.m_qos.m_liveliness;
    if( info.m_qos.m_publishMode.flow_controller_name
        && info.m_qos.m_publishMode.flow_controller_name != eprosima::fastdds::rtps::FASTDDS_FLOW_CONTROLLER_DEFAULT )
        os << field::separator << "flow-controller" << field::value << "'"
           << info.m_qos.m_publishMode.flow_controller_name << "'";
    return os;
}


std::ostream & operator<<( std::ostream & os, print_reader_info const & pri )
{
    auto & info = pri.info;
    os << /*field::separator << "type" <<*/ field::value << info.typeName();
    os << /*field::separator << "reliability" << field::group() <<*/ info.m_qos.m_reliability;
    if( ! ( info.m_qos.m_durability == eprosima::fastdds::dds::DurabilityQosPolicy() ) )
        os << /*field::separator << "durability" << field::group() <<*/ info.m_qos.m_durability;
    return os;
}


eprosima::fastdds::dds::ReliabilityQosPolicyKind reliability_kind_from_string( std::string const & s )
{
    if( s == "best-effort" )
        return eprosima::fastdds::dds::BEST_EFFORT_RELIABILITY_QOS;
    if( s == "reliable" )
        return eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS;
    DDS_THROW( runtime_error, "invalid reliability kind '" << s << "'" );
}


eprosima::fastdds::dds::DurabilityQosPolicyKind durability_kind_from_string( std::string const & s )
{
    if( s == "volatile" )
        return eprosima::fastdds::dds::VOLATILE_DURABILITY_QOS;
    if( s == "transient-local" )
        return eprosima::fastdds::dds::TRANSIENT_LOCAL_DURABILITY_QOS;
    if( s == "transient" )
        return eprosima::fastdds::dds::TRANSIENT_DURABILITY_QOS;
    if( s == "persistent" )
        return eprosima::fastdds::dds::PERSISTENT_DURABILITY_QOS;
    DDS_THROW( runtime_error, "invalid durability kind '" << s << "'" );
}


eprosima::fastdds::dds::HistoryQosPolicyKind history_kind_from_string( std::string const & s )
{
    if( s == "keep-last" )
        return eprosima::fastdds::dds::KEEP_LAST_HISTORY_QOS;
    if( s == "keep-all" )
        return eprosima::fastdds::dds::KEEP_ALL_HISTORY_QOS;
    DDS_THROW( runtime_error, "invalid history kind '" << s << "'" );
}


eprosima::fastdds::dds::LivelinessQosPolicyKind liveliness_kind_from_string( std::string const & s )
{
    if( s == "automatic" )
        return eprosima::fastdds::dds::AUTOMATIC_LIVELINESS_QOS;
    if( s == "by-participant" )
        return eprosima::fastdds::dds::MANUAL_BY_PARTICIPANT_LIVELINESS_QOS;
    if( s == "by-topic" )
        return eprosima::fastdds::dds::MANUAL_BY_TOPIC_LIVELINESS_QOS;
    DDS_THROW( runtime_error, "invalid liveliness kind '" << s << "'" );
}


eprosima::fastrtps::rtps::MemoryManagementPolicy_t history_memory_policy_from_string( std::string const & s )
{
    if( s == "preallocated" )
        return eprosima::fastrtps::rtps::PREALLOCATED_MEMORY_MODE;
    if( s == "preallocated-with-realloc" )
        return eprosima::fastrtps::rtps::PREALLOCATED_WITH_REALLOC_MEMORY_MODE;
    if( s == "dynamic-reserve" )
        return eprosima::fastrtps::rtps::DYNAMIC_RESERVE_MEMORY_MODE;
    if( s == "dynamic-reusable" )
        return eprosima::fastrtps::rtps::DYNAMIC_REUSABLE_MEMORY_MODE;
    DDS_THROW( runtime_error, "invalid history memory policy '" << s << "'" );
}


void override_reliability_qos_from_json( eprosima::fastdds::dds::ReliabilityQosPolicy & qos, json const & j )
{
    if( j.is_string() )
        qos.kind = reliability_kind_from_string( j.string_ref() );
    else if( j.is_object() )
    {
        if( auto kind_j = j.nested( "kind", &json::is_string ) )
            qos.kind = reliability_kind_from_string( kind_j.string_ref() );
    }
}


void override_durability_qos_from_json( eprosima::fastdds::dds::DurabilityQosPolicy & qos, json const & j )
{
    if( j.is_string() )
        qos.kind = durability_kind_from_string( j.get_ref< const json::string_t & >() );
    else if( j.is_object() )
    {
        if( auto kind_j = j.nested( "kind", &json::is_string ) )
            qos.kind = durability_kind_from_string( kind_j.string_ref() );
    }
}


void override_history_qos_from_json( eprosima::fastdds::dds::HistoryQosPolicy & qos, json const & j )
{
    if( j.is_number_unsigned() )
        qos.depth = j.get< int32_t >();
    else if( j.is_object() )
    {
        if( auto kind_j = j.nested( "kind", &json::is_string ) )
            qos.kind = history_kind_from_string( kind_j.string_ref() );
        j.nested( "depth" ).get_ex( qos.depth );
    }
}


void override_liveliness_qos_from_json( eprosima::fastdds::dds::LivelinessQosPolicy & qos, json const & j )
{
    if( j.is_object() )
    {
        if( auto kind = j.nested( "kind" ) )
        {
            if( kind.is_string() )
                qos.kind = liveliness_kind_from_string( kind.string_ref() );
            else
                DDS_THROW( runtime_error, "liveliness kind not a string: " << kind );
        }

        if( auto lease = j.nested( "lease-duration" ) )
        {
            if( lease.is_null() )
                qos.lease_duration = eprosima::fastdds::dds::LivelinessQosPolicy().lease_duration;
            else
                lease.get_to( qos.lease_duration );
        }

        if( auto announce = j.nested( "announcement-period" ) )
        {
            if( announce.is_null() )
                qos.announcement_period = eprosima::fastdds::dds::LivelinessQosPolicy().announcement_period;
            else
                announce.get_to( qos.announcement_period );
        }
    }
}


void override_data_sharing_qos_from_json( eprosima::fastdds::dds::DataSharingQosPolicy & qos, json const & j )
{
    if( j.is_boolean() )
    {
        if( j.get< bool >() )
            qos.automatic();
        else
            qos.off();
    }
    else if( j.exists() )
    {
        DDS_THROW( runtime_error, "data-sharing must be a boolean (off/automatic); got " << j );
    }
}


void override_endpoint_qos_from_json( eprosima::fastdds::dds::RTPSEndpointQos & qos, json const & j )
{
    if( j.is_object() )
    {
        if( auto policy_j = j.nested( "history-memory-policy", &json::is_string ) )
            qos.history_memory_policy = history_memory_policy_from_string( policy_j.string_ref() );
    }
}


void override_publish_mode_qos_from_json( eprosima::fastdds::dds::PublishModeQosPolicy & qos, json const & j, dds_participant const & participant )
{
    // At this time, this must be either missing, or a string for the flow-controller name
    if( ! j.exists() )
        return;
    if( auto j_name = j.nested( "flow-control" ) )
    {
        if( j_name.is_null() )
        {
            qos = eprosima::fastdds::dds::PublishModeQosPolicy();
        }
        else if( j_name.is_string() )
        {
            if( auto controller = participant.find_flow_controller( j_name.string_ref().c_str() ) )
            {
                qos.kind = eprosima::fastdds::dds::ASYNCHRONOUS_PUBLISH_MODE;
                qos.flow_controller_name = controller->name;
            }
            else
            {
                DDS_THROW( runtime_error, "invalid flow-control name in publish-mode; got " << j_name );
            }
        }
        else
            DDS_THROW( runtime_error, "flow-control must be a name; got " << j_name );
    }
}


void override_flow_controller_from_json( eprosima::fastdds::rtps::FlowControllerDescriptor & controller, json const & j )
{
    if( ! j.exists() )
        return;
    if( ! j.is_object() )
        DDS_THROW( runtime_error, "flow-controller must be an object; got " << j );

    j.nested( "max-bytes-per-period", &json::is_number_integer ).get_ex( controller.max_bytes_per_period );
    j.nested( "period-ms", &json::is_number_integer ).get_ex( controller.period_ms );
    j.nested( "scheduler" ).get_ex( controller.scheduler );
}


static bool parse_ip_list( json const & j, std::string const & key, std::vector< std::string > * output )
{
    if( auto whitelist_j = j.nested( key ) )
    {
        if( ! whitelist_j.is_array() )
            return false;
        for( auto & ip : whitelist_j )
        {
            if( ! ip.is_string() )
                return false;
            if( output )
                output->push_back( ip.string_ref() );
        }
    }
    return true;
}


static void override_udp_settings( eprosima::fastdds::rtps::UDPTransportDescriptor & udp, json const & j )
{
    j.nested( "send-buffer-size" ).get_ex( udp.sendBufferSize );
    j.nested( "receive-buffer-size" ).get_ex( udp.receiveBufferSize );
    j.nested( "max-message-size" ).get_ex( udp.maxMessageSize );
    if( ! parse_ip_list( j, "whitelist", &udp.interfaceWhiteList ) )
        LOG_WARNING( "invalid UDP whitelist in settings" );
    uint8_t tmp_ttl(0);
    j.nested( "ttl" ).get_ex( tmp_ttl );
    if( tmp_ttl != 0 )  // 0 means "system default"
        udp.TTL = tmp_ttl;
}


void override_participant_qos_from_json( eprosima::fastdds::dds::DomainParticipantQos & qos, json const & j )
{
    if( ! j.is_object() )
        return;
    auto & wp = qos.wire_protocol();
    j.nested( "participant-id" ).get_ex( wp.participant_id );
    j.nested( "lease-duration" ).get_ex( wp.builtin.discovery_config.leaseDuration );  // must be > announcement period!
    j.nested( "announcement-period" ).get_ex( wp.builtin.discovery_config.leaseDuration_announcementperiod );
    
    j.nested( "max-unicast-locators" ).get_ex( qos.allocation().locators.max_unicast_locators );

    j.nested( "use-builtin-transports" ).get_ex( qos.transport().use_builtin_transports );
    if( auto udp_j = j.nested( "udp" ) )
    {
        for( auto t : qos.transport().user_transports )
            if( auto udp_t = std::dynamic_pointer_cast< eprosima::fastdds::rtps::UDPTransportDescriptor >( t ) )
            {
                override_udp_settings( *udp_t, udp_j );
                break;
            }
    }

    if( auto max_bytes_j = j.nested( "max-out-message-bytes", &json::is_number_unsigned ) )
    {
        // Override maximum message size, in bytes, for SENDING messages on RTPS.
        // If you need both send & receive, override udp/max-message-size.
        auto & props = qos.properties().properties();
        auto value = std::to_string( max_bytes_j.get< uint32_t >() );
        bool found = false;
        for( auto & prop : props )
        {
            if( prop.name() == "fastdds.max_message_size" )
            {
                prop.value() = value;
                found = true;
                break;
            }
        }
        if( ! found )
            props.emplace_back( "fastdds.max_message_size", value );
    }

    if( auto controllers_j = j.nested( "flow-controllers", &json::is_object ) )
    {
        class controller_wrapper : public eprosima::fastdds::rtps::FlowControllerDescriptor
        {
            std::string _name;

        public:
            controller_wrapper( std::string const & controller_name )
                : _name( controller_name )
            {
                this->name = _name.c_str();
            }
        };
        for( auto it : controllers_j.get_json().items() )
        {
            auto p_controller = std::make_shared< controller_wrapper >( it.key() );
            // Right now all the values are at their defaults; override them:
            override_flow_controller_from_json( *p_controller, it.value() );
            qos.flow_controllers().push_back( p_controller );
        }
    }
}


}  // namespace realdds
