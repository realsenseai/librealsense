// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "rs-dds-close-range-control.h"
#include <src/librealsense-exception.h>
#include <realdds/dds-embedded-filter.h>
#include <realdds/dds-option.h>
#include <rsutils/easylogging/easyloggingpp.h>
#include <rsutils/string/from.h>
#include <rsutils/json.h>
#include <librealsense2/h/rs_icr_control.h>
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>

using rsutils::json;

namespace librealsense {

    // DDS options of the FW, by rs2_icr_control field in wire order: one option per field (the last two are read-only)
    static constexpr size_t field_count = 8;
    static constexpr size_t writable_field_count = field_count - 2;
    static const char * const field_names[field_count] = { "Mode",
                                                           "Downscale Ratio",
                                                           "High-Res Mode",
                                                           "High-Res Offset",
                                                           "Smoothing Offset",
                                                           "Completion Offset",
                                                           "Nominal MinZ",
                                                           "Improved MinZ" };
    static constexpr size_t fields_offset = offsetof( rs2_icr_control, mode );
    static_assert( sizeof( rs2_icr_control ) - fields_offset == field_count * sizeof( int32_t ),
                   "field_names must match the contiguous int32_t fields of rs2_icr_control" );

    static std::shared_ptr< realdds::dds_option > find_option( realdds::dds_options const & options, std::string const & name )
    {
        auto it = std::find_if( options.begin(), options.end(), [&]( auto const & o ) { return o->get_name() == name; } );
        return it == options.end() ? nullptr : *it;
    }

    rs_dds_close_range_control::rs_dds_close_range_control(
        std::shared_ptr< realdds::dds_embedded_filter > const & dds_embedded_filter,
        set_embedded_filter_callback set_embedded_filter_cb )
        : _dds_ef( dds_embedded_filter )
        , _set_ef_cb( std::move( set_embedded_filter_cb ) )
        , _fields( field_count )
    {
        auto const & options = _dds_ef->get_options();
        for( size_t i = 0; i < field_count; ++i )
            _fields[i] = find_option( options, field_names[i] );
    }

    static int32_t to_field( json const & dds_value )
    {
        return dds_value.is_null() ? 0 : dds_value.get< int32_t >();
    }

    // Packs rs2_icr_control; get( DDS option ) returns the field's value
    template< class Get >
    std::vector< uint8_t > rs_dds_close_range_control::pack( Get && get ) const
    {
        rs2_icr_control cfg{};
        cfg.header.version = DPP_HEADER_CURRENT_VERSION;
        cfg.header.ctl_id = 0x0008;
        cfg.header.param_count = static_cast< uint8_t >( field_count );
        int32_t fields[field_count];
        for( size_t i = 0; i < field_count; ++i )
            fields[i] = _fields[i] ? get( *_fields[i] ) : 0;
        std::vector< uint8_t > raw( sizeof( cfg ) );
        std::memcpy( raw.data(), &cfg, sizeof( cfg ) );
        std::memcpy( raw.data() + fields_offset, fields, sizeof( fields ) );
        return raw;
    }

    std::vector< uint8_t > rs_dds_close_range_control::get_raw() const
    {
        return pack( []( realdds::dds_option const & o ) { return to_field( o.get_value() ); } );
    }

    // Rejects a value its option does not accept, then adds it to the set-filter request
    static void add_checked( json & options, std::shared_ptr< realdds::dds_option > const & option, json value )
    {
        option->check_value( value );
        if( ( ! option->get_minimum_value().is_null() && value < option->get_minimum_value() )
            || ( ! option->get_maximum_value().is_null() && value > option->get_maximum_value() ) )
            throw invalid_value_exception( rsutils::string::from()
                                           << option->get_name() << " value " << value << " is out of range ["
                                           << option->get_minimum_value() << ", " << option->get_maximum_value() << "]" );
        options[option->get_name()] = std::move( value );
    }

    void rs_dds_close_range_control::set_raw( const void * data, size_t size )
    {
        if( ! data || size != sizeof( rs2_icr_control ) )
            throw invalid_value_exception( rsutils::string::from() << "Improved Close Range Control expects "
                                                                   << sizeof( rs2_icr_control ) << " bytes, got " << size );
        int32_t fields[field_count];
        std::memcpy( fields, static_cast< const uint8_t * >( data ) + fields_offset, sizeof( fields ) );

        // The min depth fields are FW-computed and never sent
        json options = json::object();
        for( size_t i = 0; i < writable_field_count; ++i )
            if( _fields[i] )
                add_checked( options, _fields[i], fields[i] );
        _set_ef_cb( options );  // the reply updates the cached values, possibly adjusted by FW
    }

    // Enums carry no range, so their bounds come from the first/last choice
    std::vector< uint8_t > rs_dds_close_range_control::get_raw_range() const
    {
        auto choices = []( realdds::dds_option const & o ) -> std::vector< std::string > const *
        {
            auto e = dynamic_cast< realdds::dds_enum_option const * >( &o );
            return e && ! e->get_choices().empty() ? &e->get_choices() : nullptr;
        };
        auto min = pack( [&]( realdds::dds_option const & o )
                         { return to_field( choices( o ) ? json( choices( o )->front() ) : o.get_minimum_value() ); } );
        auto max = pack( [&]( realdds::dds_option const & o )
                         { return to_field( choices( o ) ? json( choices( o )->back() ) : o.get_maximum_value() ); } );
        auto step = pack( [&]( realdds::dds_option const & o )
                          { return choices( o ) ? 1 : to_field( o.get_stepping() ); } );
        auto def = pack( []( realdds::dds_option const & o ) { return to_field( o.get_default_value() ); } );
        std::vector< uint8_t > range;
        for( auto * bound : { &min, &max, &step, &def } )
            range.insert( range.end(), bound->begin(), bound->end() );
        return range;
    }

    const char * rs_dds_close_range_control::get_description() const
    {
        return "Improved Close Range Control - use rs2_set/get_composite_option, see rs_icr_control.h";
    }

}  // namespace librealsense
