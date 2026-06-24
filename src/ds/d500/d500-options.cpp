// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.


#include "d500-options.h"
#include "d500-private.h"

namespace librealsense
{
    rgb_tnr_option::rgb_tnr_option(std::shared_ptr<hw_monitor> hwm, const std::weak_ptr< sensor_base > & ep)
        : _hwm(hwm), _sensor(ep)
    {
        _range = [this]()
        {
            return option_range{ 0, 1, 1, 0 };
        };
    }

    void rgb_tnr_option::set(float value)
    {
        auto strong_sensor = _sensor.lock();
        if( ! strong_sensor )
            throw std::runtime_error( "Cannot set option as sensor is not alive" );

        if (strong_sensor->is_streaming())
            throw std::runtime_error("Cannot change RGB TNR option while streaming!");

        command cmd(ds::RGB_TNR);
        cmd.param1 = SET_TNR_STATE;
        cmd.param2 = static_cast<int>(value);

        _hwm->send(cmd);
        _record_action(*this);
    }

    float rgb_tnr_option::query() const
    {
        command cmd(ds::RGB_TNR);
        cmd.param1 = GET_TNR_STATE;

        auto res = _hwm->send(cmd);
        if (res.empty())
            throw invalid_value_exception("rgb_tnr_option::query result is empty!");

        return (res.front());
    }

    option_range rgb_tnr_option::get_range() const
    {
        return *_range;
    }

    d500_align_depth_option::d500_align_depth_option( const std::weak_ptr< uvc_sensor > & ep )
        : uvc_xu_option< uint8_t >( ep,
                                    ds::depth_xu,
                                    ds::DS5_ALIGN_DEPTH,
                                    "Enable firmware depth-to-color alignment for object detection distance. Can only be set before depth streaming",
                                    { { 0.f, "Disabled" }, { 1.f, "Enabled" } },
                                    false )
    {
    }

    option_range d500_align_depth_option::get_range() const
    {
        return option_range{ 0, 1, 1, 0 };
    }

    temperature_option::temperature_option( std::shared_ptr< hw_monitor > hwm,
                                            temperature_component component,
                                            const char * description )
        : _hwm( hwm )
        , _component( component )
        , _description( description )
    {
        _range = [this]()
        {
            return option_range{ -127, 128, 1, 30 };
        };
    }

    float temperature_option::query() const
    {
        if (!is_enabled() || !_hwm)
            throw wrong_api_call_sequence_exception("error occurred in the temperature reading");

        float temperature = -1;
        try {
            command cmd(ds::fw_cmd::GTEMP, static_cast<int>(_component));
            auto res = _hwm->send(cmd);
            auto res_16 = reinterpret_cast<uint16_t*>(res.data());
            auto offset_for_component = static_cast<int>(_component) - 1;
            uint16_t result_for_component = res_16[offset_for_component];
            if (result_for_component == 0xffff)
                temperature = 0.f;
            else
            {
                // parsing the temperature result: 0xABCD:
                // whole number = 0xCD - int8_t, 
                // decimal part = 0xAB - uint8_t
                int8_t whole_number_part = static_cast<int8_t>(result_for_component >> 8 & 0xff);
                uint8_t decimal_part = static_cast<uint8_t>(result_for_component & 0x0ff);
                temperature = static_cast<float>(whole_number_part) + decimal_part / 256.f;
            }
        }
        catch (...)
        {
            throw wrong_api_call_sequence_exception("hw monitor command for reading temperature failed");
        }

        return temperature;
    }
    temperature_xu_option::temperature_xu_option(const std::weak_ptr<uvc_sensor>& ep, 
        platform::extension_unit xu, uint8_t id, 
        std::string description)
        : uvc_xu_option<int16_t>(ep, xu, id, description, false,
            // defining the parsing modifier, to be used on the calls for query and get_range methods
            [](const int16_t read_value) {
                return static_cast<float>(read_value) / 10.f;
            }) {}

    float temperature_xu_option::query() const
    {
        return uvc_xu_option<int16_t>::query();
    }

    void temperature_xu_option::set(float value)
    {
        readonly_option::set(value);
    }

    d500_thermal_compensation_option::d500_thermal_compensation_option( std::shared_ptr< hw_monitor > hwm )
        : _hwm( hwm )
    {
    }

    void d500_thermal_compensation_option::set( float value )
    {
        if( auto hwm = _hwm.lock() )
        {
            int on_off = value > 0 ? 1 : 0; // 1 toggle on, 0 off
            int param1 = 5; // received from HKR team, probably means toggle
            try
            {
                command cmd( ds::fw_cmd::HKR_THERMAL_COMPENSATION, param1, on_off );
                hwm->send( cmd );
                _value = value; // Currently no way to query actual value, save set value.
            }
            catch( ... )
            {
                throw wrong_api_call_sequence_exception( "hw monitor command for setting thermal compensation failed" );
            }
        }
    }

    power_line_freq_option::power_line_freq_option(const std::weak_ptr< uvc_sensor >& ep, rs2_option id,
        const std::map< float, std::string >& description_per_value) :
        uvc_pu_option(ep, id, description_per_value) {}
    
    d500_external_sync_mode::d500_external_sync_mode( hw_monitor & hwm, const std::weak_ptr< sensor_base > & ep,
                                                      const std::map< float, std::string > & description_per_value )
        : _hwm( hwm )
        , _sensor( ep )
        , _description_per_value( description_per_value )
    {
        _range = { RS2_D500_INTERCAM_SYNC_NONE,
                   RS2_D500_INTERCAM_SYNC_EXTERNAL_MASTER,
                   1,
                   RS2_D500_INTERCAM_SYNC_NONE };
    }

    void d500_external_sync_mode::set( float value )
    {
        auto strong_sensor = _sensor.lock();
        if( ! strong_sensor )
            throw invalid_value_exception( "Cannot set option as sensor is not alive" );

        if( strong_sensor->is_streaming() )
            throw std::runtime_error( "Cannot change external sync mode while streaming!" );

        if( ! is_valid( static_cast < rs2_d500_intercam_sync_mode >( value ) ) )
            throw invalid_value_exception( rsutils::string::from()
                                           << "d500_external_sync_mode::set invalid value " << value );

        command cmd( ds::SET_CAM_SYNC );

        cmd.param1 = static_cast< int >( value );
        cmd.require_response = false;

        _hwm.send( cmd );
        _record_action( *this );
    }

    float d500_external_sync_mode::query() const
    {
        command cmd( ds::GET_CAM_SYNC );
        auto res = _hwm.send( cmd );
        if( res.empty() )
            throw invalid_value_exception( "d500_external_sync_mode::query result is empty!" );

        return static_cast< float >( res[0] );
    }

    bool d500_external_sync_mode::is_read_only() const 
    { 
        auto strong_sensor = _sensor.lock();
        return strong_sensor && strong_sensor->is_opened();
    }

    const char * d500_external_sync_mode::get_value_description( float val ) const
    {
        try
        {
            return _description_per_value.at( val ).c_str();
        }
        catch( std::out_of_range )
        {
            throw invalid_value_exception( rsutils::string::from()
                                           << "d500_external_sync_mode description of value " << val << " not found." );
        }
    }
} // namespace librealsense
