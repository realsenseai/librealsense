// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.
#pragma once

#include <src/option.h>
#include <librealsense2/h/rs_option.h>
#include "command-transfer.h"
#include "uvc-device.h"
#include <src/uvc-sensor.h>
#include <rsutils/time/timer.h>


namespace librealsense {


class uvc_pu_option : public option
{
    std::weak_ptr< uvc_sensor > _ep;
    rs2_option _id;
    const std::map< float, std::string > _description_per_value;
    std::function< void( const option & ) > _record = []( const option & ) {};
    rsutils::lazy< option_range > _range;
    platform::processing_unit _pu = { 0, 0, 0 };
    bool _use_processing_unit = false;

    void initialize_range();

public:
    void set( float value ) override;

    float query() const override;

    option_range get_range() const override;

    bool is_enabled() const override { return true; }

    uvc_pu_option( const std::weak_ptr< uvc_sensor > & ep, rs2_option id );

    uvc_pu_option( const std::weak_ptr < uvc_sensor > & ep, rs2_option id,
                   const std::map< float, std::string > & description_per_value );

    uvc_pu_option( const std::weak_ptr< uvc_sensor > & ep,
                   rs2_option id,
                   platform::processing_unit pu );

    uvc_pu_option( const std::weak_ptr< uvc_sensor > & ep,
                   rs2_option id,
                   platform::processing_unit pu,
                   const std::map< float, std::string > & description_per_value );

    const char * get_description() const override;

    const char * get_value_description( float val ) const override
    {
        if( _description_per_value.find( val ) != _description_per_value.end() )
            return _description_per_value.at( val ).c_str();
        return nullptr;
    }
    void enable_recording( std::function< void( const option & ) > record_action ) override { _record = record_action; }
};


// XU control with exclusing access to setter/getters
template< typename T >
class uvc_xu_option : virtual public option
{
public:
    void set( float value ) override
    {
        auto ep = _ep.lock();
        if( ! ep )
            throw invalid_value_exception( "UVC sensor is not alive for setting" );

        if( ! _allow_set_while_streaming && ep->is_streaming() )
            throw invalid_value_exception( "setting this option during streaming is not allowed!" );

        ep->invoke_powered(
            [this, value]( platform::uvc_device & dev )
            {
                T t = static_cast< T >( value );
                if( ! dev.set_xu( _xu, _id, reinterpret_cast< uint8_t * >( &t ), sizeof( T ) ) )
                    throw invalid_value_exception( rsutils::string::from()
                                                   << "set_xu(id=" << std::to_string( _id ) << ") failed!"
                                                   << " Last Error: " << strerror( errno ) );
                _recording_function( *this );
            } );
    }

    float query() const override
    {
        auto ep = _ep.lock();
        if( ! ep )
            return static_cast< float >( T() );

        return static_cast< float >( ep->invoke_powered(
            [this]( platform::uvc_device & dev )
            {
                T t;
                if( ! dev.get_xu( _xu, _id, reinterpret_cast< uint8_t * >( &t ), sizeof( T ) ) )
                    throw invalid_value_exception( rsutils::string::from()
                                                   << "get_xu(id=" << _id << ") failed!"
                                                   << " Last Error: " << strerror( errno ) );
                if (_parsing_modifier)
                    return _parsing_modifier(t);

                return static_cast< float >( t );
            } ) );
    }

    option_range get_range() const override
    {
        auto uvc_range = platform::control_range();
        
        auto ep = _ep.lock();
        if( ep )
            uvc_range = ep->invoke_powered( [this]( platform::uvc_device & dev )
                                             { return dev.get_xu_range( _xu, _id, sizeof( T ) ); } );

        if( uvc_range.min.size() < sizeof( T ) )
            return option_range{ 0, 0, 1, 0 };

        auto min = *( reinterpret_cast< T* >( uvc_range.min.data() ) );
        auto max = *( reinterpret_cast< T* >( uvc_range.max.data() ) );
        auto step = *( reinterpret_cast< T* >( uvc_range.step.data() ) );
        auto def = *( reinterpret_cast< T* >( uvc_range.def.data() ) );

        if (_parsing_modifier)
        {
            return option_range{ _parsing_modifier( min ),
                                 _parsing_modifier( max ),
                                 _parsing_modifier( step ),
                                 _parsing_modifier( def ) };
        }
        return option_range{ static_cast< float >( min ),
                             static_cast< float >( max ),
                             static_cast< float >( step ),
                             static_cast< float >( def ) };
    }

    bool is_enabled() const override { return true; }

    typedef std::function< float(const T param) > parsing_modifier;

