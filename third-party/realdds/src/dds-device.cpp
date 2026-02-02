// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#include <realdds/dds-device.h>
#include <realdds/dds-participant.h>
#include <realdds/dds-topic-reader.h>
#include <realdds/dds-topic-writer.h>
#include <realdds/topics/dds-topic-names.h>
#include <realdds/dds-embedded-filter.h>
#include "dds-device-impl.h"

#include <rsutils/time/timer.h>
#include <rsutils/json.h>

using rsutils::json;


namespace realdds {


dds_device::dds_device( std::shared_ptr< dds_participant > const & participant, topics::device_info const & info )
    : _impl( std::make_shared< dds_device::impl >( participant, info ) )
{
    LOG_DEBUG( "[" << debug_name() << "] device created on: " << info.topic_root() );
}


bool dds_device::is_ready() const
{
    return _impl->is_ready();
}


bool dds_device::is_online() const
{
    return _impl->is_online();
}


bool dds_device::wait_until_ready( size_t timeout_ms, bool allow_partial_capabilities ) const
{
    if( is_ready() )
        return true;

    if( ! timeout_ms )
        DDS_THROW( runtime_error, "device is " << ( is_online() ? "not ready" : "offline" ) );

    LOG_DEBUG( "[" << debug_name() << "] waiting until ready ..." );
    rsutils::time::timer timer{ std::chrono::milliseconds( timeout_ms ) };
    bool was_online = is_online();
    do
    {
        if( timer.has_expired() )
        {
            if( allow_partial_capabilities && _impl->is_initializing() )
            {
                _impl->set_state( impl::state_t::READY );
                return false;
            }
            DDS_THROW( runtime_error, "[" << debug_name() << "] timeout waiting to get ready" );
        }
        std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
        if( was_online )
        {
            if( ! is_online() )
                DDS_THROW( runtime_error, "[" << debug_name() << "] device went offline" );
        }
        else
            was_online = is_online();
    }
    while( ! is_ready() );

    return true;
}


void dds_device::wait_until_online( size_t timeout_ms )
{
    if( is_online() )
        return;

    if( ! timeout_ms )
        DDS_THROW( runtime_error, "device is offline" );

    LOG_DEBUG( "[" << debug_name() << "] waiting until online ..." );
    rsutils::time::timer timer{ std::chrono::milliseconds( timeout_ms ) };
    do
    {
        if( timer.has_expired() )
            DDS_THROW( runtime_error, "[" << debug_name() << "] timeout waiting to come online" );
        std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
    }
    while( ! is_online() );
}


void dds_device::wait_until_offline( size_t timeout_ms )
{
    if( is_offline() )
        return;

    if( ! timeout_ms )
        DDS_THROW( runtime_error, "device is online" );

    LOG_DEBUG( "[" << debug_name() << "] waiting until offline ..." );
    rsutils::time::timer timer{ std::chrono::milliseconds( timeout_ms ) };
    do
    {
        if( timer.has_expired() )
            DDS_THROW( runtime_error, "[" << debug_name() << "] timeout waiting to go offline" );
        std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
    }
    while( ! is_offline() );
}


void dds_device::on_discovery_lost()
{
    // Called when the device-watcher has lost connection with the device
    // Only devices that are discovered by the device-watcher get called with this!
    // A device that loses discovery needs to be reinitialized
    _impl->set_state( impl::state_t::OFFLINE );
}


void dds_device::on_discovery_restored( topics::device_info const & new_info )
{
    // Called when the device-watcher has re-connected with a device that was lost before
    // Only devices that are discovered by the device-watcher get called with this!
    if( new_info.name() != device_info().name() )
        DDS_THROW( runtime_error, "device name cannot change" );
    if( new_info.topic_root() != device_info().topic_root() )
        DDS_THROW( runtime_error, "device topic root cannot change" );
    if( new_info.serial_number() != device_info().serial_number() )
        DDS_THROW( runtime_error, "device serial number cannot change" );

    _impl->_info = new_info;
    _impl->set_state( impl::state_t::INITIALIZING );
    // NOTE: still not ready - pending handshake/reinitialization
}


std::shared_ptr< dds_participant > const & dds_device::participant() const
{
    return _impl->_participant;
}

std::shared_ptr< dds_subscriber > const & dds_device::subscriber() const
{
    return _impl->_subscriber;
}

topics::device_info const & dds_device::device_info() const
{
    return _impl->_info;
}

std::string dds_device::debug_name() const
{
    return _impl->debug_name();
}

dds_guid const & dds_device::server_guid() const
{
    return _impl->_server_guid;
}

dds_guid const & dds_device::guid() const
{
    return _impl->guid();
}

size_t dds_device::number_of_streams() const
{
    return _impl->_streams.size();
}

size_t dds_device::foreach_stream( std::function< void( std::shared_ptr< dds_stream > stream ) > fn ) const
{
    for ( auto const & stream : _impl->_streams )
    {
        fn( stream.second );
    }

    return _impl->_streams.size();
}

size_t dds_device::foreach_option( std::function< void( std::shared_ptr< dds_option > option ) > fn ) const
{
    for( auto const & option : _impl->_options)
    {
        fn( option );
    }

    return _impl->_options.size();
}

void dds_device::open( const dds_stream_profiles & profiles )
{
    wait_until_ready( 0 );  // throw if not
    _impl->open( profiles );
}

void dds_device::close( const dds_stream_profiles & profiles )
{
    wait_until_ready( 0 );  // throw if not
    _impl->close( profiles );
}

void dds_device::set_option_value( const std::shared_ptr< dds_option > & option, json new_value )
{
    wait_until_ready( 0 );  // throw if not
    _impl->set_option_value( option, std::move( new_value ) );
}

json dds_device::query_option_value( const std::shared_ptr< dds_option > & option )
{
    wait_until_ready( 0 );  // throw if not
    return _impl->query_option_value( option );
}

void dds_device::set_embedded_filter(const std::shared_ptr< dds_embedded_filter >& filter, const json& options_value)
{
    wait_until_ready( 0 );  // throw if not
    _impl->set_embedded_filter(filter, options_value);
}

json dds_device::query_embedded_filter(const std::shared_ptr< dds_embedded_filter >& filter)
{
    wait_until_ready( 0 );  // throw if not
    return _impl->query_embedded_filter(filter);
}

void dds_device::send_control( json const & control, json * reply ) const
{
    wait_until_ready( 0 );  // throw if not
    _impl->write_control_message( control, reply );
}

bool dds_device::has_extrinsics() const
{
    return ! _impl->_extrinsics_map.empty();
}

std::shared_ptr< extrinsics > dds_device::get_extrinsics( std::string const & from, std::string const & to ) const
{
    auto iter = _impl->_extrinsics_map.find( std::make_pair( from, to ) );
    if( iter != _impl->_extrinsics_map.end() )
        return iter->second;

    std::shared_ptr< extrinsics > empty;
    return empty;
}

bool dds_device::supports_metadata() const
{
    return !! _impl->_metadata_reader;
}

rsutils::subscription dds_device::on_metadata_available( on_metadata_available_callback && cb )
{
    return _impl->on_metadata_available( std::move( cb ) );
}

rsutils::subscription dds_device::on_device_log( on_device_log_callback && cb )
{
    return _impl->on_device_log( std::move( cb ) );
}

rsutils::subscription dds_device::on_notification( on_notification_callback && cb )
{
    return _impl->on_notification( std::move( cb ) );
}

rsutils::subscription dds_device::on_calibration_changed( on_calibration_changed_callback && cb )
{
    return _impl->on_calibration_changed( std::move( cb ) );
}


bool dds_device::check_reply( json const & reply, std::string * p_explanation )
{
    auto status_j = reply.nested( topics::reply::key::status );
    if( ! status_j )
        return true;  // no status == ok
    std::ostringstream os;
    if( ! status_j.is_string() )
        os << "bad status " << status_j;
    else if( status_j.string_ref() == topics::reply::status::ok )
        return true;
    else
    {
        // An 'id' is mandatory, but if it's a response to a control it's contained there
        auto const control = reply.nested( topics::reply::key::control );
        auto const control_sample = control ? reply.nested( topics::reply::key::sample ) : rsutils::json_ref( rsutils::missing_json );
        auto & id = ( control_sample ? control.get_json() : reply )
                        .nested( topics::reply::key::id, &json::is_string )
                        .string_ref_or_empty();
        auto & status = status_j.string_ref();
        if( ! id.empty() || status != "error" )
        {
            os << "[";
            if( id.empty() )
                os << status;
            else if( status == "error" )
                os << "\"" << id << "\"";
            else
                os << "\"" << id << "\" " << status;
            os << "]";
        }
        if( auto explanation_j = reply.nested( topics::reply::key::explanation ) )
        {
            os << ' ';
            if( explanation_j.string_ref_or_empty().empty() )
                os << "bad explanation " << explanation_j;
            else
                os << explanation_j.string_ref();
        }
    }
    if( ! p_explanation )
    {
        LOG_DEBUG( "error: " << std::setw( 4 ) << reply );
        DDS_THROW( runtime_error, os.str() );
    }
    *p_explanation = os.str();
    return false;
}


}  // namespace realdds
