// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "rs-dds-close-range-control.h"
#include <src/librealsense-exception.h>
#include <realdds/dds-embedded-filter.h>
#include <realdds/dds-option.h>
#include <rsutils/easylogging/easyloggingpp.h>
#include <rsutils/string/from.h>
#include <rsutils/json.h>
#include <librealsense2/h/rs_hdrd_control.h>
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <string>
#include <type_traits>

using rsutils::json;

namespace librealsense {

    // FW-advertised DDS option names of the rs2_hdrd_control fields, in wire order
    static const char * const field_names[] = { "Enable", "Filter Type", "Secondary frame downscale ratio", "Shift Mode",
                                                "Shift Pixels", "MinZ Threshold Mode", "Threshold" };
    static constexpr size_t field_count = std::extent< decltype( field_names ) >::value;
    static constexpr size_t downscale_ratio_field = 2;
    static constexpr size_t fields_offset = offsetof( rs2_hdrd_control, enable );
    static_assert( offsetof( rs2_hdrd_control, reserved ) - fields_offset == field_count * sizeof( int32_t ),
                   "field_names must match the contiguous int32_t fields of rs2_hdrd_control" );

    // The downscale ratio is a DDS enum of "2"/"4"; rs2_hdrd_control holds 1 = x2, 2 = x4
    static int32_t to_field( size_t i, json const & dds_value )
    {
        if( dds_value.is_null() )
            return 0;
        return i == downscale_ratio_field ? std::stoi( dds_value.get< std::string >() ) / 2 : dds_value.get< int32_t >();
    }

    static json to_dds( size_t i, int32_t field )
    {
        return i == downscale_ratio_field ? json( std::to_string( field * 2 ) ) : json( field );
    }

    rs_dds_close_range_control::rs_dds_close_range_control(
        std::shared_ptr< realdds::dds_embedded_filter > const & dds_embedded_filter,
        set_embedded_filter_callback set_embedded_filter_cb )
        : _dds_ef( dds_embedded_filter )
        , _set_ef_cb( std::move( set_embedded_filter_cb ) )
        , _fields( field_count )
    {
        for( auto & option : _dds_ef->get_options() )
        {
            auto it = std::find( std::begin( field_names ), std::end( field_names ), option->get_name() );
            if( it == std::end( field_names ) )
                LOG_WARNING( "Ignoring unknown Improved Close Range Depth option '" << option->get_name() << "'" );
            else
                _fields[it - std::begin( field_names )] = option;
        }
    }

    // Packs rs2_hdrd_control; get( field index, its DDS option ) returns the field's value
    template< class Get >
    std::vector< uint8_t > rs_dds_close_range_control::pack( Get && get ) const
    {
        rs2_hdrd_control cfg{};
        cfg.header.version = DPP_HEADER_CURRENT_VERSION;
        cfg.header.ctl_id = 0x0008;
        cfg.header.param_count = static_cast< uint8_t >( field_count );
        int32_t fields[field_count];
        for( size_t i = 0; i < field_count; ++i )
            fields[i] = _fields[i] ? get( i, *_fields[i] ) : 0;
        auto bytes = reinterpret_cast< uint8_t * >( &cfg );
        std::memcpy( bytes + fields_offset, fields, sizeof( fields ) );
        return { bytes, bytes + sizeof( cfg ) };
    }

    std::vector< uint8_t > rs_dds_close_range_control::get_raw() const
    {
        return pack( []( size_t i, realdds::dds_option const & o ) { return to_field( i, o.get_value() ); } );
    }

    void rs_dds_close_range_control::set_raw( const void * data, size_t size )
    {
        if( ! data || size != sizeof( rs2_hdrd_control ) )
            throw invalid_value_exception( rsutils::string::from() << "Improved Close Range Control expects "
                                                                   << sizeof( rs2_hdrd_control ) << " bytes, got " << size );
        int32_t fields[field_count];
        std::memcpy( fields, static_cast< const uint8_t * >( data ) + fields_offset, sizeof( fields ) );

        json options = json::object();
        for( size_t i = 0; i < field_count; ++i )
        {
            auto & option = _fields[i];
            if( ! option )
                continue;
            json value = to_dds( i, fields[i] );
            option->check_value( value );
            if( ( ! option->get_minimum_value().is_null() && value < option->get_minimum_value() )
                || ( ! option->get_maximum_value().is_null() && value > option->get_maximum_value() ) )
                throw invalid_value_exception( rsutils::string::from()
                                               << option->get_name() << " value " << value << " is out of range ["
                                               << option->get_minimum_value() << ", " << option->get_maximum_value() << "]" );
            options[option->get_name()] = std::move( value );
        }
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
        auto min = pack( [&]( size_t i, realdds::dds_option const & o )
                         { return to_field( i, choices( o ) ? json( choices( o )->front() ) : o.get_minimum_value() ); } );
        auto max = pack( [&]( size_t i, realdds::dds_option const & o )
                         { return to_field( i, choices( o ) ? json( choices( o )->back() ) : o.get_maximum_value() ); } );
        auto step = pack( [&]( size_t i, realdds::dds_option const & o )
                          { return choices( o ) ? 1 : to_field( i, o.get_stepping() ); } );
        auto def = pack( []( size_t i, realdds::dds_option const & o ) { return to_field( i, o.get_default_value() ); } );
        std::vector< uint8_t > range;
        for( auto * bound : { &min, &max, &step, &def } )
            range.insert( range.end(), bound->begin(), bound->end() );
        return range;
    }

    const char * rs_dds_close_range_control::get_description() const
    {
        return "Improved Close Range Control - use rs2_set/get_composite_option, see rs_hdrd_control.h";
    }

}  // namespace librealsense
