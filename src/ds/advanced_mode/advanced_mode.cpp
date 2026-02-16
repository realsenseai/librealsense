// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#include "core/advanced_mode.h"
#include "json_loader.hpp"
#include "ds/d400/d400-color.h"
#include "ds/d500/d500-color.h"

#include <src/ds/features/amplitude-factor-feature.h>
#include <src/ds/features/remove-ir-pattern-feature.h>

#include <rsutils/string/from.h>
#include <rsutils/string/hexdump.h>

namespace librealsense
{
    void ds_advanced_mode_base::register_to_visual_preset_option()
    {
        float max_preset = static_cast< float >( RS2_RS400_VISUAL_PRESET_MEDIUM_DENSITY );
        if( synthetic_sensor * synt = dynamic_cast< synthetic_sensor * >( _depth_sensor ) )
            max_preset = synt->get_preset_max_value(); // "Remove IR Pattern" visual preset is available only for D400, D410, D415, D460
        _preset_opt = std::make_shared< advanced_mode_preset_option >( *this,
                                                                        *_depth_sensor,
                                                                        option_range{ 0, max_preset, 1, RS2_RS400_VISUAL_PRESET_CUSTOM } );
        _depth_sensor->register_option( RS2_OPTION_VISUAL_PRESET, _preset_opt );
    }

    void ds_advanced_mode_base::unregister_from_visual_preset_option()
    {
        _depth_sensor->unregister_option(RS2_OPTION_VISUAL_PRESET);
    }

    void ds_advanced_mode_base::register_to_depth_scale_option()
    {
        if (_depth_units_register_action)
        {
            _depth_units_register_action();
        }
    }

    void ds_advanced_mode_base::unregister_from_depth_scale_option()
    {
        _depth_sensor->unregister_option(RS2_OPTION_DEPTH_UNITS);
    }

    void ds_advanced_mode_base::initialize_advanced_mode( device_interface * dev )
    {
        _dev = dev;
        _debug_interface = dynamic_cast< debug_interface * >( dev );
        if( ! dev || ! _debug_interface )
            throw std::runtime_error( "Advanced mode should be initialized with a device supporting debug interface" );

        for( size_t i = 0; i < _dev->get_sensors_count(); ++i )
        {
            auto base = dynamic_cast< sensor_base * >( &_dev->get_sensor( i ) );
            if( dynamic_cast< depth_sensor * >( base ) )
                _depth_sensor = base;
        }
        if( !_depth_sensor )
            throw std::runtime_error( "Advanced mode expects camera to have a depth sensor" );

        for( size_t i = 0; i < _dev->get_sensors_count(); ++i )
        {
            auto base = dynamic_cast< sensor_base * >( &_dev->get_sensor( i ) );
            if( dynamic_cast< color_sensor * >( base ) )
                _color_sensor = base;
        }

        device_specific_initialization();

        // _depth_units_register_action not needed for d500 devices 
        // since advanced mode toggling is not enabled
        if (auto d400_dev = dynamic_cast<d400_device*>(_dev))
        {   
            auto& depth_units_action = d400_dev->_depth_units_register_action;
            if (depth_units_action) 
            {
                ds_advanced_mode_base::set_depth_units_register_action(depth_units_action);
            }
        }
        
        ds_advanced_mode_base::set_hardware_reset_action([this]()
        {
            _dev->hardware_reset();
        });
    }

    void ds_advanced_mode_base::device_specific_initialization()
    {
        auto results = send_receive( encode_command( ds::fw_cmd::UAMG ) );
        assert_no_error( ds::fw_cmd::UAMG, results );
        _enabled = results[4] > 0;

        if( is_enabled() )
            register_to_visual_preset_option();

        _amplitude_factor_support = _dev->supports_feature( amplitude_factor_feature::ID );
    }

    bool ds_advanced_mode_base::is_enabled() const
    {
        return _enabled;
    }

    void ds_advanced_mode_base::toggle_advanced_mode( bool enable )
    {
        if (! enable )
        {
            unregister_from_visual_preset_option();
            unregister_from_depth_scale_option();
        }

        send_receive( encode_command( ds::fw_cmd::EN_ADV, enable ) );

        if (_hardware_reset_action)
            _hardware_reset_action();
        else
            send_receive( encode_command( ds::fw_cmd::HWRST ) );

        if( enable )
        {
            register_to_visual_preset_option();
            register_to_depth_scale_option();
        }
    }