    uvc_xu_option( const std::weak_ptr< uvc_sensor > & ep,
                   platform::extension_unit xu,
                   uint8_t id,
                   std::string description,
                   bool allow_set_while_streaming = true,
                   parsing_modifier modifier = nullptr)
        : _ep( ep )
        , _xu( xu )
        , _id( id )
        , _desciption( std::move( description ) )
        , _allow_set_while_streaming( allow_set_while_streaming )
        , _parsing_modifier(modifier)
    {
    }

    uvc_xu_option( const std::weak_ptr< uvc_sensor > & ep,
                   platform::extension_unit xu,
                   uint8_t id,
                   std::string description,
                   const std::map< float, std::string > & description_per_value,
                   bool allow_set_while_streaming = true,
                   parsing_modifier modifier = nullptr)
        : _ep( ep )
        , _xu( xu )
        , _id( id )
        , _desciption( std::move( description ) )
        , _description_per_value( description_per_value )
        , _allow_set_while_streaming( allow_set_while_streaming )
        , _parsing_modifier(modifier)
    {
    }

    const char * get_description() const override { return _desciption.c_str(); }
    void enable_recording( std::function< void( const option & ) > record_action ) override
    {
        _recording_function = record_action;
    }
    const char * get_value_description( float val ) const override
    {
        if( _description_per_value.find( val ) != _description_per_value.end() )
            return _description_per_value.at( val ).c_str();
        return nullptr;
    }

protected:
    std::weak_ptr < uvc_sensor > _ep;
    platform::extension_unit _xu;
    uint8_t _id;
    std::string _desciption;
    std::function< void( const option & ) > _recording_function = []( const option & ) {
    };
    const std::map< float, std::string > _description_per_value;
    bool _allow_set_while_streaming;
    parsing_modifier _parsing_modifier = nullptr;
};


template< typename T >
class protected_xu_option : public uvc_xu_option< T >
{
public:
    protected_xu_option( const std::weak_ptr< uvc_sensor > & ep,
                         platform::extension_unit xu,
                         uint8_t id,
                         std::string description )
        : uvc_xu_option< T >( ep, xu, id, description )
    {
    }

    protected_xu_option( const std::weak_ptr< uvc_sensor > & ep,
                         platform::extension_unit xu,
                         uint8_t id,
                         std::string description,
                         const std::map< float, std::string > & description_per_value )
        : uvc_xu_option< T >( ep, xu, id, description, description_per_value )
    {
    }

    void set( float value ) override
    {
        std::lock_guard< std::mutex > lk( _mtx );
        uvc_xu_option< T >::set( value );
    }

    float query() const override
    {
        std::lock_guard< std::mutex > lk( _mtx );
        return uvc_xu_option< T >::query();
    }

protected:
    mutable std::mutex _mtx;
};


template< typename T >
class ensure_set_xu_option : public uvc_xu_option< T >
{
public:
    ensure_set_xu_option( const std::weak_ptr< uvc_sensor > & ep,
                          platform::extension_unit xu,
                          uint8_t id,
                          std::string description,
                          std::chrono::milliseconds timeout = std::chrono::milliseconds( 1000 ),
                          bool allow_set_while_streaming = true )
        : uvc_xu_option< T >( ep, xu, id, description, allow_set_while_streaming )
        , _timer( timeout )
    {
    }

    ensure_set_xu_option( const std::weak_ptr< uvc_sensor > & ep,
                          platform::extension_unit xu,
                          uint8_t id,
                          std::string description,
                          const std::map< float, std::string > & description_per_value,
                          std::chrono::milliseconds timeout = std::chrono::milliseconds( 1000 ),
                          bool allow_set_while_streaming = true )
        : uvc_xu_option< T >( ep, xu, id, description, description_per_value, allow_set_while_streaming )
        , _timer( timeout )
    {
    }

    void set( float value ) override
    {
        uvc_xu_option< T >::set( value );

        // Check that set operation succeeded
        _timer.start();
        do
        {
            std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );

            float current_value = uvc_xu_option< T >::query();
            if( current_value == value )
                return;
        }
        while( ! _timer.has_expired() );

        throw std::runtime_error( rsutils::string::from() << "Failed to set the option to value " << value );
    }

protected:
    rsutils::time::timer _timer;
};


class command_transfer_over_xu : public platform::command_transfer
{
    uvc_sensor & _uvc;
    platform::extension_unit _xu;
    uint8_t _ctrl;

public:
    std::vector< uint8_t > send_receive( uint8_t const * pb, size_t cb, int, bool require_response ) override;

    command_transfer_over_xu( uvc_sensor & uvc, platform::extension_unit xu, uint8_t ctrl )
        : _uvc( uvc )
        , _xu( std::move( xu ) )
        , _ctrl( ctrl )
    {
    }
};


}  // namespace librealsense
