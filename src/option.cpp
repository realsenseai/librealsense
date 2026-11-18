// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#include "option.h"
#include "sensor.h"

#include <rsutils/string/from.h>
#include <rsutils/json.h>

using rsutils::json;


namespace librealsense {


bool option_base::is_valid(float value) const
{
    if (!std::isnormal(_opt_range.step) && _opt_range.step != 0)
        throw invalid_value_exception( rsutils::string::from()
                                       << "is_valid(...) failed! step is not properly defined. (" << _opt_range.step
                                       << ")" );

    if ((value < _opt_range.min) || (value > _opt_range.max))
        return false;

    if (_opt_range.step == 0)
        return true;

    auto n = (value - _opt_range.min) / _opt_range.step;
    return (fabs(fmod(n, 1)) < std::numeric_limits<float>::min());
}

option_range option_base::get_range() const
{
    return _opt_range;
}
void option_base::enable_recording(std::function<void(const option&)> recording_action)
{
    _recording_function = recording_action;
}

json option::get_value() const noexcept
{
    json value;
    try
    {
        value = query();
    }
    catch( ... )
    {
        // Sometimes option values may not be available, meaning the value stays null
    }
    return value;
}


rs2_option_type option::get_value_type() const noexcept
{
    // By default, all options are floats
    return RS2_OPTION_TYPE_FLOAT;
}


void option::set_value( json value )
{
    set( value );
}


void option::create_snapshot(std::shared_ptr<option>& snapshot) const
{
    snapshot = std::make_shared<const_value_option>(get_description(), query());
}

void float_option::set(float value)
{
    if (!is_valid(value))
        throw invalid_value_exception( rsutils::string::from()
                                       << "set(...) failed! " << value << " is not a valid value" );
    _value = value;
}


void auto_disabling_control::set( float value )
{
    auto strong = _affected_control.lock();
    if( ! strong )
        return;

    auto move_to_manual = false;
    auto val = strong->query();

    if( std::find( _move_to_manual_values.begin(), _move_to_manual_values.end(), val ) != _move_to_manual_values.end() )
    {
        move_to_manual = true;
    }

    if( move_to_manual )
    {
        LOG_DEBUG( "Move option to manual mode in order to set a value" );
        strong->set( _manual_value );
    }
    _proxy->set( value );
    _recording_function( *this );
}


void gated_option::set( float value )
{
    bool gated_set = false;
    for( auto & gated : _gated_options )
    {
        auto strong = gated.first.lock();
        if( ! strong )
            continue;  // if gated option is not available, step over it
        auto val = strong->query();
        if( val )
        {
            gated_set = true;
            LOG_WARNING( gated.second.c_str() );
        }
    }

    if( ! gated_set )
        _proxy->set( value );

    _recording_function( *this );
}


void gated_by_value_option::set( float requested_option_value )
{
    for( auto & gate : _gating_options )
    {
        auto gating_option = std::get< 0 >( gate ).lock();
        if( ! gating_option )
            throw std::runtime_error( rsutils::string::from() << "Gating option not alive. " << std::get< 2 >( gate ) );

        auto wanted_gate_value = std::get< 1 >( gate );
        auto current_gate_value = gating_option->query();
        if( current_gate_value != wanted_gate_value )
            throw librealsense::invalid_value_exception( std::get< 2 >( gate ) );
    }

    _proxy->set( requested_option_value );

    _recording_function( *this );
}


void locked_option::set( float value )
{
    if( _is_locked() )
        throw invalid_value_exception( _reason );
    proxy_option::set( value );
    _recording_function( *this );
}


void min_distance_option::set( float value )
{
    auto strong = _max_option.lock();
    if( ! strong )
        return;

    auto max_value = strong->query();

    if( max_value < value )
    {
        auto max = strong->get_range().max;
        strong->set( max );
    }
    _proxy->set( value );
    _recording_function( *this );

    notify( value );
}


void max_distance_option::set( float value )
{
    auto strong = _min_option.lock();
    if( ! strong )
        return;

    auto min_value = strong->query();

    if( min_value > value )
    {
        auto min = strong->get_range().min;
        strong->set( min );
    }
    _proxy->set( value );
    _recording_function( *this );

    notify( value );
}



}