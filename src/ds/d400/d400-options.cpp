// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2016 RealSense, Inc. All Rights Reserved.


#include "d400-options.h"

#include <rsutils/string/from.h>
#include <src/hid-sensor.h>

namespace librealsense
{

    asic_temperature_option_mipi::asic_temperature_option_mipi(std::shared_ptr<hw_monitor> hwm, rs2_option opt)
        : _hw_monitor(hwm), _option(opt)
    {}

    float asic_temperature_option_mipi::query() const
    {
        if (!is_enabled() || !_hw_monitor)
            throw wrong_api_call_sequence_exception("query is available during streaming only");

        float temperature = -1;
        try {
            command cmd(ds::ASIC_TEMP_MIPI);
            auto res = _hw_monitor->send(cmd);
            temperature = static_cast<float>(res[0]);
        }
        catch (...)
        {
            throw wrong_api_call_sequence_exception("hw monitor command for asic temperature failed");
        }

        return temperature;
    }

    option_range asic_temperature_option_mipi::get_range() const
    {
        return option_range{ -40, 125, 0, 0 };
    }

    thermal_compensation_option_mipi::thermal_compensation_option_mipi( std::shared_ptr< hw_monitor > hwm )
        : bool_option( true )
        , _hw_monitor( hwm )
    {}

    void thermal_compensation_option_mipi::set( float value )
    {
        auto hwm = _hw_monitor.lock();
        if( ! hwm )
            throw wrong_api_call_sequence_exception( "hw monitor is not available for thermal compensation" );

        try
        {
            command cmd( ds::TC_CMD, ds::TC_CMD_SWITCH, value > 0 ? 1 : 0 );
            hwm->send( cmd );
        }
        catch( const std::exception & e )
        {
            throw wrong_api_call_sequence_exception(
                std::string( "hw monitor command for setting thermal compensation failed: " ) + e.what() );
        }

        bool_option::set( value );
        _recording_function( *this );
    }

    projector_temperature_option_mipi::projector_temperature_option_mipi(std::shared_ptr<hw_monitor> hwm, rs2_option opt)
        : _hw_monitor(hwm), _option(opt)
    {}

    float projector_temperature_option_mipi::query() const
    {
        if (!is_enabled() || !_hw_monitor)
            throw wrong_api_call_sequence_exception("query is available during streaming only");

        float temperature;
        try {
            command cmd(ds::GTEMP);
            auto res = _hw_monitor->send(cmd);
            temperature = static_cast<float>(res[0]);
        }
        catch (...)
        {
            throw wrong_api_call_sequence_exception("hw monitor command for projector temperature failed");
        }

        return temperature;
    }

    option_range projector_temperature_option_mipi::get_range() const
    {
        return option_range{ -40, 125, 0, 0 };
    }

    auto_exposure_limit_option::auto_exposure_limit_option( hw_monitor & hwm,
                                                            const std::weak_ptr< sensor_base > & depth_ep,
                                                            option_range range,
                                                            std::shared_ptr< limits_option > exposure_limit_enable,
                                                            bool new_opcode )
        : option_base( range )
        , _hwm( hwm )
        , _sensor( depth_ep )
        , _exposure_limit_toggle( exposure_limit_enable )
        , _new_opcode( new_opcode )
    {
        _range = [range]()
        {
            return range;
        };
        if (auto toggle = _exposure_limit_toggle.lock())
            toggle->set_cached_limit(range.max);
    }

    void auto_exposure_limit_option::set(float value)
    {
        if (!is_valid(value))
            throw invalid_value_exception("set(enable_auto_exposure) failed! Invalid Auto-Exposure mode request " + std::to_string(value));

        if (auto toggle = _exposure_limit_toggle.lock())
        {
            toggle->set_cached_limit(value);
            if (toggle->query() == 0.f)
                toggle->set(1);
        }

        if( _new_opcode )
            set_using_new_opcode( value );
        else
            set_using_old_opcode( value );

        _record_action( *this );
        
    }

    float auto_exposure_limit_option::query() const
    {
        float ret = _new_opcode ? query_using_new_opcode() : query_using_old_opcode();
        
        if( ret < get_range().min || ret > get_range().max )
        {
            if( auto toggle = _exposure_limit_toggle.lock() )
                return toggle->get_cached_limit();
        }
        return ret;
    }