    void ds_advanced_mode_base::apply_preset( const std::vector< platform::stream_profile > & configuration,
                                              rs2_rs400_visual_preset preset, uint16_t device_pid )
    {
        auto p = get_all();
        res_type res;
        // configuration is empty before first streaming - so set default res
        if( configuration.empty() )
            res = low_resolution;
        else
            res = get_res_type( configuration.front().width, configuration.front().height );

        switch( preset )
        {
        case RS2_RS400_VISUAL_PRESET_DEFAULT:
            switch( device_pid )
            {
            case ds::RS410_PID:
            case ds::RS415_PID:
            case ds::RS415_GMSL_PID:
                default_410( p );
                break;
            case ds::RS421_PID:
            case ds::RS430_PID:
            case ds::RS430I_PID:
            case ds::RS430_GMSL_PID:
            case ds::RS435_RGB_PID:
            case ds::RS435I_PID:
                default_430( p );
                break;
            case ds::RS436_PID:
                default_436( p );
                break;
            case ds::RS455_PID:
            case ds::RS457_PID:
            case ds::D555_PID:
            case ds::D585_PID:
                default_450_mid_low_res( p );
                switch( res )
                {
                case low_resolution:
                case medium_resolution:
                    // applied defaultly
                    break;
                case high_resolution:
                    default_450_high_res( p );
                    break;
                default:
                    throw invalid_value_exception( rsutils::string::from()
                                                   << "apply_preset(...) failed! Given device doesn't support Default Preset (pid=0x"
                                                   << rsutils::string::hexdump( device_pid ) << ")" );
                    break;
                }
                break;
            case ds::D585S_PID:
                default_585S( p );
                break;
            case ds::RS405U_PID:
                default_405u( p );
                break;
            case ds::RS405_PID:
            case ds::RS401_GMSL_PID:
                default_405( p );
                break;
            case ds::RS400_PID:
                default_400( p );
                break;
            case ds::RS420_PID:
                default_420( p );
                break;
            default:
                throw invalid_value_exception( rsutils::string::from()
                                               << "apply_preset(...) failed! Given device doesn't support Default Preset (pid=0x"
                                               << rsutils::string::hexdump( device_pid ) << ")" );
                break;
            }
            break;
        case RS2_RS400_VISUAL_PRESET_HAND:
            hand_gesture( p );
            // depth units for D405
            if( device_pid == ds::RS405_PID || device_pid == ds::RS401_GMSL_PID )
                p.depth_table.depthUnits = 100;  // 0.1mm
            break;
        case RS2_RS400_VISUAL_PRESET_HIGH_ACCURACY:
            high_accuracy( p );
            break;
        case RS2_RS400_VISUAL_PRESET_HIGH_DENSITY:
            high_density( p );
            break;
        case RS2_RS400_VISUAL_PRESET_MEDIUM_DENSITY:
            mid_density( p );
            break;
        case RS2_RS400_VISUAL_PRESET_REMOVE_IR_PATTERN: {
            if( ! _dev->supports_feature( remove_ir_pattern_feature::ID ) )
                throw invalid_value_exception( "apply_preset(...) failed! The device does not support remove IR pattern feature" );

            switch( device_pid )
            {
            case ds::RS400_PID:
            case ds::RS410_PID:
            case ds::RS415_PID:
                d415_remove_ir( p );
                break;
            case ds::RS460_PID:
                d460_remove_ir( p );
                break;
            default:
                throw invalid_value_exception( rsutils::string::from()
                                               << "apply_preset(...) failed! Given device doesn't support Remove IR Pattern Preset (pid=0x"
                                               << std::hex << device_pid << ")" );
                break;
            }
        }
        break;
        default:
            throw invalid_value_exception( rsutils::string::from()
                                           << "apply_preset(...) failed! Invalid preset! (" << preset << ")" );
        }
        set_all( p );
    }

    void ds_advanced_mode_base::get_depth_control_group( STDepthControlGroup * ptr, int mode ) const
    {
        *ptr = get< STDepthControlGroup >( advanced_mode_traits< STDepthControlGroup >::group, nullptr, mode );
    }

    void ds_advanced_mode_base::get_rsm(STRsm* ptr, int mode) const
    {
        *ptr = get<STRsm>(advanced_mode_traits<STRsm>::group, nullptr, mode);
    }

    void ds_advanced_mode_base::get_rau_support_vector_control( STRauSupportVectorControl * ptr, int mode ) const
    {
        *ptr = get< STRauSupportVectorControl >( advanced_mode_traits< STRauSupportVectorControl >::group, nullptr, mode );
    }

    void ds_advanced_mode_base::get_color_control( STColorControl * ptr, int mode ) const
    {
        *ptr = get< STColorControl >( advanced_mode_traits< STColorControl >::group, nullptr, mode );
    }

    void ds_advanced_mode_base::get_rau_color_thresholds_control( STRauColorThresholdsControl * ptr, int mode ) const
    {
        *ptr = get< STRauColorThresholdsControl >( advanced_mode_traits< STRauColorThresholdsControl >::group, nullptr, mode );
    }

    void ds_advanced_mode_base::get_slo_color_thresholds_control( STSloColorThresholdsControl * ptr, int mode ) const
    {
        *ptr = get< STSloColorThresholdsControl >( advanced_mode_traits< STSloColorThresholdsControl >::group, nullptr, mode );
    }

    void ds_advanced_mode_base::get_slo_penalty_control( STSloPenaltyControl * ptr, int mode ) const
    {
        *ptr = get< STSloPenaltyControl >( advanced_mode_traits< STSloPenaltyControl >::group, nullptr, mode );
    }

    void ds_advanced_mode_base::get_hdad( STHdad * ptr, int mode ) const
    {
        *ptr = get< STHdad >( advanced_mode_traits< STHdad >::group, nullptr, mode );
    }

    void ds_advanced_mode_base::get_color_correction( STColorCorrection * ptr, int mode ) const
    {
        *ptr = get< STColorCorrection >( advanced_mode_traits< STColorCorrection >::group, nullptr, mode );
    }

    void ds_advanced_mode_base::get_depth_table_control( STDepthTableControl * ptr, int mode ) const
    {
        *ptr = get< STDepthTableControl >( advanced_mode_traits< STDepthTableControl >::group, nullptr, mode );
    }

    void ds_advanced_mode_base::get_ae_control( STAEControl * ptr, int mode ) const
    {
        *ptr = get< STAEControl >( advanced_mode_traits< STAEControl >::group, nullptr, mode );
    }

