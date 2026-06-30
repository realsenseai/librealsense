// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#include <realdds/dds-guid.h>
#include <realdds/dds-participant.h>
#include <rsutils/string/hexdump.h>
#include <rsutils/string/hexarray.h>
#include <sstream>
#include <cstdlib>
#include <limits>
#include <cctype>


namespace realdds {


std::ostream & operator<<( std::ostream & os, print_raw_guid_prefix const & g )
{
    if( eprosima::fastdds::rtps::c_VendorId_eProsima[0] == g._prefix.value[0]
        && eprosima::fastdds::rtps::c_VendorId_eProsima[1] == g._prefix.value[1] )
    {
        // For eProsima, use a more readable (and usually, shorter) representation or host.pid.eid
        // bytes 2-7 are the host info, 8-11 the participant ID
        os << rsutils::string::hexdump( g._prefix.value, g._prefix.size ).format( "{+2}{6}.{04}" );
    }
    else
    {
        // Otherwise, just dump the 24-character hex string
        os << rsutils::string::hexdump( g._prefix.value, g._prefix.size );
    }
    return os;
}


std::ostream & operator<<( std::ostream & os, print_raw_guid const & g )
{
    if( g._guid != unknown_guid )
    {
        if( g._guid.guidPrefix != g._base_prefix )
            os << print_raw_guid_prefix( g._guid.guidPrefix, g._base_prefix );
        os << '.';  // FastDDS uses '|'
        os << rsutils::string::in_hex( g._guid.entityId.to_uint32() );
    }
    else
    {
        os << "|GUID UNKNOWN|";  // same as in FastDDS
    }
    return os;
}


std::ostream & operator<<( std::ostream & os, print_guid const & g )
{
    if( g._guid != unknown_guid )
    {
        if( g._guid.guidPrefix != g._base_prefix )
        {
            std::string participant = dds_participant::name_from_guid( g._guid );
            if( ! participant.empty() )
                os << participant;
            else
                os << print_raw_guid_prefix( g._guid.guidPrefix, g._base_prefix );
        }
        if( g._print_entity )
        {
            os << '.';  // FastDDS uses '|'
            os << rsutils::string::in_hex( g._guid.entityId.to_uint32() );
        }
    }
    else
    {
        os << "|GUID UNKNOWN|";  // same as in FastDDS
    }
    return os;
}


std::string print( dds_guid const & guid,
                   dds_guid_prefix const & base_prefix,
                   bool readable_name )
{
    std::ostringstream output;
    if( guid != unknown_guid )
    {
        if( guid.guidPrefix != base_prefix )
        {
            std::string participant;
            if( readable_name )
                participant = dds_participant::name_from_guid( guid );
            if( ! participant.empty() )
            {
                output << participant;
            }
            else
            {
                output << print_raw_guid_prefix( guid.guidPrefix, base_prefix );
            }
        }
        output << '.';  // FastDDS uses '|'
        output << std::hex << guid.entityId.to_uint32() << std::dec;
    }
    else
    {
        output << "|GUID UNKNOWN|";  // same as in FastDDS
    }
    return output.str();
}


static char const * hex_to_uint32( char const * pch, uint32_t * pv )
{
    // strtoul accepts leading spaces or a minus sign
    if( ! std::isxdigit( *pch ) )
        return nullptr;

    auto v = std::strtoul( pch, const_cast< char ** >( &pch ), 16 );
    if( v > std::numeric_limits< uint32_t >::max() )
        return nullptr;

    *pv = static_cast< uint32_t >( v );
    return pch;
}


dds_guid guid_from_string( std::string const & s )
{
    // Expecting one of two formats:
    //     223344556677.<0xPID>.<0xEID>       // eProsima (first two bytes 010f)
    // Or:
    //     001122334455667788990011.<0xEID>   // everything else
    if( s.length() < 16 )
        return dds_guid();
    auto period = s.find( '.' );
    if( period == std::string::npos )
        return dds_guid();
    char const * pch = s.data();
    dds_guid guid;
    uint8_t * pb = guid.guidPrefix.value;
    if( period == 12 )
    {
        // This format is for eProsima
        *pb++ = eprosima::fastdds::rtps::c_VendorId_eProsima[0];
        *pb++ = eprosima::fastdds::rtps::c_VendorId_eProsima[1];
    }
    else if( period != 24 )
        return dds_guid();

    pb = rsutils::string::hex_to_bytes( pch, pch + period, pb );
    if( ! pb )
        return dds_guid();

    uint32_t participant_or_entity_id;
    pch = hex_to_uint32( pch + period + 1, &participant_or_entity_id );
    if( ! pch )
        return dds_guid();

    if( ! *pch )
    {
        if( period != 24 )
            return dds_guid();
        guid.entityId = participant_or_entity_id;
    }
    else
    {
        if( *pch != '.' )
            return dds_guid();

        // little-endian; same as in GuidUtils::guid_prefix_create
        *pb++ = static_cast< uint8_t >( participant_or_entity_id & 0xFF );
        *pb++ = static_cast< uint8_t >( ( participant_or_entity_id >> 8 ) & 0xFF );
        *pb++ = static_cast< uint8_t >( ( participant_or_entity_id >> 16 ) & 0xFF );
        *pb++ = static_cast< uint8_t >( ( participant_or_entity_id >> 24 ) & 0xFF );

        pch = hex_to_uint32( pch + 1, &participant_or_entity_id );
        if( ! pch || *pch )
            return dds_guid();
        guid.entityId = participant_or_entity_id;
    }
    return guid;
}


}  // namespace realdds
