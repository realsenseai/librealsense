// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.
// License: Apache 2.0. See LICENSE file in root directory.

#include <realdds/dds-participant.h>
#include <realdds/dds-utilities.h>
#include <realdds/dds-guid.h>
#include <realdds/dds-time.h>
#include <realdds/dds-serialization.h>
#include <realdds/dds-network-adapter-watcher.h>

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipantListener.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastrtps/types/DynamicDataFactory.h>
#include <fastdds/dds/core/status/SubscriptionMatchedStatus.hpp>
#include <fastdds/rtps/transport/UDPv4TransportDescriptor.h>

#include <rsutils/string/from.h>
#include <rsutils/string/slice.h>
#include <rsutils/json.h>

#include <map>
#include <mutex>

using namespace eprosima::fastdds::dds;
using namespace realdds;


namespace {
    std::mutex participants_mutex;
    typedef int participant_id;
    participant_id last_participants_id = 0;

    std::map< std::string, dds_guid_prefix > participant_by_name;

    struct participant_info
    {
        std::string name;
        participant_id id;
        dds_guid_prefix const prefix;

        participant_info( participant_info && ) = default;
        participant_info( char const * name_sz, dds_guid_prefix const & prefix_ )
            : prefix( prefix_ )
        {
            id = ++last_participants_id;
            if( name_sz )
            {
                name = name_sz;
                bool needs_disambiguation = false;
                if( name_sz[0] == '/' && ! name_sz[1] )
                {
                    // This is likely a ROS participant, without an actual name. There will be one per ROS process at
                    // least, so many could exist with the same name!
                    needs_disambiguation = true;
                }
                else if( participant_by_name.find( name ) != participant_by_name.end() )
                {
                    // Sometimes multiple participants (e.g., of the same executable, or on different hosts) come up
                    // with the same name. We want to differentiate those, too...
                    needs_disambiguation = true;
                }
                if( needs_disambiguation )
                {
                    // To make it readable, we use the id, converting to '/.<id>'
                    //      010f58cfc2dd816201000000.1c1  ->  /.1
                    // NOTE:
                    //      http://design.ros2.org/articles/topic_and_service_names.html#dds-topic-names
                    //      "A valid name has the following characteristics:
                    //          - First character is an alpha character ([a-z|A-Z]), tilde (~) or forward slash (/)
                    //          - Subsequent characters can be alphanumeric ([0-9|a-z|A-Z]), underscores (_), or forward slashes (/)"
                    // The '.' after the '/' is not strictly valid so can be identified as a custom addition by us...
                    name += '.';
                    name += std::to_string( id );
                }
                participant_by_name[name] = prefix;
            }
        }
        ~participant_info()
        {
            auto it = participant_by_name.find( name );
            if( it != participant_by_name.end()  &&  it->second == prefix )
                participant_by_name.erase( it );
        }
    };

    std::map< dds_guid_prefix, participant_info > participants;
}


struct dds_participant::listener_impl : public eprosima::fastdds::dds::DomainParticipantListener
{
    dds_participant & _owner;

    listener_impl() = delete;
    listener_impl( dds_participant & owner )
        : _owner( owner )
    {
    }

    virtual void on_participant_discovery( eprosima::fastdds::dds::DomainParticipant * participant,
                                           eprosima::fastrtps::rtps::ParticipantDiscoveryInfo && info ) override
    {
        switch( info.status )
        {
        case eprosima::fastrtps::rtps::ParticipantDiscoveryInfo::DISCOVERED_PARTICIPANT: {
            std::string name;
            std::string guid = rsutils::string::from( realdds::print_guid( info.info.m_guid ) );  // has to be outside the mutex
            {
                std::lock_guard< std::mutex > lock( participants_mutex );
                participant_info pinfo( info.info.m_participantName, info.info.m_guid.guidPrefix );
                name = pinfo.name;
                // When remote participant is active before this participant, listener callback can happen before
                // create_participant function returns and _owner.name() can be used.
                rsutils::string::slice owner_name;
                if( _owner.get() )
                    owner_name = _owner.name();
                LOG_DEBUG( owner_name << ": +participant '" << name << "' " << guid );
                participants.emplace( info.info.m_guid.guidPrefix, std::move( pinfo ) );
            }
            _owner.on_participant_added( info.info.m_guid, name.c_str() );
            break;
        }
        case eprosima::fastrtps::rtps::ParticipantDiscoveryInfo::REMOVED_PARTICIPANT:
        case eprosima::fastrtps::rtps::ParticipantDiscoveryInfo::DROPPED_PARTICIPANT:
            LOG_DEBUG( _owner.name() << ": -participant " << _owner.print( info.info.m_guid ) );
            _owner.on_participant_removed( info.info.m_guid, info.info.m_participantName.c_str() );
            {
                std::lock_guard< std::mutex > lock( participants_mutex );
                participants.erase( info.info.m_guid.guidPrefix );
            }
            break;
        default:
            break;
        }
    }