    void ds_advanced_mode_base::get_census_radius( STCensusRadius * ptr, int mode ) const
    {
        *ptr = get< STCensusRadius >( advanced_mode_traits< STCensusRadius >::group, nullptr, mode );
    }

    void ds_advanced_mode_base::get_amp_factor( STAFactor* ptr, int mode ) const
    {
        *ptr = _amplitude_factor_support ? get< STAFactor >( advanced_mode_traits< STAFactor >::group, nullptr, mode ) :
            []() { STAFactor af; af.amplitude = 0.f; return af; }();
    }

    bool ds_advanced_mode_base::supports_option( const sensor_base * sensor, rs2_option opt ) const
    {
        if( ! sensor )
            return false;
        return sensor->supports_option( opt );
    }

    void ds_advanced_mode_base::get_laser_power(laser_power_control* ptr) const
    {
        if( supports_option( _depth_sensor, RS2_OPTION_LASER_POWER ) )
        {
            ptr->laser_power = _depth_sensor->get_option(RS2_OPTION_LASER_POWER).query();
            ptr->was_set = true;
        }
    }

    void ds_advanced_mode_base::get_laser_state(laser_state_control* ptr) const
    {
        if( supports_option( _depth_sensor, RS2_OPTION_EMITTER_ENABLED ) )
        {
            ptr->laser_state = static_cast< int >( _depth_sensor->get_option( RS2_OPTION_EMITTER_ENABLED ).query() );
            ptr->was_set = true;
        }
    }

    void ds_advanced_mode_base::get_exposure( sensor_base * sensor, exposure_control * ptr ) const
    {
        if( supports_option( sensor, RS2_OPTION_EXPOSURE ) )
        {
            ptr->exposure = sensor->get_option(RS2_OPTION_EXPOSURE).query();
            ptr->was_set = true;
        }
    }

    void ds_advanced_mode_base::get_auto_exposure( sensor_base * sensor, auto_exposure_control * ptr ) const
    {
        if( supports_option( sensor, RS2_OPTION_ENABLE_AUTO_EXPOSURE ) )
        {
            ptr->auto_exposure = static_cast< int >( sensor->get_option( RS2_OPTION_ENABLE_AUTO_EXPOSURE ).query() );
            ptr->was_set = true;
        }
    }

    void ds_advanced_mode_base::get_depth_exposure( exposure_control * ptr ) const
    {
        get_exposure( _depth_sensor, ptr );
    }

    void ds_advanced_mode_base::get_depth_auto_exposure( auto_exposure_control * ptr ) const
    {
        get_auto_exposure( _depth_sensor, ptr );
    }

    void ds_advanced_mode_base::get_depth_gain( gain_control * ptr ) const
    {
        if( supports_option( _depth_sensor, RS2_OPTION_GAIN ) )
        {
            ptr->gain = _depth_sensor->get_option( RS2_OPTION_GAIN ).query();
            ptr->was_set = true;
        }
    }

    void ds_advanced_mode_base::get_depth_auto_white_balance( auto_white_balance_control * ptr ) const
    {
        if( supports_option( _depth_sensor, RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE ) )
        {
            ptr->auto_white_balance = static_cast< int >( _depth_sensor->get_option( RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE ).query() );
            ptr->was_set = true;
        }
    }

    void ds_advanced_mode_base::get_color_exposure( exposure_control * ptr ) const
    {
        get_exposure( _color_sensor, ptr );
    }

    void ds_advanced_mode_base::get_color_auto_exposure( auto_exposure_control * ptr ) const
    {
        get_auto_exposure( _color_sensor, ptr );
    }

    void ds_advanced_mode_base::get_color_backlight_compensation( backlight_compensation_control * ptr ) const
    {
        if( supports_option( _color_sensor, RS2_OPTION_BACKLIGHT_COMPENSATION ) )
        {
            ptr->backlight_compensation = static_cast< int >( _color_sensor->get_option( RS2_OPTION_BACKLIGHT_COMPENSATION ).query() );
            ptr->was_set = true;
        }
    }

    void ds_advanced_mode_base::get_color_brightness( brightness_control * ptr ) const
    {
        if( supports_option( _color_sensor, RS2_OPTION_BRIGHTNESS ) )
        {
            ptr->brightness = _color_sensor->get_option(RS2_OPTION_BRIGHTNESS).query();
            ptr->was_set = true;
        }
    }

    void ds_advanced_mode_base::get_color_contrast( contrast_control * ptr ) const
    {
        if( supports_option( _color_sensor, RS2_OPTION_CONTRAST ) )
        {
            ptr->contrast = _color_sensor->get_option( RS2_OPTION_CONTRAST ).query();
            ptr->was_set = true;
        }
    }

    void ds_advanced_mode_base::get_color_gain( gain_control * ptr ) const
    {
        if( supports_option( _color_sensor, RS2_OPTION_GAIN ) )
        {
            ptr->gain = _color_sensor->get_option( RS2_OPTION_GAIN ).query();
            ptr->was_set = true;
        }
    }

    void ds_advanced_mode_base::get_color_gamma( gamma_control * ptr ) const
    {
        if( supports_option( _color_sensor, RS2_OPTION_GAMMA ) )
        {
            ptr->gamma = _color_sensor->get_option( RS2_OPTION_GAMMA ).query();
            ptr->was_set = true;
        }
    }

    void ds_advanced_mode_base::get_color_hue( hue_control * ptr ) const
    {
        if( supports_option( _color_sensor, RS2_OPTION_HUE ) )
        {
            ptr->hue = _color_sensor->get_option( RS2_OPTION_HUE ).query();
            ptr->was_set = true;
        }
    }

