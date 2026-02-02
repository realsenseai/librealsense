// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#include <rsutils/type/eth-config.h>
#include <rsutils/type/eth-config-struct.h>

#include <rsutils/number/crc32.h>
#include <rsutils/string/hexdump.h>
#include <rsutils/string/from.h>



namespace rsutils {
namespace type {

std::ostream & operator<<( std::ostream & os, link_priority p )
{
    if( static_cast< uint8_t >( p ) & static_cast< uint8_t >( link_priority::_dynamic_bit ) )
    {
        os << "dynamic-";
        p = static_cast< link_priority >( static_cast< uint8_t >( p )
                                          & ~static_cast< uint8_t >( link_priority::_dynamic_bit ) );
    }
    switch( p )
    {
    case link_priority::usb_only:
        os << "usb-only";
        break;
    case link_priority::usb_first:
        os << "usb-first";
        break;
    case link_priority::eth_only:
        os << "eth-only";
        break;
    case link_priority::eth_first:
        os << "eth-first";
        break;
    default:
        os << "UNKNOWN-" << (int) p;
        break;
    }
    return os;
}


std::ostream & operator<<( std::ostream & os, ip_3 const & ip3 )
{
    os << ip3.ip;
    if( ip3.netmask.is_valid() )
        os << " & " << ip3.netmask;
    if( ip3.gateway.is_valid() )
        os << " / " << ip3.gateway;
    return os;
}


eth_config::eth_config( eth_config_v3 const & v3 )
    : header( v3.header )
    , mac_address( rsutils::string::from( rsutils::string::hexdump( v3.mac_address, sizeof( v3.mac_address ) )
                                              .format( "{01}:{01}:{01}:{01}:{01}:{01}" ) ) )
    , configured{ v3.config_ip, v3.config_netmask, v3.config_gateway }
    , actual{ v3.actual_ip, v3.actual_netmask, v3.actual_gateway }
    , dds{ v3.domain_id }
    , link{ v3.mtu, v3.link_speed, v3.link_check_timeout, link_priority( v3.link_priority ) }
    , dhcp{ v3.dhcp_on != 0, v3.dhcp_timeout }
    , transmission_delay( 0 )
    , udp_ttl( 0 )
{
    if( header.version != 3 )
        throw std::runtime_error( rsutils::string::from() << "eth-config expecting version 3. Got " << header.version );
}

eth_config::eth_config( eth_config_v4 const & v4 )
    : header( v4.header )
    , mac_address( rsutils::string::from( rsutils::string::hexdump( v4.mac_address, sizeof( v4.mac_address ) )
                                              .format( "{01}:{01}:{01}:{01}:{01}:{01}" ) ) )
    , configured{ v4.config_ip, v4.config_netmask, v4.config_gateway }
    , actual{ v4.actual_ip, v4.actual_netmask, v4.actual_gateway }
    , dds{ v4.domain_id }
    , link{ v4.mtu, v4.link_speed, v4.link_check_timeout, link_priority( v4.link_priority ) }
    , dhcp{ v4.dhcp_on != 0, v4.dhcp_timeout }
    , transmission_delay( v4.transmission_delay )
    , udp_ttl( 0 )
{
    if( header.version != 4 )
        throw std::runtime_error( rsutils::string::from() << "eth-config expecting version 4. Got " << header.version );
}

eth_config::eth_config( eth_config_v5 const & v5 )
    : header( v5.header )
    , mac_address( rsutils::string::from( rsutils::string::hexdump( v5.mac_address, sizeof( v5.mac_address ) )
                                              .format( "{01}:{01}:{01}:{01}:{01}:{01}" ) ) )
    , configured{ v5.config_ip, v5.config_netmask, v5.config_gateway }
    , actual{ v5.actual_ip, v5.actual_netmask, v5.actual_gateway }
    , dds{ v5.domain_id }
    , link{ v5.mtu, v5.link_speed, v5.link_check_timeout, link_priority( v5.link_priority ) }
    , dhcp{ v5.dhcp_on != 0, v5.dhcp_timeout }
    , transmission_delay( v5.transmission_delay )
    , udp_ttl( v5.udp_ttl )
{
    if( header.version != 5 )
        throw std::runtime_error( rsutils::string::from() << "eth-config expecting version 5. Got " << header.version );
}

eth_config::eth_config( std::vector< uint8_t > const & hwm_response )
{
    auto header = reinterpret_cast< eth_config_header const * >( hwm_response.data() );
    if( hwm_response.size() < sizeof( *header ) )
        throw std::runtime_error( rsutils::string::from()
                                  << "HWM response size (" << hwm_response.size() << ") does not fit header (size "
                                  << sizeof( *header ) << ")" );

    if( hwm_response.size() != sizeof( *header ) + header->size )
        throw std::runtime_error( rsutils::string::from()
                                  << "HWM response size (" << hwm_response.size() << ") does not fit header ("
                                  << sizeof( *header ) << ") + header size (" << header->size << ")" );

    auto const crc = rsutils::number::calc_crc32( hwm_response.data() + sizeof( *header ), header->size );
    if( header->crc != crc )
        throw std::runtime_error( rsutils::string::from()
                                  << "Eth config table CRC (" << header->crc << ") does not match response " << crc );

    switch( header->version )
    {
    case 3: {
        if( header->size != sizeof( eth_config_v3 ) - sizeof( *header ) )
            throw std::runtime_error( rsutils::string::from()
                                      << "Invalid Eth config table v3 size (" << header->size << "); expecting "
                                      << sizeof( eth_config_v3 ) << "-" << sizeof( *header ) );
        auto config = reinterpret_cast< eth_config_v3 const * >( hwm_response.data() );
        *this = *config;
        break;
    }
    case 4: {
        if( header->size != sizeof( eth_config_v4 ) - sizeof( *header ) )
            throw std::runtime_error( rsutils::string::from()
                                      << "Invalid Eth config table v4 size (" << header->size << "); expecting "
                                      << sizeof( eth_config_v4 ) << "-" << sizeof( *header ) );
        auto config = reinterpret_cast< eth_config_v4 const * >( hwm_response.data() );
        *this = *config;
        break;
    }
    case 5: {
        if( header->size != sizeof( eth_config_v5 ) - sizeof( *header ) )
            throw std::runtime_error( rsutils::string::from()
                                      << "Invalid Eth config table v5 size (" << header->size << "); expecting "
                                      << sizeof( eth_config_v5 ) << "-" << sizeof( *header ) );
        auto config = reinterpret_cast< eth_config_v5 const * >( hwm_response.data() );
        *this = *config;
        break;
    }
    default:
        throw std::runtime_error( rsutils::string::from()
                                  << "Unrecognized Eth config table version " << header->version );
    }
}


bool eth_config::operator==( eth_config const & other ) const noexcept
{
    // Only compare those items that are configurable
    return configured.ip == other.configured.ip
        && configured.netmask == other.configured.netmask
        && configured.gateway == other.configured.gateway
        && dds.domain_id == other.dds.domain_id
        && dhcp.on == other.dhcp.on
        && link.mtu == other.link.mtu
        && link.priority == other.link.priority
        && link.timeout == other.link.timeout
        && dhcp.timeout == other.dhcp.timeout
        && transmission_delay == other.transmission_delay
        && udp_ttl == other.udp_ttl;
}


bool eth_config::operator!=( eth_config const & other ) const noexcept
{
    return !operator==( other );
}


std::vector< uint8_t > eth_config::build_command() const
{
    validate();

    std::vector< uint8_t > data;
    data.resize( sizeof( eth_config_v5 ) );
    eth_config_v5 & cfg = *reinterpret_cast< eth_config_v5 * >( data.data() );
    configured.ip.get_components( cfg.config_ip );
    configured.netmask.get_components( cfg.config_netmask );
    configured.gateway.get_components( cfg.config_gateway );
    cfg.dhcp_on = dhcp.on;
    cfg.dhcp_timeout = dhcp.timeout;
    cfg.domain_id = dds.domain_id;
    cfg.link_check_timeout = link.timeout;
    cfg.link_priority = (uint8_t)link.priority;
    cfg.mtu = link.mtu;
    cfg.transmission_delay = transmission_delay;
    cfg.udp_ttl = udp_ttl;

    if( header.version < 5 )
    {
        cfg.udp_ttl = 0;  // Not supported in FW, keep default DDS layer value.
    }

    if( header.version == 3 )
    {
        cfg.mtu = 9000;  // R/O, but must be set to this value
        cfg.transmission_delay = 0; // Was reserved on v3, keep 0
        data.resize( sizeof( eth_config_v3 ) ); // Trim greater version reserved bytes
    }

    eth_config_header & cfg_header = *reinterpret_cast< eth_config_header * >( data.data() ); // Getting reference to header again after possible resize
    cfg_header.version = header.version;
    cfg_header.size = static_cast< uint16_t >( data.size() - sizeof( cfg_header ) );
    cfg_header.crc = rsutils::number::calc_crc32( data.data() + sizeof( cfg_header ), cfg_header.size );
    
    return data;
}

void eth_config::validate() const
{
    if( header.version != 3 && header.version != 4 && header.version != 5 )
        throw std::invalid_argument( rsutils::string::from() << "Unrecognized Eth config table version " << header.version );

    if( ! configured.ip.is_valid() )
        throw std::invalid_argument( rsutils::string::from() << "Invalid configured IP address " << configured.ip );
    if( ! configured.netmask.is_valid() )
        throw std::invalid_argument( rsutils::string::from() << "Invalid configured network mask " << configured.netmask );
    // Allowing gateway to be 0.0.0.0 (unset). Camera will only communicate in local network.

    if( dds.domain_id < 0 || dds.domain_id > 232 )
        throw std::invalid_argument( rsutils::string::from() << "Domain ID should be in 0-232 range. Current " << dds.domain_id );

    if( link.mtu < 500 || link.mtu > 9000 )
        throw std::invalid_argument( rsutils::string::from() << "MTU size should be 500-9000. Current " << link.mtu );
    if( ( link.mtu % 500 ) != 0 )
        throw std::invalid_argument( rsutils::string::from() << "MTU size must be divisible by 500. Current " << link.mtu );
    if( header.version == 3 && link.mtu != 9000 )
        throw std::invalid_argument( "Camera FW supports only MTU 9000." );

    if( link.timeout < 2000 || link.timeout > 30000 )
        throw std::invalid_argument( rsutils::string::from() << "Link timeout should be 2000-30000. Current " << link.timeout );
    if( ( link.timeout % 100 ) != 0 )
        throw std::invalid_argument( rsutils::string::from() << "Link timeout must be divisible by 100. Current " << link.timeout );

    if( transmission_delay > 144 )
        throw std::invalid_argument( rsutils::string::from() << "Transmission delay should be 0-144. Current " << transmission_delay );
    if( ( transmission_delay % 3 ) != 0)
        throw std::invalid_argument( rsutils::string::from() << "Transmission delay must be divisible by 3. Current " << transmission_delay );
    if( header.version == 3 && transmission_delay != 0 )
        throw std::invalid_argument( "Camera FW does not support transmission delay." );

    if( header.version < 5 && udp_ttl != 0 )
        throw std::invalid_argument( "Camera FW does not support changing UDP TTL value." );
    if( header.version >= 5 && udp_ttl > 255 )
        throw std::invalid_argument( rsutils::string::from() << "UDP TTL should be 1-255 (or 0 for system default). Current " << udp_ttl );
}

}  // namespace type
}  // namespace rsutils