    option_range auto_exposure_limit_option::get_range() const
    {
        return *_range;
    }

    bool auto_exposure_limit_option::is_read_only() const
    {
        if( auto strong = _sensor.lock() )
            return strong->is_opened();
        return false;
    }

    void auto_exposure_limit_option::set_using_new_opcode( float value )
    {
        // get structure: min ae, min gain, max ae, max gain
        command cmd_get( ds::GETAELIMITS );
        std::vector< uint8_t > ret = _hwm.send( cmd_get );
        if( ret.empty() )
            throw invalid_value_exception( "auto_exposure_limit_option::query result is empty!" );

        static const int max_gain_offset = 12;
        // set structure: min ae, max ae, min gain, max gain
        command cmd( ds::SETAELIMITS );
        cmd.param1 = 0;
        cmd.param2 = static_cast< int >( value );
        cmd.param3 = 0;
        cmd.param4 = *( reinterpret_cast< uint32_t * >( ret.data() + max_gain_offset ) );
        _hwm.send( cmd );
    }

    void auto_exposure_limit_option::set_using_old_opcode( float value )
    {
        command cmd_get( ds::AUTO_CALIB );
        cmd_get.param1 = 5;
        std::vector< uint8_t > ret = _hwm.send( cmd_get );
        if( ret.empty() )
            throw invalid_value_exception( "auto_exposure_limit_option::query result is empty!" );

        static const int max_gain_offset = 4;
        command cmd( ds::AUTO_CALIB );
        cmd.param1 = 4;
        cmd.param2 = static_cast< int >( value );
        cmd.param3 = *( reinterpret_cast< uint32_t * >( ret.data() + max_gain_offset ) );
        _hwm.send( cmd );
    }

    float auto_exposure_limit_option::query_using_new_opcode() const
    {
        // get structure: min ae, min gain, max ae, max gain
        command cmd( ds::GETAELIMITS );

        auto res = _hwm.send( cmd );
        if( res.empty() )
            throw invalid_value_exception( "auto_exposure_limit_option::query result is empty!" );

        static const int max_ae_offset = 8;
        return static_cast< float >( *( reinterpret_cast< uint32_t * >( res.data() + max_ae_offset ) ) );
    }

    float auto_exposure_limit_option::query_using_old_opcode() const
    {
        command cmd( ds::AUTO_CALIB );
        cmd.param1 = 5;

        auto res = _hwm.send( cmd );
        if( res.empty() )
            throw invalid_value_exception( "auto_exposure_limit_option::query result is empty!" );

        return static_cast< float >( *( reinterpret_cast< uint32_t * >( res.data() ) ) );
    }

    auto_gain_limit_option::auto_gain_limit_option( hw_monitor & hwm,
                                                    const std::weak_ptr< sensor_base > & depth_ep,
                                                    option_range range,
                                                    std::shared_ptr< limits_option > gain_limit_enable,
                                                    bool new_opcode )
        : option_base( range )
        , _hwm( hwm )
        , _sensor( depth_ep )
        , _gain_limit_toggle( gain_limit_enable )
        , _new_opcode( new_opcode )
    {
        _range = [range]()
        {
            return range;
        };
        if (auto toggle = _gain_limit_toggle.lock())
            toggle->set_cached_limit(range.max);
    }

    
    void auto_gain_limit_option::set(float value)
    {
        if (!is_valid(value))
            throw invalid_value_exception("set(enable_auto_gain) failed! Invalid Auto-Gain mode request " + std::to_string(value));
          
        if (auto toggle = _gain_limit_toggle.lock())
        {
            toggle->set_cached_limit(value);
            if (toggle->query() == 0.f)
                toggle->set(1);
        }
            
        if( _new_opcode )
            set_using_new_opcode( value );
        else
            set_using_old_opcode( value );

        _record_action( *this );
        
    }

    float auto_gain_limit_option::query() const
    {
        float ret = _new_opcode ? query_using_new_opcode() : query_using_old_opcode();
        
        if (ret< get_range().min || ret > get_range().max)
        {
            if (auto toggle = _gain_limit_toggle.lock())
                return toggle->get_cached_limit();
        }
        return ret;
    }