    void ds_advanced_mode_base::get_color_saturation( saturation_control * ptr ) const
    {
        if( supports_option( _color_sensor, RS2_OPTION_SATURATION ) )
        {
            ptr->saturation = _color_sensor->get_option( RS2_OPTION_SATURATION ).query();
            ptr->was_set = true;
        }
    }

    void ds_advanced_mode_base::get_color_sharpness( sharpness_control * ptr ) const
    {
        if( supports_option( _color_sensor, RS2_OPTION_SHARPNESS ) )
        {
            ptr->sharpness = _color_sensor->get_option( RS2_OPTION_SHARPNESS ).query();
            ptr->was_set = true;
        }
    }

    void ds_advanced_mode_base::get_color_white_balance( white_balance_control * ptr ) const
    {
        if( supports_option( _color_sensor, RS2_OPTION_WHITE_BALANCE ) )
        {
            ptr->white_balance = _color_sensor->get_option( RS2_OPTION_WHITE_BALANCE ).query();
            ptr->was_set = true;
        }
    }

    void ds_advanced_mode_base::get_color_auto_white_balance( auto_white_balance_control * ptr ) const
    {
        if( supports_option( _color_sensor, RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE ) )
        {
            ptr->auto_white_balance = static_cast< int >( _color_sensor->get_option( RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE ).query() );
            ptr->was_set = true;
        }
    }

    void ds_advanced_mode_base::get_color_power_line_frequency( power_line_frequency_control * ptr ) const
    {
        if( supports_option( _color_sensor, RS2_OPTION_POWER_LINE_FREQUENCY ) )
        {
            ptr->power_line_frequency = static_cast< int >( _color_sensor->get_option( RS2_OPTION_POWER_LINE_FREQUENCY ).query() );
            ptr->was_set = true;
        }
    }

    void ds_advanced_mode_base::get_hdr_preset( hdr_preset::hdr_preset * ptr ) const
    {
        auto buffer = send_receive( encode_command( ds::GETSUBPRESET ) );
        *ptr = parse_hdr_preset( buffer );
    }

    void ds_advanced_mode_base::set_depth_control_group( const STDepthControlGroup & val )
    {
        set( val, advanced_mode_traits< STDepthControlGroup >::group );
        if( _preset_opt )
            _preset_opt->set( RS2_RS400_VISUAL_PRESET_CUSTOM );
    }

    void ds_advanced_mode_base::set_rsm( const STRsm & val )
    {
        set( val, advanced_mode_traits< STRsm >::group );
        if( _preset_opt )
            _preset_opt->set( RS2_RS400_VISUAL_PRESET_CUSTOM );
    }

    void ds_advanced_mode_base::set_rau_support_vector_control( const STRauSupportVectorControl & val )
    {
        set( val, advanced_mode_traits< STRauSupportVectorControl >::group );
        if( _preset_opt )
            _preset_opt->set( RS2_RS400_VISUAL_PRESET_CUSTOM );
    }

    void ds_advanced_mode_base::set_color_control( const STColorControl & val )
    {
        set( val, advanced_mode_traits< STColorControl >::group );
        if( _preset_opt )
            _preset_opt->set( RS2_RS400_VISUAL_PRESET_CUSTOM );
    }

    void ds_advanced_mode_base::set_rau_color_thresholds_control( const STRauColorThresholdsControl & val )
    {
        set( val, advanced_mode_traits< STRauColorThresholdsControl >::group );
        if( _preset_opt )
            _preset_opt->set( RS2_RS400_VISUAL_PRESET_CUSTOM );
    }

    void ds_advanced_mode_base::set_slo_color_thresholds_control( const STSloColorThresholdsControl & val )
    {
        set( val, advanced_mode_traits< STSloColorThresholdsControl >::group );
        if( _preset_opt )
            _preset_opt->set( RS2_RS400_VISUAL_PRESET_CUSTOM );
    }

    void ds_advanced_mode_base::set_slo_penalty_control( const STSloPenaltyControl & val )
    {
        set( val, advanced_mode_traits< STSloPenaltyControl >::group );
        if( _preset_opt )
            _preset_opt->set( RS2_RS400_VISUAL_PRESET_CUSTOM );
    }

    void ds_advanced_mode_base::set_hdad( const STHdad & val )
    {
        set( val, advanced_mode_traits< STHdad >::group );
        if( _preset_opt )
            _preset_opt->set( RS2_RS400_VISUAL_PRESET_CUSTOM );
    }

    void ds_advanced_mode_base::set_color_correction( const STColorCorrection & val )
    {
        set( val, advanced_mode_traits< STColorCorrection >::group );
        if( _preset_opt )
            _preset_opt->set( RS2_RS400_VISUAL_PRESET_CUSTOM );
    }

    void ds_advanced_mode_base::set_depth_table_control( const STDepthTableControl & val )
    {
        set( val, advanced_mode_traits< STDepthTableControl >::group );
        if( _preset_opt )
            _preset_opt->set( RS2_RS400_VISUAL_PRESET_CUSTOM );
    }

    void ds_advanced_mode_base::set_ae_control( const STAEControl & val )
    {
        set( val, advanced_mode_traits< STAEControl >::group );
        if( _preset_opt )
            _preset_opt->set( RS2_RS400_VISUAL_PRESET_CUSTOM );
    }