    virtual void on_publisher_discovery( eprosima::fastdds::dds::DomainParticipant * participant,
                                         eprosima::fastrtps::rtps::WriterDiscoveryInfo && info ) override
    {
        switch( info.status )
        {
        case eprosima::fastrtps::rtps::WriterDiscoveryInfo::DISCOVERED_WRITER:
            /* Process the case when a new publisher was found in the domain */
            LOG_DEBUG( _owner.name() << ": +writer (" << _owner.print( info.info.guid(), true ) << ") publishing "
                                     << info.info );
            _owner.on_writer_added( info.info );
            break;

        case eprosima::fastrtps::rtps::WriterDiscoveryInfo::REMOVED_WRITER:
            /* Process the case when a publisher was removed from the domain */
            LOG_DEBUG( _owner.name() << ": -writer (" << _owner.print( info.info.guid(), true ) << ") publishing '"
                                     << info.info.topicName() << "'" );
            _owner.on_writer_removed( info.info.guid(), info.info.topicName().c_str() );
            break;
        }
    }

    virtual void on_subscriber_discovery( eprosima::fastdds::dds::DomainParticipant * participant,
                                          eprosima::fastrtps::rtps::ReaderDiscoveryInfo && info ) override
    {
        switch( info.status )
        {
        case eprosima::fastrtps::rtps::ReaderDiscoveryInfo::DISCOVERED_READER:
            LOG_DEBUG( _owner.name() << ": +reader (" << _owner.print( info.info.guid(), true ) << ") of " << info.info );
            _owner.on_reader_added( info.info );
            break;

        case eprosima::fastrtps::rtps::ReaderDiscoveryInfo::REMOVED_READER:
            LOG_DEBUG( _owner.name() << ": -reader (" << _owner.print( info.info.guid(), true ) << ") of '"
                                     << info.info.topicName() << "'" );
            _owner.on_reader_removed( info.info.guid(), info.info.topicName().c_str() );
            break;
        }
    }

    virtual void on_type_discovery( eprosima::fastdds::dds::DomainParticipant * participant,
                                    const eprosima::fastrtps::rtps::SampleIdentity & request_sample_id,
                                    const eprosima::fastrtps::string_255 & topic_name,
                                    const eprosima::fastrtps::types::TypeIdentifier * identifier,
                                    const eprosima::fastrtps::types::TypeObject * object,
                                    eprosima::fastrtps::types::DynamicType_ptr dyn_type ) override
    {
        TypeSupport type_support( new eprosima::fastrtps::types::DynamicPubSubType( dyn_type ) );
        LOG_DEBUG( _owner.name() << ": +topic " << topic_name << " [" << type_support->getName() << "]" );
        _owner.on_type_discovery( topic_name.c_str(), dyn_type );
    }
};


dds_participant::qos::qos( std::string const & participant_name )
{
    name( participant_name );

    // Indicates for how much time should a remote DomainParticipant consider the local DomainParticipant to be alive.
    wire_protocol().builtin.discovery_config.leaseDuration = realdds::dds_time( 3.0 );  // seconds
    wire_protocol().builtin.discovery_config.leaseDuration_announcementperiod = realdds::dds_time( 1.5 );

    // Disable shared memory, use only UDP
    // Disabling because sometimes, after improper destruction (e.g. stopping debug) the shared memory is not opened
    // correctly and the application is stuck. eProsima is working on it. Manual solution delete shared memory files,
    // C:\ProgramData\eprosima\fastrtps_interprocess on Windows, /dev/shm on Linux
    auto udp_transport = std::make_shared< eprosima::fastdds::rtps::UDPv4TransportDescriptor >();
    // Also change the receive buffers: we deal with lots of information and, without this, we'll get dropped frames and
    // unusual behavior...
    udp_transport->sendBufferSize = 16 * 1024 * 1024;
    udp_transport->receiveBufferSize = 16 * 1024 * 1024;
    // The server may not be able to handle fragmented packets; certain hardware cannot handle more than 1500 bytes!
    // UDP default is 64K; overridable via 'udp/max-message-size'
    //udp_transport->maxMessageSize = 1470; TODO this affects reading, too! We need writer-only property for this...
    transport().use_builtin_transports = false;
    transport().user_transports.push_back( udp_transport );

    // If the user has many local interfaces (possible: I have 5!) then anything over the limit will not get
    // communicated and end-point discovery will be affected. We want to avoid this so increase the limit:
    allocation().locators.max_unicast_locators = 10;
}


