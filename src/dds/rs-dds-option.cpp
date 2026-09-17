// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#include "rs-dds-option.h"
#include <src/core/enum-helpers.h>

#include <realdds/dds-option.h>
#include <rsutils/json.h>

using rsutils::json;


namespace librealsense {


static option_range range_from_realdds( std::shared_ptr< realdds::dds_option > const & dds_opt )
{
    if( dds_opt->get_minimum_value().is_number() && dds_opt->get_maximum_value().is_number()
        && dds_opt->get_stepping().is_number() )
    {
        return { dds_opt->get_minimum_value(),
                 dds_opt->get_maximum_value(),
                 dds_opt->get_stepping(),
                 dds_opt->get_default_value() };
    }
    if( std::dynamic_pointer_cast< realdds::dds_boolean_option >( dds_opt ) )
    {
        return { 0, 1, 1, bool( dds_opt->get_default_value() ) ? 1.f : 0.f };
    }
    if( auto e = std::dynamic_pointer_cast< realdds::dds_enum_option >( dds_opt ) )
    {
        return { 0, float( e->get_choices().size() - 1 ), 1, (float)e->get_value_index( e->get_default_value() ) };
    }
    return { 0, 0, 0, 0 };
}


static rs2_option_type rs_type_from_dds_option( std::shared_ptr< realdds::dds_option > const & dds_opt )
{
    if( std::dynamic_pointer_cast< realdds::dds_float_option >( dds_opt ) )
        return RS2_OPTION_TYPE_FLOAT;
    if( std::dynamic_pointer_cast< realdds::dds_string_option >( dds_opt ) )
        return RS2_OPTION_TYPE_STRING;
    if( std::dynamic_pointer_cast< realdds::dds_boolean_option >( dds_opt ) )
        return RS2_OPTION_TYPE_BOOLEAN;
    if( std::dynamic_pointer_cast< realdds::dds_integer_option >( dds_opt ) )
        return RS2_OPTION_TYPE_INTEGER;
    if( std::dynamic_pointer_cast< realdds::dds_rect_option >( dds_opt ) )
        return RS2_OPTION_TYPE_RECT;
    throw not_implemented_exception( "unknown DDS option type" );
}


rs_dds_option::rs_dds_option( const std::shared_ptr< realdds::dds_option > & dds_opt,
                              set_option_callback set_opt_cb,
                              query_option_callback query_opt_cb )
    : option_base( range_from_realdds( dds_opt ) )
    , _dds_opt( dds_opt )
    , _rs_type( rs_type_from_dds_option( dds_opt ) )
    , _set_opt_cb( set_opt_cb )
    , _query_opt_cb( query_opt_cb )
{
}


void rs_dds_option::set( float value )
{
    if( ! _set_opt_cb )
        throw std::runtime_error( "Set option callback is not set for option " + _dds_opt->get_name() );

    // This is the legacy API for setting option values - it accepts a float. It's not always called via the rs2_...
    // APIs, but can be called from within librealsense, so we have to convert to the proper type:
    // (usually the range checks are only done at the level of rs2_...)

    auto validate_range = []( float const value, option_range const & range )
    {
        if( range.min != range.max && range.step )
        {
            if( value < range.min )
            {
                throw librealsense::invalid_value_exception(
                    rsutils::string::from() << "value (" << value << ") less than minimum (" << range.min << ")" );
            }
            if( value > range.max )
            {
                throw librealsense::invalid_value_exception(
                    rsutils::string::from() << "value (" << value << ") greater than maximum (" << range.max << ")" );
            }
        }
    };

    json j_value;
    switch( _rs_type )
    {
    case RS2_OPTION_TYPE_FLOAT:
        validate_range( value, get_range() );
        j_value = value;
        break;

    case RS2_OPTION_TYPE_INTEGER:
        validate_range( value, get_range() );
        if( (int) value != value )
            throw invalid_value_exception( rsutils::string::from() << "not an integer: " << value );
        j_value = int( value );
        break;

    case RS2_OPTION_TYPE_BOOLEAN:
        if( value == 0.f )
            j_value = false;
        else if( value == 1.f )
            j_value = true;
        else
            throw invalid_value_exception( rsutils::string::from() << "not a boolean: " << value );
        break;

    case RS2_OPTION_TYPE_STRING:
        // We can convert "enum" options to a float value
        if( auto desc = get_value_description( value ) )
            j_value = desc;
        else
            throw not_implemented_exception( "use rs2_set_option_value to set string values" );
        break;

    default:
        throw not_implemented_exception( rsutils::string::from()
                                         << "use rs2_set_option_value to set " << get_string( _rs_type ) << " value" );
    }

    if( is_read_only() )
        throw invalid_value_exception( "option is read-only: " + _dds_opt->get_name() );

    _set_opt_cb( j_value );
}


float rs_dds_option::query() const
{
    if( ! _query_opt_cb )
        throw std::runtime_error( "Query option callback is not set for option " + _dds_opt->get_name() );

    auto const value = _query_opt_cb();
    try
    {
        return value;  // try to convert to float
    }
    catch( std::exception const & )
    {
        throw invalid_value_exception( rsutils::string::from() << "option '" << _dds_opt->get_name() << "' value (" << value << ") is not a float" );
    }
}


json rs_dds_option::get_value() const noexcept
{
    return _dds_opt->get_value();
}


void rs_dds_option::set_value( json value )
{
    if( ! _set_opt_cb )
        throw std::runtime_error( "Set option callback is not set for option " + _dds_opt->get_name() );

    if( is_read_only() )
        throw invalid_value_exception( "option is read-only: " + _dds_opt->get_name() );

    _set_opt_cb( std::move( value ) );
}


bool rs_dds_option::is_read_only() const
{
    // Reporting locked through is_read_only() lets a UI disable the control instead of erroring on click.
    return _dds_opt->is_read_only() || ( _locked && _locked() );
}


bool rs_dds_option::is_enabled() const
{
    return ! _dds_opt->get_value().is_null();
}


const char * rs_dds_option::get_description() const
{
    return _dds_opt->get_description().c_str();
}


const char * rs_dds_option::get_value_description( float v ) const
{
    auto e = std::dynamic_pointer_cast< realdds::dds_enum_option >( _dds_opt );
    if( ! e )
        return nullptr;
    if( v < 0.f )
        return nullptr;
    auto & choices = e->get_choices();
    auto i = size_t( v + 0.005 );  // round to nearest integer value
    if( fabs( v - i ) > 0.01f )    // verify we're approx equal to it, since float == is not reliable
        return nullptr;
    if( i >= choices.size() )
        return nullptr;
    return choices[i].c_str();
}


}  // namespace librealsense