    void ds_advanced_mode_base::set_census_radius( const STCensusRadius & val )
    {
        set( val, advanced_mode_traits< STCensusRadius >::group );
        if( _preset_opt )
            _preset_opt->set( RS2_RS400_VISUAL_PRESET_CUSTOM );
    }

    void ds_advanced_mode_base::set_amp_factor( const STAFactor & val )
    {
        if( _amplitude_factor_support )
        {
            set( val, advanced_mode_traits< STAFactor >::group );
            if( _preset_opt )
                _preset_opt->set( RS2_RS400_VISUAL_PRESET_CUSTOM );
        }
    }

    void ds_advanced_mode_base::set_laser_power( const laser_power_control & val )
    {
        if( val.was_set )
            _depth_sensor->get_option( RS2_OPTION_LASER_POWER ).set( val.laser_power );
    }

    void ds_advanced_mode_base::set_laser_state( const laser_state_control & val )
    {
        if( val.was_set )
            _depth_sensor->get_option( RS2_OPTION_EMITTER_ENABLED ).set( (float)val.laser_state );
    }

    void ds_advanced_mode_base::set_exposure( sensor_base * sensor, const exposure_control & val )
    {
        if( sensor )
            sensor->get_option( RS2_OPTION_EXPOSURE ).set( val.exposure );
    }

    void ds_advanced_mode_base::set_auto_exposure( sensor_base * sensor, const auto_exposure_control & val )
    {
        if( sensor )
            sensor->get_option( RS2_OPTION_ENABLE_AUTO_EXPOSURE ).set( float( val.auto_exposure ) );
    }

    void ds_advanced_mode_base::set_depth_exposure(const exposure_control& val)
    {
        if( val.was_set )
            set_exposure( _depth_sensor, val );
    }

    void ds_advanced_mode_base::set_depth_auto_exposure( const auto_exposure_control & val )
    {
        if( val.was_set )
            set_auto_exposure( _depth_sensor, val );
    }

    void ds_advanced_mode_base::set_depth_gain( const gain_control & val )
    {
        if( val.was_set )
            _depth_sensor->get_option( RS2_OPTION_GAIN ).set( val.gain );
    }

    void ds_advanced_mode_base::set_depth_auto_white_balance( const auto_white_balance_control & val )
    {
        if( val.was_set )
            _depth_sensor->get_option( RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE ).set( float( val.auto_white_balance ) );
    }

    void ds_advanced_mode_base::set_color_exposure( const exposure_control & val )
    {
        if( val.was_set && ! _color_sensor )
            throw invalid_value_exception( "Can't set color_exposure value! Color sensor not found." );

        if( val.was_set )
            set_exposure( _color_sensor, val );
    }

    void ds_advanced_mode_base::set_color_auto_exposure( const auto_exposure_control & val )
    {
        if( val.was_set && ! _color_sensor )
            throw invalid_value_exception( "Can't set color_auto_exposure value! Color sensor not found." );

        if( val.was_set )
            set_auto_exposure( _color_sensor, val );
    }

    void ds_advanced_mode_base::set_color_backlight_compensation( const backlight_compensation_control & val )
    {
        if( val.was_set && ! _color_sensor )
            throw invalid_value_exception( "Can't set color_backlight_compensation value! Color sensor not found." );

        if( val.was_set )
            _color_sensor->get_option( RS2_OPTION_BACKLIGHT_COMPENSATION ).set( (float)val.backlight_compensation );
    }

    void ds_advanced_mode_base::set_color_brightness( const brightness_control & val )
    {
        if( val.was_set && ! _color_sensor )
            throw invalid_value_exception( "Can't set color_brightness value! Color sensor not found." );

        if( val.was_set )
            _color_sensor->get_option( RS2_OPTION_BRIGHTNESS ).set( val.brightness );
    }

    void ds_advanced_mode_base::set_color_contrast( const contrast_control & val )
    {
        if( val.was_set && ! _color_sensor )
            throw invalid_value_exception( "Can't set color_contrast value! Color sensor not found." );

        if( val.was_set )
            _color_sensor->get_option( RS2_OPTION_CONTRAST ).set( val.contrast );
    }

    void ds_advanced_mode_base::set_color_gain( const gain_control & val )
    {
        if( val.was_set && ! _color_sensor )
            throw invalid_value_exception( "Can't set color_gain value! Color sensor not found." );

        if( val.was_set )
            _color_sensor->get_option( RS2_OPTION_GAIN ).set( val.gain );
    }

    void ds_advanced_mode_base::set_color_gamma( const gamma_control & val )
    {
        if( val.was_set && ! _color_sensor )
            throw invalid_value_exception( "Can't set color_gamma value! Color sensor not found." );

        if( val.was_set )
            _color_sensor->get_option( RS2_OPTION_GAMMA ).set( val.gamma );
    }

    void ds_advanced_mode_base::set_color_hue( const hue_control & val )
    {
        if( val.was_set && ! _color_sensor )
            throw invalid_value_exception( "Can't set color_hue value! Color sensor not found." );

        if( val.was_set )
            _color_sensor->get_option( RS2_OPTION_HUE ).set( val.hue );
    }

    void ds_advanced_mode_base::set_color_saturation( const saturation_control & val )
    {
        if( val.was_set && ! _color_sensor )
            throw invalid_value_exception( "Can't set color_saturation value! Color sensor not found." );

        if( val.was_set )
            _color_sensor->get_option( RS2_OPTION_SATURATION ).set( val.saturation );
    }