void dds_participant::init( dds_domain_id domain_id, qos & pqos, rsutils::json const & settings )
{
    if( is_valid() )
    {
        DDS_THROW( runtime_error,
                   "participant is already initialized; cannot init '" << pqos.name() << "' on domain id "
                                                                       << domain_id );
    }

    if( domain_id == -1 )
    {
        // Get it from settings and default to 0
        domain_id = settings.nested( "domain" ).default_value( 0 );
    }

    _domain_listener = std::make_shared< listener_impl >( *this );

    // Initialize the timestr "start" time
    timestr{ realdds::now() };

    // The QoS given are what the user wants to use, but are supplied BEFORE any overrides from the settings
    override_participant_qos_from_json( pqos, settings );

    // NOTE: the listener callbacks we use are all specific to FastDDS and so are always enabled:
    // https://fast-dds.docs.eprosima.com/en/latest/fastdds/dds_layer/core/entity/entity.html#listener
    // We need none of the standard callbacks at this level: these can be enabled on a per-reader/-writer basis!
    StatusMask const par_mask = StatusMask::none();
    _participant_factory = DomainParticipantFactory::get_shared_instance();
    _participant
        = DDS_API_CALL( _participant_factory->create_participant( domain_id, pqos, _domain_listener.get(), par_mask ) );

    if( ! _participant )
        DDS_THROW( runtime_error, "failed creating participant " << pqos.name() << " on domain id " << domain_id );

    if( settings.is_object() )
        _settings = settings;
    else if( settings.is_null() )
        _settings = rsutils::json::object();
    else
        DDS_THROW( runtime_error, "provided settings are invalid: " << settings );

    _adapter_watcher = std::make_shared< dds_network_adapter_watcher >(
        [this]
        {
            LOG_DEBUG( name() << ": refreshing QoS" );
            refresh_qos();
        } );

    LOG_DEBUG( "participant " << realdds::print_raw_guid( guid() ) << " " << pqos << "\nis up on domain " << domain_id
                              << " from settings: " << std::setw( 4 ) << _settings );

#ifdef BUILD_EASYLOGGINGPP
    // DDS participant destruction happens when all contexts are done with it but, in some situations (e.g., Python), it
    // means that it may happen last -- even after ELPP has already been destroyed! So, just like we keep the participant
    // factory alive above, we want to keep the logging mechanism alive as long as a participant is!
    // (because a participant has callbacks, and callbacks use the log)
    _elpp = el::base::elStorage;
#endif
}


dds_participant::~dds_participant()
{
    if( is_valid() )
    {
        if( _participant->has_active_entities() )
        {
            LOG_DEBUG( "participant has active entities!, deleting them all.." );
            DDS_API_CALL_NO_THROW( _participant->delete_contained_entities() );
        }

        DDS_API_CALL_NO_THROW( _participant_factory->delete_participant( _participant ) );
    }
}


dds_guid const & dds_participant::guid() const
{
    return get()->guid();
}


dds_domain_id dds_participant::domain_id() const
{
    return get()->get_domain_id();
}


eprosima::fastdds::dds::DomainParticipantQos const & dds_participant::get_qos() const
{
    return get()->get_qos();
}


rsutils::string::slice dds_participant::name() const
{
    auto & string_255 = get_qos().name();
    return rsutils::string::slice( string_255.c_str(), string_255.size() );
}


std::shared_ptr< const eprosima::fastdds::rtps::FlowControllerDescriptor >
dds_participant::find_flow_controller( char const * name ) const
{
    for( auto & controller : get_qos().flow_controllers() )
    {
        if( ! strcmp( name, controller->name ) )
            return controller;
    }
    return {};
}


void dds_participant::refresh_qos()
{
    get()->set_qos( get_qos() );
}


std::string dds_participant::print( dds_guid const & guid_to_print, bool print_entity ) const
{
    return rsutils::string::from( realdds::print_guid( guid_to_print, guid(), print_entity ) );
}