    option_range auto_gain_limit_option::get_range() const
    {
        return *_range;
    }

    bool auto_gain_limit_option::is_read_only() const
    {
        if( auto strong = _sensor.lock() )
            return strong->is_opened();
        return false;
    }

    void auto_gain_limit_option::set_using_new_opcode( float value )
    {
        // get structure: min ae, min gain, max ae, max gain
        command cmd_get( ds::GETAELIMITS );
        std::vector< uint8_t > ret = _hwm.send( cmd_get );
        if( ret.empty() )
            throw invalid_value_exception( "auto_exposure_limit_option::query result is empty!" );

        static const int max_ae_offset = 8;
        // set structure: min ae, max ae, min gain, max gain
        command cmd( ds::SETAELIMITS );
        cmd.param1 = 0;
        cmd.param2 = *( reinterpret_cast< uint32_t * >( ret.data() + max_ae_offset ) );
        cmd.param3 = 0;
        cmd.param4 = static_cast< int >( value );
        _hwm.send( cmd );
    }

    void auto_gain_limit_option::set_using_old_opcode( float value )
    {
        command cmd_get( ds::AUTO_CALIB );
        cmd_get.param1 = 5;
        std::vector< uint8_t > ret = _hwm.send( cmd_get );
        if( ret.empty() )
            throw invalid_value_exception( "auto_exposure_limit_option::query result is empty!" );

        command cmd( ds::AUTO_CALIB );
        cmd.param1 = 4;
        cmd.param2 = *( reinterpret_cast< uint32_t * >( ret.data() ) );
        cmd.param3 = static_cast< int >( value );
        _hwm.send( cmd );
    }

    float auto_gain_limit_option::query_using_new_opcode() const
    {
        // get structure: min ae, min gain, max ae, max gain
        command cmd( ds::GETAELIMITS );

        auto res = _hwm.send( cmd );
        if( res.empty() )
            throw invalid_value_exception( "auto_exposure_limit_option::query result is empty!" );

        static const int max_gain_offset = 12;
        return static_cast< float >( *( reinterpret_cast< uint32_t * >( res.data() + max_gain_offset ) ) );
    }

    float auto_gain_limit_option::query_using_old_opcode() const
    {
        command cmd( ds::AUTO_CALIB );
        cmd.param1 = 5;

        auto res = _hwm.send( cmd );
        if( res.empty() )
            throw invalid_value_exception( "auto_exposure_limit_option::query result is empty!" );

        static const int max_gain_offset = 4;
        return static_cast< float >( *( reinterpret_cast< uint32_t * >( res.data() + max_gain_offset ) ) );
    }

    limits_option::limits_option(
        rs2_option option, option_range range, const char * description, hw_monitor & hwm, bool new_opcode )
        : _option( option )
        , _toggle_range( range )
        , _description( description )
        , _hwm( hwm )
        , _new_opcode( new_opcode ){}

    void limits_option::set( float value ) 
    {
        auto set_limit = _cached_limit;
        if( value == 0 )  // 0: gain auto-limit is disabled, 1 : gain auto-limit is enabled (all range 16-248 is valid)
            set_limit = 0;

        if( _new_opcode )
            set_using_new_opcode(value, set_limit );
        else
            set_using_old_opcode( value, set_limit );
    }

    float limits_option::query() const
    {
        int offset = 0;
        std::vector< uint8_t > res;
        if( _new_opcode )
        {
            offset = 8;
            if( _option == RS2_OPTION_AUTO_GAIN_LIMIT_TOGGLE )
                offset = 12;
            res = query_using_new_opcode();
        }
        else
        {
            if( _option == RS2_OPTION_AUTO_GAIN_LIMIT_TOGGLE )
                offset = 4;
            res = query_using_old_opcode();
        }

        if( res.empty() )
            throw invalid_value_exception( "auto_exposure_limit_option::query result is empty!" );
        float limit_val = static_cast< float >( *( reinterpret_cast< uint32_t * >( res.data() + offset ) ) );

        if( limit_val == 0 )
            return 0;
        return 1;
    }