    void ds_advanced_mode_base::set_color_sharpness( const sharpness_control & val )
    {
        if( val.was_set && ! _color_sensor )
            throw invalid_value_exception( "Can't set color_sharpness value! Color sensor not found." );

        if( val.was_set )
            _color_sensor->get_option( RS2_OPTION_SHARPNESS ).set( val.sharpness );
    }

    void ds_advanced_mode_base::set_color_white_balance( const white_balance_control & val )
    {
        if( val.was_set && ! _color_sensor )
            throw invalid_value_exception( "Can't set color_white_balance value! Color sensor not found." );

        if( val.was_set )
            _color_sensor->get_option( RS2_OPTION_WHITE_BALANCE ).set( val.white_balance );
    }

    void ds_advanced_mode_base::set_color_auto_white_balance( const auto_white_balance_control & val )
    {
        if( val.was_set && ! _color_sensor )
            throw invalid_value_exception( "Can't set color_auto_white_balance value! Color sensor not found." );

        if( val.was_set )
            _color_sensor->get_option( RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE ).set( (float)val.auto_white_balance );
    }

    void ds_advanced_mode_base::set_color_power_line_frequency( const power_line_frequency_control & val )
    {
        if( val.was_set && ! _color_sensor )
            throw invalid_value_exception( "Can't set color_power_line_frequency value! Color sensor not found." );

        if( val.was_set )
            _color_sensor->get_option( RS2_OPTION_POWER_LINE_FREQUENCY ).set( (float)val.power_line_frequency );
    }

    void ds_advanced_mode_base::block( const std::string & exception_message )
    {
        _blocked = true;
        _block_message = exception_message;
    }

    void ds_advanced_mode_base::unblock()
    {
        _blocked = false;
        _block_message.clear();
    }

    std::vector<uint8_t> ds_advanced_mode_base::serialize_json() const
    {
        if (!is_enabled())
            throw wrong_api_call_sequence_exception( rsutils::string::from()
                                                     << "serialize_json() failed! Device is not in Advanced-Mode." );

        auto p = get_all();
        return generate_json(*_dev, p);
    }

    void ds_advanced_mode_base::load_json(const std::string& json_content)
    {
        if (!is_enabled())
            throw wrong_api_call_sequence_exception( rsutils::string::from()
                                                     << "load_json(...) failed! Device is not in Advanced-Mode." );

        auto p = get_all();
        update_structs(*_dev,  json_content, p);
        set_all(p);
        if( _preset_opt )
            _preset_opt->set( RS2_RS400_VISUAL_PRESET_CUSTOM );
    }

    preset ds_advanced_mode_base::get_all() const
    {
        preset p = {0};

        rsutils::deferred depth_bulk = _depth_sensor->bulk_operation();
        get_depth_control_group( &p.depth_controls );
        get_rsm( &p.rsm );
        get_rau_support_vector_control( &p.rsvc );
        get_color_control( &p.color_control );
        get_rau_color_thresholds_control( &p.rctc );
        get_slo_color_thresholds_control( &p.sctc );
        get_slo_penalty_control( &p.spc );
        get_hdad( &p.hdad );
        get_color_correction( &p.cc );
        get_depth_table_control( &p.depth_table );
        get_ae_control( &p.ae );
        get_census_radius( &p.census );
        get_amp_factor( &p.amplitude_factor );
        get_laser_power( &p.laser_power );
        get_laser_state( &p.laser_state );
        get_depth_exposure( &p.depth_exposure );
        get_depth_auto_exposure( &p.depth_auto_exposure );
        get_depth_gain( &p.depth_gain );
        get_depth_auto_white_balance( &p.depth_auto_white_balance );

        rsutils::deferred color_bulk;
        if( _color_sensor )
        {
            color_bulk = _color_sensor->bulk_operation();
            get_color_exposure( &p.color_exposure );
            get_color_auto_exposure( &p.color_auto_exposure );
            get_color_backlight_compensation( &p.color_backlight_compensation );
            get_color_brightness( &p.color_brightness );
            get_color_contrast( &p.color_contrast );
            get_color_gain( &p.color_gain );
            get_color_gamma( &p.color_gamma );
            get_color_hue( &p.color_hue );
            get_color_saturation( &p.color_saturation );
            get_color_sharpness( &p.color_sharpness );
            get_color_white_balance( &p.color_white_balance );
            get_color_auto_white_balance( &p.color_auto_white_balance );
            get_color_power_line_frequency( &p.color_power_line_frequency );
        }

        get_hdr_preset( &p.auto_hdr );

        return p;
    }

    void ds_advanced_mode_base::set_all( const preset & p )
    {
        set_all_depth( p );
        if( should_set_rgb_preset() )
            set_all_rgb( p );
        if( should_set_hdr_preset(p) )
            set_hdr_preset( p );
    }

