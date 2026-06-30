// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#pragma once

#include "dds-defines.h"
#include <fastdds/rtps/common/Guid.h>
#include <iosfwd>


namespace realdds {


static constexpr auto & unknown_guid = eprosima::fastrtps::rtps::c_Guid_Unknown;


// Convert a prefix to a hex string: not human-readable!
// In one of two formats:
//     223344556677.<0xPID>               // eProsima
// Or:
//     001122334455667788990011           // everything else
// Used internally by next function.
//
struct print_raw_guid_prefix
{
    dds_guid_prefix const & _prefix;
    explicit print_raw_guid_prefix( dds_guid_prefix const & prefix,
                                    dds_guid_prefix const & /*base_prefix*/ = unknown_guid.guidPrefix )
        : _prefix( prefix )
    {
    }
};
std::ostream & operator<<( std::ostream &, print_raw_guid_prefix const & );


// Custom GUID printer: attempts a more succinct representation:
// If the participant is known, its name will be shown instead of the raw bytes.
//     <name-or-prefix>.<entity-id-in-hex>
// If a base_prefix is provided, will try to minimize a common denominator -- you can use your
// participant's guid if you want to shorten.
//
struct print_guid
{
    dds_guid const & _guid;
    dds_guid_prefix const & _base_prefix;
    bool _print_entity;

    explicit print_guid( dds_guid const & guid, dds_guid_prefix const & base_prefix = unknown_guid.guidPrefix,
                         bool print_entity = false )
        : _guid( guid )
        , _base_prefix( base_prefix )
        , _print_entity( print_entity )
    {
    }
    explicit print_guid( dds_guid const & guid, dds_guid const & base_guid, bool print_entity = false )
        : print_guid( guid, base_guid.guidPrefix, print_entity )
    {
    }
};
std::ostream & operator<<( std::ostream &, print_guid const & );


// Same, except leaves output in raw form (bytes, not name)
//     <prefix>.<entity-id-in-hex>
//
struct print_raw_guid
{
    dds_guid const & _guid;
    dds_guid_prefix const & _base_prefix;

    explicit print_raw_guid( dds_guid const & guid, dds_guid_prefix const & base_prefix = unknown_guid.guidPrefix )
        : _guid( guid )
        , _base_prefix( base_prefix )
    {
    }
    explicit print_raw_guid( dds_guid const & guid, dds_guid const & base_guid )
        : print_raw_guid( guid, base_guid.guidPrefix )
    {
    }
};
std::ostream & operator<<( std::ostream &, print_raw_guid const & );


// The reverse: get a guid from a RAW guid string.
// Expecting one of two formats:
//     223344556677.<0xPID>.<0xEID>       // eProsima
// Or:
//     001122334455667788990011.<0xEID>   // everything else
// Returns unknown_guid if not parseable
dds_guid guid_from_string( std::string const & );


}  // namespace realdds
