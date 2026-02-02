// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022-4 RealSense, Inc. All Rights Reserved.
#pragma once

#include "dds-defines.h"

#include <fastdds/dds/domain/qos/DomainParticipantQos.hpp>

#include <rsutils/json.h>
#include <memory>
#include <functional>
#include <string>
#include <list>
#include <atomic>


namespace eprosima {
namespace fastdds {
namespace dds {
class DomainParticipant;
class DomainParticipantFactory;
}  // namespace dds
}  // namespace fastdds
namespace fastrtps {
namespace rtps {
class WriterProxyData;
class ReaderProxyData;
}  // namespace rtps
}  // namespace fastrtps
}  // namespace eprosima
#ifdef BUILD_EASYLOGGINGPP
namespace el {
namespace base {
class Storage;
}  // namespace base
}  // namespace el
#endif
namespace rsutils {
namespace string {
class slice;
}  // namespace string
}  // namespace rsutils


namespace realdds {


class dds_network_adapter_watcher;


// The starting point for any DDS interaction, a participant has a name and is the focal point for creating, destroying,
// and managing other DDS objects. It defines the DDS domain (ID) in which every other object lives.
//
class dds_participant
{
    eprosima::fastdds::dds::DomainParticipant * _participant = nullptr;
    std::shared_ptr< eprosima::fastdds::dds::DomainParticipantFactory > _participant_factory;
#ifdef BUILD_EASYLOGGINGPP
    std::shared_ptr< el::base::Storage > _elpp;
#endif

    struct listener_impl;

    rsutils::json _settings;
    std::shared_ptr< dds_network_adapter_watcher > _adapter_watcher;

public:
    dds_participant() = default;
    dds_participant( const dds_participant & ) = delete;
    ~dds_participant();

public:
    // Centralizes our default QoS settings, so that the user can then override before calling init().
    // Note that init() will try to override further with information from the settings it is passed.
    //
    class qos : public eprosima::fastdds::dds::DomainParticipantQos
    {
        using super = eprosima::fastdds::dds::DomainParticipantQos;

    public:
        qos( std::string const & participant_name );
    };

    // Return the QoS (so user won't have to actually know about the DomainParticipant)
    eprosima::fastdds::dds::DomainParticipantQos const & get_qos() const;

    // Refresh the QoS, so it will pick up any changes in the system (e.g., if adapters have changed)
    void refresh_qos();

    // Creates the underlying DDS participant and sets the QoS.
    // If callbacks are needed, set them before calling init. Note they may be called before init returns!
    // 
    // The domain ID may be -1: in this case the settings "domain" is queried and a default of 0 is used
    //
    void init( dds_domain_id did, std::string const & participant_name, rsutils::json const & settings )
    {
        qos pqos( participant_name );
        init( did, pqos, settings );
    }
    // Same, with custom QoS
    //
    void init( dds_domain_id, qos &, rsutils::json const & settings );

    bool is_valid() const { return ( nullptr != _participant ); }
    bool operator!() const { return ! is_valid(); }

    eprosima::fastdds::dds::DomainParticipant * get() const { return _participant; }
    eprosima::fastdds::dds::DomainParticipant * operator->() const { return get(); }

    rsutils::json const & settings() const { return _settings; }

    // RTPS 8.2.4.2 "Every Participant has GUID <prefix, ENTITYID_PARTICIPANT>, where the constant ENTITYID_PARTICIPANT
    //     is a special value defined by the RTPS protocol. Its actual value depends on the PSM."
    // In FastDDS, this constant is ENTITYID_RTPSParticipant = 0x1c1.
    // 
    // RTPS 8.2.4.3 "GUIDs of all Endpoints within a Participant have the same prefix.
    //     - Once the GUID of an Endpoint is known, the GUID of the Participant that contains the endpoint is also
    //     known.
    //     - The GUID of any endpoint can be deduced from the GUID of the Participant to which it belongs and its
    //     entityId."
    // 
    // The prefix is a combination of (vendor, host, process, participant-id).
    //
    dds_guid const & guid() const;

    // Returns the domain-ID for this participant
    //
    dds_domain_id domain_id() const;

    // Returns this participant's name from the QoS
    //
    rsutils::string::slice name() const;

    // Given a flow-controller name, look it up in the participant QoS and return its object
    //
    std::shared_ptr< const eprosima::fastdds::rtps::FlowControllerDescriptor > find_flow_controller( char const * name ) const;