    void ds_advanced_mode_base::set_all_depth( const preset& p )
    {
        rsutils::deferred depth_bulk = _depth_sensor->bulk_operation();

        set( p.depth_controls, advanced_mode_traits< STDepthControlGroup >::group );
        set( p.rsm           , advanced_mode_traits< STRsm >::group );
        set( p.rsvc          , advanced_mode_traits< STRauSupportVectorControl >::group );
        set( p.hdad          , advanced_mode_traits< STHdad >::group );

        // Setting auto-white-balance control before colorCorrection parameters
        set_depth_auto_white_balance( p.depth_auto_white_balance );
        set( p.cc, advanced_mode_traits< STColorCorrection >::group );

        set( p.depth_table, advanced_mode_traits< STDepthTableControl >::group );
        set( p.ae         , advanced_mode_traits< STAEControl >::group );
        set( p.census     , advanced_mode_traits< STCensusRadius >::group );
        if( _amplitude_factor_support )
            set( p.amplitude_factor, advanced_mode_traits< STAFactor >::group );

        set_laser_state( p.laser_state );
        if( p.laser_state.was_set && p.laser_state.laser_state == 1 )  // 1 - on
            set_laser_power( p.laser_power );

        set_depth_auto_exposure( p.depth_auto_exposure );
        if( p.depth_auto_exposure.was_set && p.depth_auto_exposure.auto_exposure == 0 )
        {
            set_depth_gain( p.depth_gain );
            set_depth_exposure( p.depth_exposure );
        }

        // Depth sensor related even though they have color in the name. Probably color from left IR imager.
        set( p.color_control, advanced_mode_traits< STColorControl >::group );
        set( p.rctc         , advanced_mode_traits< STRauColorThresholdsControl >::group );
        set( p.sctc         , advanced_mode_traits< STSloColorThresholdsControl >::group );
        set( p.spc          , advanced_mode_traits< STSloPenaltyControl >::group );
    }

    void ds_advanced_mode_base::set_all_rgb( const preset & p )
    {
        rsutils::deferred color_bulk;
        if( _color_sensor )
        {
            color_bulk = _color_sensor->bulk_operation();

            set_color_auto_exposure( p.color_auto_exposure );
            if( p.color_auto_exposure.was_set && p.color_auto_exposure.auto_exposure == 0 )
            {
                set_color_exposure( p.color_exposure );
                set_color_gain( p.color_gain );
            }

            set_color_backlight_compensation( p.color_backlight_compensation );
            set_color_brightness( p.color_brightness );
            set_color_contrast( p.color_contrast );
            set_color_gamma( p.color_gamma );
            set_color_hue( p.color_hue );
            set_color_saturation( p.color_saturation );
            set_color_sharpness( p.color_sharpness );

            set_color_auto_white_balance( p.color_auto_white_balance );
            if( p.color_auto_white_balance.was_set && p.color_auto_white_balance.auto_white_balance == 0 )
                set_color_white_balance( p.color_white_balance );

            // TODO: W/O due to a FW bug of power_line_frequency control on Windows OS
            // set_color_power_line_frequency(p.color_power_line_frequency);
        }
    }

    bool ds_advanced_mode_base::should_set_rgb_preset() const
    {
        auto product_line = _dev->get_info( rs2_camera_info::RS2_CAMERA_INFO_PRODUCT_LINE );

        return product_line != "D500";
    }

    bool ds_advanced_mode_base::should_set_hdr_preset( const preset & p )
    {
        return p.auto_hdr.header.num_of_items > 0;
    }

    void ds_advanced_mode_base::set_hdr_preset( const preset & p )
    {
        if( ! _dev->supports_info( RS2_CAMERA_INFO_NAME ) ||
            _dev->get_info( RS2_CAMERA_INFO_NAME ).find( "D45" ) == std::string::npos )
        {
            throw std::runtime_error( "HDR preset is not supported on the connected device" );  // feature only works for D45* cameras
        }
        // if auto exposure is not enabled, enable it if needed - temporary W/A until FW enable it
        auto & auto_exp = _depth_sensor->get_option( RS2_OPTION_ENABLE_AUTO_EXPOSURE );
        if( auto_exp.get_value() == 0 && p.auto_hdr.is_auto )
        {
            auto_exp.set( 1 );
        }

        // some devices support multiple modes of auto exposure, for this feature to work correctly we need to set the correct mode
        if( _depth_sensor->supports_option( RS2_OPTION_DEPTH_AUTO_EXPOSURE_MODE ) && p.auto_hdr.is_auto )
        {
            auto & auto_exp_mode = _depth_sensor->get_option( RS2_OPTION_DEPTH_AUTO_EXPOSURE_MODE );
            if( auto_exp_mode.get_value() != RS2_DEPTH_AUTO_EXPOSURE_ACCELERATED )
            {
                auto_exp_mode.set( RS2_DEPTH_AUTO_EXPOSURE_ACCELERATED );
            }
        }

        // serialize the hdr_preset and send it
        auto buffer = serialize_hdr_preset( p.auto_hdr.header, p.auto_hdr.items );
        send_receive( encode_command( ds::SETSUBPRESET, static_cast< uint32_t >( buffer.size() ), 0, 0, 0, buffer ) );
    }

    std::vector< uint8_t > ds_advanced_mode_base::send_receive( const std::vector< uint8_t > & input ) const
    {
        auto res = _debug_interface->send_receive_raw_data( input );
        if( res.empty() )
        {
            throw std::runtime_error( "Advanced mode write failed!" );
        }
        return res;
    }

    uint32_t ds_advanced_mode_base::pack( uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3 )
    {
        return ( c0 << 24 ) | ( c1 << 16 ) | ( c2 << 8 ) | c3;
    }

    std::vector< uint8_t > ds_advanced_mode_base::assert_no_error( ds::fw_cmd opcode, const std::vector< uint8_t > & results )
    {
        if( results.size() < sizeof( uint32_t ) )
            throw std::runtime_error( "Incomplete operation result!" );

        auto opCodeAsUint32 = pack( results[3], results[2], results[1], results[0] );
        if( opCodeAsUint32 != static_cast< uint32_t >( opcode ) )
        {
            std::stringstream ss;
            ss << "Operation failed with error code=" << static_cast< int >( opCodeAsUint32 );
            throw std::runtime_error( ss.str() );
        }
        std::vector< uint8_t > result;
        result.resize( results.size() - sizeof( uint32_t ) );
        std::copy( results.data() + sizeof( uint32_t ), results.data() + results.size(), result.data() );

        return result;
    }