    void limits_option::set_using_new_opcode( float value, float set_limit )
    {
        // get structure: min ae, min gain, max ae, max gain
        command cmd_get( ds::GETAELIMITS );
        std::vector< uint8_t > ret = _hwm.send( cmd_get );
        if( ret.empty() )
            throw invalid_value_exception( "auto_exposure_limit_option::query result is empty!" );

        int offset = 8;
        // set structure: min ae, max ae, min gain, max gain
        command cmd( ds::SETAELIMITS );
        cmd.param1 = 0;
        cmd.param2 = *( reinterpret_cast< uint32_t * >( ret.data() + offset ) );
        cmd.param3 = 0;
        cmd.param4 = static_cast< int >( set_limit );
        if( _option == RS2_OPTION_AUTO_EXPOSURE_LIMIT_TOGGLE )
        {
            offset = 12;
            cmd.param2 = static_cast< int >( set_limit );
            cmd.param4 = *( reinterpret_cast< uint32_t * >( ret.data() + offset ) );
        }
        _hwm.send( cmd );
    }

    void limits_option::set_using_old_opcode( float value, float set_limit )
    {
        command cmd_get( ds::AUTO_CALIB );
        cmd_get.param1 = 5;
        std::vector< uint8_t > ret = _hwm.send( cmd_get );
        if( ret.empty() )
            throw invalid_value_exception( "auto_exposure_limit_option::query result is empty!" );

        int offset = 0;
        command cmd( ds::AUTO_CALIB );
        cmd.param1 = 4;
        cmd.param2 = *( reinterpret_cast< uint32_t * >( ret.data() + offset ) );
        cmd.param3 = static_cast< int >( set_limit );
        if( _option == RS2_OPTION_AUTO_EXPOSURE_LIMIT_TOGGLE )
        {
            offset = 4;
            cmd.param2 = static_cast< int >( set_limit );
            cmd.param3 = *( reinterpret_cast< uint32_t * >( ret.data() + offset ) );
        }
        _hwm.send( cmd );
    }

    std::vector< uint8_t > limits_option::query_using_new_opcode() const
    {
        // get structure: min ae, min gain, max ae, max gain
        command cmd( ds::GETAELIMITS );
        return _hwm.send( cmd );
    }

    std::vector< uint8_t > limits_option::query_using_old_opcode() const
    {
        command cmd( ds::AUTO_CALIB );
        cmd.param1 = 5;
        return _hwm.send( cmd );
    }

    void librealsense::gyro_sensitivity_option::set( float value ) 
    {
        auto strong = _sensor.lock();
        if( ! strong )
            throw invalid_value_exception( "Hid sensor is not alive for setting" );

        if( strong->is_streaming() )
            throw invalid_value_exception( "setting this option during streaming is not allowed!" );

        if(!is_valid(value))
            throw invalid_value_exception( "set(gyro_sensitivity) failed! Invalid Gyro sensitivity resolution request "
                                           + std::to_string( value ) );

        _value = value;
        _user_set = true;
        strong->set_imu_sensitivity( RS2_STREAM_GYRO, value );
    }

    float librealsense::gyro_sensitivity_option::query() const
    {
        // Until the user sets a value, follow the FW (like DDS and GMSL do)
        if( ! _user_set && ! _fw_value_read )
        {
            auto strong = _sensor.lock();
            float fw_value;
            if( strong && ! strong->is_opened() && strong->read_fw_imu_sensitivity( RS2_STREAM_GYRO, fw_value ) )
            {
                _value = fw_value;
                _fw_value_read = true;
            }
        }
        return _value;
    }

    
    const char * librealsense::gyro_sensitivity_option::get_value_description( float val ) const
    {
        return get_gyro_sensitivity_value_description( val );
    }

    const char * librealsense::gyro_sensitivity_option::get_description() const
    {
        return "gyro sensitivity resolutions, lowers the dynamic range for a more accurate readings";
    }

    bool librealsense::gyro_sensitivity_option::is_read_only() const
    {
        if( auto strong = _sensor.lock() )
            return strong->is_opened();
        return false;
    }


 }
