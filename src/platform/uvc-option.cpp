// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#include "uvc-option.h"
#include <src/hw-monitor.h>


namespace librealsense {


uvc_pu_option::uvc_pu_option( const std::weak_ptr< uvc_sensor > & ep, rs2_option id )
    : uvc_pu_option(ep, id, std::map<float, std::string>())
{    
}


uvc_pu_option::uvc_pu_option( const std::weak_ptr< uvc_sensor > & ep, rs2_option id,
                              const std::map< float, std::string > & description_per_value )
    : _ep(ep), _id(id), _description_per_value(description_per_value)
{
    initialize_range();
}


uvc_pu_option::uvc_pu_option( const std::weak_ptr< uvc_sensor > & ep,
                              rs2_option id,
                              platform::processing_unit pu )
    : uvc_pu_option( ep, id, pu, std::map< float, std::string >() )
{
}


uvc_pu_option::uvc_pu_option( const std::weak_ptr< uvc_sensor > & ep,
                              rs2_option id,
                              platform::processing_unit pu,
                              const std::map< float, std::string > & description_per_value )
    : _ep( ep )
    , _id( id )
    , _description_per_value( description_per_value )
    , _pu( pu )
    , _use_processing_unit( true )
{
    initialize_range();
}


void uvc_pu_option::initialize_range()
{
    _range = [this]()
    {
        auto ep = _ep.lock();
        if( ! ep )
            throw invalid_value_exception( "Cannot set range, UVC sensor is not alive" );

        auto uvc_range = ep->invoke_powered( [this]( platform::uvc_device & dev )
            {
                if( _use_processing_unit )
                    return dev.get_pu_range( _pu, _id );
                return dev.get_pu_range( _id );
            });

        if (uvc_range.min.size() < sizeof(int32_t)) return option_range{ 0,0,1,0 };

        auto min = *(reinterpret_cast<int32_t*>(uvc_range.min.data()));
        auto max = *(reinterpret_cast<int32_t*>(uvc_range.max.data()));
        auto step = *(reinterpret_cast<int32_t*>(uvc_range.step.data()));
        auto def = *(reinterpret_cast<int32_t*>(uvc_range.def.data()));
        return option_range{ static_cast<float>(min),
                            static_cast<float>(max),
                            static_cast<float>(step),
                            static_cast<float>(def) };
    };
}


void uvc_pu_option::set(float value)
{
    auto ep = _ep.lock();
    if( ! ep )
        throw invalid_value_exception( "Cannot set option, UVC sensor is not alive" );
    ep->invoke_powered(
        [this, value](platform::uvc_device& dev)
        {
            auto const success = _use_processing_unit
                ? dev.set_pu( _pu, _id, static_cast< int32_t >( value ) )
                : dev.set_pu( _id, static_cast< int32_t >( value ) );
            if( ! success )
            throw invalid_value_exception( rsutils::string::from()
                                           << "set_pu(id=" << std::to_string( _id ) << ") failed!"
                                           << " Last Error: " << strerror( errno ) );
            _record(*this);
        });
}


float uvc_pu_option::query() const
{
    auto ep = _ep.lock();
    if( ! ep )
        throw invalid_value_exception( "Cannot query option, UVC sensor is not alive" );
    return static_cast<float>(ep->invoke_powered(
        [this](platform::uvc_device& dev)
        {
            int32_t value = 0;
            auto const success = _use_processing_unit
                ? dev.get_pu( _pu, _id, value )
                : dev.get_pu( _id, value );
            if( ! success )
                throw invalid_value_exception( rsutils::string::from()
                                               << "get_pu(id=" << std::to_string( _id ) << ") failed!"
                                               << " Last Error: " << strerror( errno ) );

            return static_cast<float>(value);
        }));
}


option_range uvc_pu_option::get_range() const
{
    return *_range;
}


const char* uvc_pu_option::get_description() const
{
    switch(_id)
    {
    case RS2_OPTION_BACKLIGHT_COMPENSATION: return "Enable / disable backlight compensation";
    case RS2_OPTION_BRIGHTNESS: return "UVC image brightness";
    case RS2_OPTION_CONTRAST: return "UVC image contrast";
    case RS2_OPTION_EXPOSURE: return "Controls exposure time of color camera. Setting any value will disable auto exposure";
    case RS2_OPTION_GAIN: return "UVC image gain";
    case RS2_OPTION_GAMMA: return "UVC image gamma setting";
    case RS2_OPTION_HUE: return "UVC image hue";
    case RS2_OPTION_SATURATION: return "UVC image saturation setting";
    case RS2_OPTION_SHARPNESS: return "UVC image sharpness setting";
    case RS2_OPTION_WHITE_BALANCE: return "Controls white balance of color image. Setting any value will disable auto white balance";
    case RS2_OPTION_ENABLE_AUTO_EXPOSURE: return "Enable / disable auto-exposure";
    case RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE: return "Enable / disable auto-white-balance";
    case RS2_OPTION_POWER_LINE_FREQUENCY: return "Power Line Frequency";
    case RS2_OPTION_AUTO_EXPOSURE_PRIORITY: return "Restrict Auto-Exposure to enforce constant FPS rate. Turn ON to remove the restrictions (may result in FPS drop)";
    default:
        auto ep = _ep.lock();
        if( ! ep )
            throw invalid_value_exception( "Cannot get option name, UVC sensor is not alive" );

        return ep->get_option_name( _id ).c_str();
    }
}


std::vector<uint8_t> command_transfer_over_xu::send_receive( uint8_t const * const pb, size_t const cb, int, bool require_response)
{
    return _uvc.invoke_powered([this, pb, cb, require_response]( platform::uvc_device & dev )
        {
            std::vector<uint8_t> result;

            std::lock_guard< platform::uvc_device > lock(dev);

            if( cb > HW_MONITOR_BUFFER_SIZE )
            {
                LOG_ERROR( "XU command size is invalid" );
                throw invalid_value_exception( rsutils::string::from()
                                               << "Requested XU command size " << std::dec << cb
                                               << " exceeds permitted limit " << HW_MONITOR_BUFFER_SIZE );
            }

            std::vector< uint8_t > transmit_buf( HW_MONITOR_BUFFER_SIZE, 0 );
            std::copy( pb, pb + cb, transmit_buf.begin() );

            if( ! dev.set_xu( _xu, _ctrl, transmit_buf.data(), static_cast< int >( transmit_buf.size() ) ) )
                throw invalid_value_exception( rsutils::string::from()
                                               << "set_xu(ctrl=" << unsigned( _ctrl ) << ") failed!"
                                               << " Last Error: " << strerror( errno ) );

            if( require_response )
            {
                result.resize( HW_MONITOR_BUFFER_SIZE );
                if( ! dev.get_xu( _xu, _ctrl, result.data(), static_cast< int >( result.size() ) ) )
                    throw invalid_value_exception( rsutils::string::from()
                                                   << "get_xu(ctrl=" << unsigned( _ctrl ) << ") failed!"
                                                   << " Last Error: " << strerror( errno ) );

                // Returned data size located in the last 4 bytes
                auto data_size = *( reinterpret_cast< uint32_t * >( result.data() + HW_MONITOR_DATA_SIZE_OFFSET ) )
                               + SIZE_OF_HW_MONITOR_HEADER;
                result.resize( data_size );
            }

            return result;
        });
}


}