    std::vector<uint8_t> ds_advanced_mode_base::encode_command( ds::fw_cmd opcode,
                                                                uint32_t p1,
                                                                uint32_t p2,
                                                                uint32_t p3,
                                                                uint32_t p4,
                                                                std::vector< uint8_t > data ) const
    {
        std::vector< uint8_t > raw_data;
        auto cmd_op_code = static_cast< uint32_t >( opcode );

        const uint16_t pre_header_data = 0xcdab;
        raw_data.resize( HW_MONITOR_BUFFER_SIZE );
        auto write_ptr = raw_data.data();
        auto header_size = 4;

        size_t cur_index = 2;
        *reinterpret_cast< uint16_t * >( write_ptr + cur_index ) = pre_header_data;
        cur_index += sizeof( uint16_t );
        *reinterpret_cast< unsigned int * >( write_ptr + cur_index ) = cmd_op_code;
        cur_index += sizeof( unsigned int );

        // Parameters
        *reinterpret_cast< unsigned * >( write_ptr + cur_index ) = p1;
        cur_index += sizeof( unsigned );
        *reinterpret_cast< unsigned * >( write_ptr + cur_index ) = p2;
        cur_index += sizeof( unsigned );
        *reinterpret_cast< unsigned * >( write_ptr + cur_index ) = p3;
        cur_index += sizeof( unsigned );
        *reinterpret_cast< unsigned * >( write_ptr + cur_index ) = p4;
        cur_index += sizeof( unsigned );

        // Data
        std::copy( data.begin(), data.end(), reinterpret_cast< uint8_t * >( write_ptr + cur_index ) );
        cur_index += data.size();

        *reinterpret_cast< uint16_t * >( raw_data.data() ) = static_cast< uint16_t >( cur_index - header_size );  // Length doesn't include hdr.
        raw_data.resize( cur_index );
        return raw_data;
    }

    ds_advanced_mode_base::res_type ds_advanced_mode_base::get_res_type( uint32_t width, uint32_t height ) const
    {
        if( width == 256 )  // Crop resolution
            return res_type::high_resolution;

        if( width == 640 )
            return res_type::medium_resolution;
        else if( width < 640 )
            return res_type::low_resolution;

        return res_type::high_resolution;
    }

    advanced_mode_preset_option::advanced_mode_preset_option( ds_advanced_mode_base & advanced,
                                                              sensor_base & ep,
                                                              const option_range & opt_range )
        : option_base( opt_range )
        , _ep( ep )
        , _advanced( advanced )
        , _last_preset( RS2_RS400_VISUAL_PRESET_CUSTOM )
    {
        _ep.register_on_open( [this]( std::vector< platform::stream_profile > configurations ) {
            _sensor_profiles = configurations;
            std::lock_guard< std::mutex > lock( _mtx );
            if( _last_preset != RS2_RS400_VISUAL_PRESET_CUSTOM )
                _advanced.apply_preset( _sensor_profiles, _last_preset, get_device_pid( _ep ) );
        } );
    }

    rs2_rs400_visual_preset advanced_mode_preset_option::to_preset( float x )
    {
        return ( static_cast< rs2_rs400_visual_preset >( (int)x ) );
    }

    void advanced_mode_preset_option::set( float value )
    {
        std::lock_guard< std::mutex > lock( _mtx );
        if( ! is_valid( value ) )
            throw invalid_value_exception( rsutils::string::from()
                                           << "set(advanced_mode_preset_option) failed! Given value " << value
                                           << " is out of range." );

        if( ! _advanced.is_enabled() )
            throw wrong_api_call_sequence_exception(
                rsutils::string::from() << "set(advanced_mode_preset_option) failed! Device is not in Advanced-Mode." );

        auto preset = to_preset( value );
        if( preset == RS2_RS400_VISUAL_PRESET_CUSTOM )
        {
            _last_preset = preset;
            return;
        }

        _advanced.apply_preset( _sensor_profiles, preset, get_device_pid( _ep ) );
        _last_preset = preset;
        _recording_function( *this );
    }

    float advanced_mode_preset_option::query() const
    {
        return static_cast< float >( _last_preset );
    }

    bool advanced_mode_preset_option::is_enabled() const
    {
        return true;
    }

    const char * advanced_mode_preset_option::get_description() const
    {
        return "Advanced-Mode Preset";
    }

    const char * advanced_mode_preset_option::get_value_description( float val ) const
    {
        try
        {
            return rs2_rs400_visual_preset_to_string( to_preset( val ) );
        }
        catch( std::out_of_range )
        {
            throw invalid_value_exception(
                rsutils::string::from()
                << "advanced_mode_preset: get_value_description(...) failed! Description of value " << val
                << " is not found." );
        }
    }

    uint16_t advanced_mode_preset_option::get_device_pid( const sensor_base & sensor ) const
    {
        auto str_pid = sensor.get_info( RS2_CAMERA_INFO_PRODUCT_ID );
        uint16_t device_pid{};
        std::stringstream ss;
        ss << std::hex << str_pid;
        ss >> device_pid;
        return device_pid;
    }
    }