/*static*/ std::string dds_participant::name_from_guid( dds_guid_prefix const & pref )
{
    std::string participant;
    std::lock_guard< std::mutex > lock( participants_mutex );
    auto it = participants.find( pref );
    if( it != participants.end() )
        participant = it->second.name;
    return participant;
}


/*static*/ std::string dds_participant::name_from_guid( dds_guid const & guid )
{
    return name_from_guid( guid.guidPrefix );
}


void dds_participant::on_writer_added( eprosima::fastrtps::rtps::WriterProxyData const & data )
{
    for( auto & wl : _listeners )
    {
        if( auto l = wl.lock() )
        {
            if( l->_on_writer_added )
                l->_on_writer_added( data );
        }
    }
}


void dds_participant::on_writer_removed( dds_guid guid, char const * topic_name )
{
    for( auto & wl : _listeners )
    {
        if( auto l = wl.lock() )
        {
            if( l->_on_writer_removed )
                l->_on_writer_removed( guid, topic_name );
        }
    }
}


void dds_participant::on_reader_added( eprosima::fastrtps::rtps::ReaderProxyData const & data )
{
    for( auto & wl : _listeners )
    {
        if( auto l = wl.lock() )
        {
            if( l->_on_reader_added )
                l->_on_reader_added( data );
        }
    }
}


void dds_participant::on_reader_removed( dds_guid guid, char const * topic_name )
{
    for( auto & wl : _listeners )
    {
        if( auto l = wl.lock() )
        {
            if( l->_on_reader_removed )
                l->_on_reader_removed( guid, topic_name );
        }
    }
}


void dds_participant::on_participant_added( dds_guid guid, char const * participant_name )
{
    for( auto & wl : _listeners )
    {
        if( auto l = wl.lock() )
        {
            if( l->_on_participant_added )
                l->_on_participant_added( guid, participant_name );
        }
    }
}


void dds_participant::on_participant_removed( dds_guid guid, char const * participant_name )
{
    for( auto & wl : _listeners )
    {
        if( auto l = wl.lock() )
        {
            if( l->_on_participant_removed )
                l->_on_participant_removed( guid, participant_name );
        }
    }
}


void dds_participant::on_type_discovery( char const * topic_name, eprosima::fastrtps::types::DynamicType_ptr dyn_type )
{
    for( auto & wl : _listeners )
    {
        if( auto l = wl.lock() )
        {
            if( l->_on_type_discovery )
                l->_on_type_discovery( topic_name, dyn_type );
        }
    }
}


dds_guid dds_participant::create_guid()
{
    using eprosima::fastrtps::rtps::octet;

    // RTPS 9.3.1.2 "the EntityId_t is the unique identification of the Endpoint within the Participant. The PSM
    //      maps the EntityId_t to the following structure:
    //          struct EntityId_t {
    //              octet entityKey[3];
    //              octet entityKind;
    //          };
    //      The reserved constant ENTITYID_UNKNOWN defined by the PIM is mapped to :
    //          #define ENTITYID_UNKNOWN {{0x00, 0x00, 0x00}, 0x00}
    //      The entityKind field within EntityId_t encodes the kind of Entity (Participant, Reader, Writer, Reader
    //      Group, Writer Group) and whether the Entity is a built-in Entity (fully pre-defined by the
    //      Protocol, automatically instantiated), a user-defined Entity (defined by the Protocol, but
    //      instantiated by the user only as needed by the application) or a vendor-specific Entity (defined by a
    //      vendor-specific extension to the Protocol, can therefore be ignored by another vendors
    //      implementation)
    // 
    //      ...
    // 
    //      The information on whether the object is a built-in entity, a vendor-specific entity, or a user-defined
    //      entity is encoded in the two most-significant bits of the entityKind. These two bits are set to:
    //          - 00 for user-defined entities
    //          - 11 for built-in entities
    //          - 01 for vendor-specific entities"
    // 
    // The FastDDS implementations use 0x01 (vendor-specific). We use 0x00 (user-defined).
    // See DomainParticipantImpl::create_instance_handle and RTPSParticipantImpl::get_new_entity_id
    //
    ++_next_entity_id;
    auto e = guid().entityId;
    e.value[3] = 0x00;  // user-defined; fastdds sets this to 0x01
    e.value[2] = static_cast< octet >( _next_entity_id & 0xFF );
    e.value[1] = static_cast< octet >( ( _next_entity_id >> 8 ) & 0xFF );
    e.value[0] = static_cast< octet >( ( _next_entity_id >> 16 ) & 0xFF );
    return dds_guid( guid().guidPrefix, e );
}