    // Utility to create a custom GUID, to help
    //
    dds_guid create_guid();

    // Common utility: a participant is usually used as the base for printing guids, to better abbreviate
    // prefixes and even completely remove the participant part of the GUID if it's a local one...
    // Can choose to print only base (participant) or also entity id (reader/writer)
    //
    std::string print( dds_guid const & guid_to_print, bool print_entity = false ) const;

    // The participant tracks all known participants and maps them to names. This can be useful mostly for debugging
    // and, in fact, is used to show human-readable string instead of GUIDs.
    //
    // If a participant name is not found, an empty string is returned.
    //
    static std::string name_from_guid( dds_guid_prefix const& );
    //
    // The name really depends on the prefix, but passing in the whole guid is possible, too:
    //
    static std::string name_from_guid( dds_guid const & );

    class listener
    {
        friend class dds_participant;

        std::function< void( eprosima::fastrtps::rtps::WriterProxyData const & ) > _on_writer_added;
        std::function< void( dds_guid, char const* topic_name ) > _on_writer_removed;
        std::function< void( eprosima::fastrtps::rtps::ReaderProxyData const & ) > _on_reader_added;
        std::function< void( dds_guid, char const* topic_name ) > _on_reader_removed;
        std::function< void( dds_guid, char const* participant_name ) > _on_participant_added;
        std::function< void( dds_guid, char const* participant_name ) > _on_participant_removed;
        std::function< void( char const* topic_name, eprosima::fastrtps::types::DynamicType_ptr dyn_type ) > _on_type_discovery;

        listener() = default;

    public:
        listener* on_writer_added( std::function< void( eprosima::fastrtps::rtps::WriterProxyData const & ) > callback )
        {
            _on_writer_added = std::move( callback );
            return this;
        }
        listener* on_writer_removed( std::function< void( dds_guid guid, char const* topic_name ) > callback )
        {
            _on_writer_removed = std::move( callback );
            return this;
        }
        listener* on_reader_added( std::function< void( eprosima::fastrtps::rtps::ReaderProxyData const & ) > callback )
        {
            _on_reader_added = std::move( callback );
            return this;
        }
        listener* on_reader_removed( std::function< void( dds_guid guid, char const* topic_name ) > callback )
        {
            _on_reader_removed = std::move( callback );
            return this;
        }
        listener* on_participant_added( std::function< void( dds_guid guid, char const* participant_name ) > callback )
        {
            _on_participant_added = std::move( callback );
            return this;
        }
        listener* on_participant_removed( std::function< void( dds_guid guid, char const* participant_name ) > callback )
        {
            _on_participant_removed = std::move( callback );
            return this;
        }
        listener* on_type_discovery( std::function< void( char const* topic_name,
            eprosima::fastrtps::types::DynamicType_ptr dyn_type ) > callback )
        {
            _on_type_discovery = std::move( callback );
            return this;
        }
    };

    // Create an instance that gets callbacks and returns a shared_ptr. When the shared_ptr is destroyed the
    // callbacks to it will automatically stop.
    //
    // The shared_ptr must therefore be assigned and kept alive.
    // The out parameter is to enable syntax like this:
    //     _participant.create_listener( &_listener )->on_writer_removed( ... );
    // rather than:
    //     _listener = _participant.create_listener();
    //     _listener->on_writer_removed( ... );
    // The latter is still possible.
    //
    std::shared_ptr< listener > create_listener( std::shared_ptr< listener > * out = nullptr )
    {
        std::shared_ptr< listener > l( new listener );
        std::weak_ptr< listener > wl( l );
        _listeners.emplace_back( wl );
        if( out )
            *out = l;
        return l;
    }

private:
    std::list< std::weak_ptr< listener > > _listeners;
    std::shared_ptr< listener_impl > _domain_listener;
    std::atomic< uint32_t > _next_entity_id{ 0 };  // for create_guid()

    void on_writer_added( eprosima::fastrtps::rtps::WriterProxyData const & );
    void on_writer_removed( dds_guid, char const * topic_name );
    void on_reader_added( eprosima::fastrtps::rtps::ReaderProxyData const & );
    void on_reader_removed( dds_guid, char const * topic_name );
    void on_participant_added( dds_guid, char const * participant_name );
    void on_participant_removed( dds_guid, char const * participant_name );
    void on_type_discovery( char const * topic_name, eprosima::fastrtps::types::DynamicType_ptr dyn_type );
};  // class dds_participant


}  // namespace realdds
