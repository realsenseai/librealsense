// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.


#include "ds-options.h"
#include <src/hid-sensor.h>


namespace librealsense
{
    const char* emitter_option::get_value_description(float val) const
    {
        switch (static_cast<int>(val))
        {
            case 0:
            {
                return "Off";
            }
            case 1:
            {
                return "Laser";
            }
            case 2:
            {
                return "Laser Auto";
            }
            case 3:
            {
                return "LED";
            }
            default:
                throw invalid_value_exception("value not found");
        }
    }

    emitter_option::emitter_option( const std::weak_ptr< uvc_sensor > & ep )
        : uvc_xu_option(ep, ds::depth_xu, ds::DS5_DEPTH_EMITTER_ENABLED,
                        "Emitter select, 0-disable all emitters, 1-enable laser, 2-enable laser auto (opt), 3-enable LED (opt)")
    {}

    float asic_and_projector_temperature_options::query() const
    {
        if (!is_enabled())
            throw wrong_api_call_sequence_exception("query is available during streaming only");

        #pragma pack(push, 1)
        struct temperature
        {
            uint8_t is_projector_valid;
            uint8_t is_asic_valid;
            int8_t projector_temperature;
            int8_t asic_temperature;
        };
        #pragma pack(pop)

        auto strong_ep = _ep.lock();
        if (!strong_ep)
            throw camera_disconnected_exception("asic and proj temperatures cannot access the sensor");

        auto temperature_data = static_cast<temperature>(strong_ep->invoke_powered(
            [this](platform::uvc_device& dev)
            {
                temperature temp{};
                if (!dev.get_xu(ds::depth_xu,
                                ds::DS5_ASIC_AND_PROJECTOR_TEMPERATURES,
                                reinterpret_cast<uint8_t*>(&temp),
                                sizeof(temperature)))
                 {
                        throw invalid_value_exception(rsutils::string::from() << "get_xu(ctrl=DS5_ASIC_AND_PROJECTOR_TEMPERATURES) failed!" << " Last Error: " << strerror(errno));
                 }

                return temp;
            }));

        int8_t temperature::* field;
        uint8_t temperature::* is_valid_field;

        switch (_option)
        {
        case RS2_OPTION_ASIC_TEMPERATURE:
            field = &temperature::asic_temperature;
            is_valid_field = &temperature::is_asic_valid;
            break;
        case RS2_OPTION_PROJECTOR_TEMPERATURE:
            field = &temperature::projector_temperature;
            is_valid_field = &temperature::is_projector_valid;
            break;
        default:
            throw invalid_value_exception(rsutils::string::from() << strong_ep->get_option_name(_option) << " is not temperature option!");
        }

        if (0 == temperature_data.*is_valid_field)
            LOG_DEBUG(strong_ep->get_option_name(_option) << " value is not valid!");

        return temperature_data.*field;
    }

    option_range asic_and_projector_temperature_options::get_range() const
    {
        return option_range { -40, 125, 0, 0 };
    }

    bool asic_and_projector_temperature_options::is_enabled() const
    {
        auto strong_ep = _ep.lock();
        if (!strong_ep)
            throw camera_disconnected_exception("asic and proj temperatures cannot access the sensor");

        return strong_ep->is_streaming();
    }

    const char* asic_and_projector_temperature_options::get_description() const
    {
        auto strong_ep = _ep.lock();
        if (! strong_ep)
            throw camera_disconnected_exception("asic and proj temperatures cannot access the sensor");

        switch (_option)
        {
        case RS2_OPTION_ASIC_TEMPERATURE:
            return "Current Asic Temperature (degree celsius)";
        case RS2_OPTION_PROJECTOR_TEMPERATURE:
            return "Current Projector Temperature (degree celsius)";
        default:
            throw invalid_value_exception(rsutils::string::from() << strong_ep->get_option_name(_option) << " is not temperature option!");
        }
    }

    asic_and_projector_temperature_options::asic_and_projector_temperature_options( const std::weak_ptr<uvc_sensor> & ep, rs2_option opt)
        : _option(opt), _ep(std::move(ep))
        {}

    float motion_module_temperature_option::query() const
    {
        if (!is_enabled())
            throw wrong_api_call_sequence_exception("query is available during streaming only");

        static const auto report_field = platform::custom_sensor_report_field::value;
        auto data = _ep.get_custom_report_data(custom_sensor_name, report_name, report_field);
        if (data.empty())
            throw invalid_value_exception("query() motion_module_temperature_option failed! Empty buffer arrived.");

        auto data_str = std::string(reinterpret_cast<char const*>(data.data()));
        return std::stof(data_str);
    }

    option_range motion_module_temperature_option::get_range() const
    {
        if (!is_enabled())
            throw wrong_api_call_sequence_exception("get option range is available during streaming only");

        static const auto min_report_field = platform::custom_sensor_report_field::minimum;
        static const auto max_report_field = platform::custom_sensor_report_field::maximum;
        auto min_data = _ep.get_custom_report_data(custom_sensor_name, report_name, min_report_field);
        auto max_data = _ep.get_custom_report_data(custom_sensor_name, report_name, max_report_field);
        if (min_data.empty() || max_data.empty())
            throw invalid_value_exception("get_range() motion_module_temperature_option failed! Empty buffer arrived.");

        auto min_str = std::string(reinterpret_cast<char const*>(min_data.data()));
        auto max_str = std::string(reinterpret_cast<char const*>(max_data.data()));

        return option_range{std::stof(min_str),
                            std::stof(max_str),
                            0, 0};
    }

    bool motion_module_temperature_option::is_enabled() const
    {
        return _ep.is_streaming();
    }

    const char* motion_module_temperature_option::get_description() const
    {
        return "Current Motion-Module Temperature (degree celsius)";
    }

    motion_module_temperature_option::motion_module_temperature_option(hid_sensor& ep)
        : _ep(ep)
    {}

    void enable_motion_correction::set(float value)
    {
        if (!is_valid(value))
            throw invalid_value_exception(rsutils::string::from() << "set(enable_motion_correction) failed! Given value " << value << " is out of range.");

        _is_active = value > _opt_range.min;
        _recording_function(*this);
    }

    float enable_motion_correction::query() const
    {
        auto is_active = _is_active.load();
        return is_active ? _opt_range.max : _opt_range.min;
    }

    enable_motion_correction::enable_motion_correction(sensor_base* mm_ep,
                                                       const option_range& opt_range)
        : option_base(opt_range), _is_active(true)
    {}

    void enable_auto_exposure_option::set(float value)
    {
        if (!is_valid(value))
            throw invalid_value_exception("set(enable_auto_exposure) failed! Invalid Auto-Exposure mode request " + std::to_string(value));

        auto auto_exposure_prev_state = _auto_exposure_state->get_enable_auto_exposure();
        _auto_exposure_state->set_enable_auto_exposure(0.f < std::fabs(value));

        if (_auto_exposure_state->get_enable_auto_exposure()) // auto_exposure current value
        {
            if (!auto_exposure_prev_state) // auto_exposure previous value
            {
                _to_add_frames = true; // auto_exposure moved from disable to enable
            }
        }
        else
        {
            if (auto_exposure_prev_state)
            {
                _to_add_frames = false; // auto_exposure moved from enable to disable
            }
        }
        _recording_function(*this);
    }

    float enable_auto_exposure_option::query() const
    {
        return _auto_exposure_state->get_enable_auto_exposure();
    }

    enable_auto_exposure_option::enable_auto_exposure_option(synthetic_sensor* fisheye_ep,
                                                             std::shared_ptr<auto_exposure_mechanism> auto_exposure,
                                                             std::shared_ptr<auto_exposure_state> auto_exposure_state,
                                                             const option_range& opt_range)
        : option_base(opt_range),
          _auto_exposure_state(auto_exposure_state),
          _to_add_frames((_auto_exposure_state->get_enable_auto_exposure())),
          _auto_exposure(auto_exposure)
    {}

    auto_exposure_mode_option::auto_exposure_mode_option(std::shared_ptr<auto_exposure_mechanism> auto_exposure,
                                                         std::shared_ptr<auto_exposure_state> auto_exposure_state,
                                                         const option_range& opt_range,
                                                         const std::map<float, std::string>& description_per_value)
        : option_base(opt_range),
          _auto_exposure_state(auto_exposure_state),
          _auto_exposure(auto_exposure),
          _description_per_value(description_per_value)
    {}

    void auto_exposure_mode_option::set(float value)
    {
        if (!is_valid(value))
            throw invalid_value_exception(rsutils::string::from() << "set(auto_exposure_mode_option) failed! Given value " << value << " is out of range.");

        _auto_exposure_state->set_auto_exposure_mode(static_cast<auto_exposure_modes>((int)value));
        _auto_exposure->update_auto_exposure_state(*_auto_exposure_state);
        _recording_function(*this);
    }

    float auto_exposure_mode_option::query() const
    {
        return static_cast<float>(_auto_exposure_state->get_auto_exposure_mode());
    }

    const char* auto_exposure_mode_option::get_value_description(float val) const
    {
        try{
            return _description_per_value.at(val).c_str();
        }
        catch(std::out_of_range)
        {
            throw invalid_value_exception(rsutils::string::from() << "auto_exposure_mode: get_value_description(...) failed! Description of value " << val << " is not found.");
        }
    }

    auto_exposure_step_option::auto_exposure_step_option(std::shared_ptr<auto_exposure_mechanism> auto_exposure,
        std::shared_ptr<auto_exposure_state> auto_exposure_state,
        const option_range& opt_range)
        : option_base(opt_range),
        _auto_exposure_state(auto_exposure_state),
        _auto_exposure(auto_exposure)
    {}

    void auto_exposure_step_option::set(float value)
    {
        if (!std::isnormal(_opt_range.step) || ((value < _opt_range.min) || (value > _opt_range.max)))
            throw invalid_value_exception(rsutils::string::from() << "set(auto_exposure_step_option) failed! Given value " << value << " is out of range.");

        _auto_exposure_state->set_auto_exposure_step(value);
        _auto_exposure->update_auto_exposure_state(*_auto_exposure_state);
        _recording_function(*this);
    }

    float auto_exposure_step_option::query() const
    {
        return static_cast<float>(_auto_exposure_state->get_auto_exposure_step());
    }

    auto_exposure_antiflicker_rate_option::auto_exposure_antiflicker_rate_option(std::shared_ptr<auto_exposure_mechanism> auto_exposure,
                                                                                 std::shared_ptr<auto_exposure_state> auto_exposure_state,
                                                                                 const option_range& opt_range,
                                                                                 const std::map<float, std::string>& description_per_value)
        : option_base(opt_range),
          _description_per_value(description_per_value),
          _auto_exposure_state(auto_exposure_state),
          _auto_exposure(auto_exposure)
    {}

    void auto_exposure_antiflicker_rate_option::set(float value)
    {
        if (!is_valid(value))
            throw invalid_value_exception(rsutils::string::from() << "set(auto_exposure_antiflicker_rate_option) failed! Given value " << value << " is out of range.");

        _auto_exposure_state->set_auto_exposure_antiflicker_rate(static_cast<uint32_t>(value));
        _auto_exposure->update_auto_exposure_state(*_auto_exposure_state);
        _recording_function(*this);
    }

    float auto_exposure_antiflicker_rate_option::query() const
    {
        return static_cast<float>(_auto_exposure_state->get_auto_exposure_antiflicker_rate());
    }

    const char* auto_exposure_antiflicker_rate_option::get_value_description(float val) const
    {
        try{
            return _description_per_value.at(val).c_str();
        }
        catch(std::out_of_range)
        {
            throw invalid_value_exception(rsutils::string::from() << "antiflicker_rate: get_value_description(...) failed! Description of value " << val << " is not found.");
        }
    }

    ds::depth_table_control depth_scale_option::get_depth_table(ds::advanced_query_mode mode) const
    {
        command cmd(ds::GET_ADV);
        cmd.param1 = ds::etDepthTableControl;
        cmd.param2 = mode;
        auto res = _hwm.send(cmd);

        if (res.size() < sizeof(ds::depth_table_control))
            throw std::runtime_error("Not enough bytes returned from the firmware!");

        auto table = (const ds::depth_table_control*)res.data();
        return *table;
    }

    depth_scale_option::depth_scale_option(hw_monitor& hwm)
        : _hwm(hwm)
    {
        _range = [this]()
        {
            auto min = get_depth_table(ds::GET_MIN);
            auto max = get_depth_table(ds::GET_MAX);
            return option_range{ (float)(0.000001 * min.depth_units),
                                 (float)(0.000001 * max.depth_units),
                                 0.000001f, 0.001f };
        };
    }

    void depth_scale_option::set(float value)
    {
        command cmd(ds::SET_ADV);
        cmd.param1 = ds::etDepthTableControl;

        auto depth_table = get_depth_table(ds::GET_VAL);
        depth_table.depth_units = static_cast<uint32_t>(1000000 * value);
        auto ptr = (uint8_t*)(&depth_table);
        cmd.data = std::vector<uint8_t>(ptr, ptr + sizeof(ds::depth_table_control));

        _hwm.send(cmd);
        _record_action(*this);
        notify(value);
    }

    float depth_scale_option::query() const
    {
        auto table = get_depth_table(ds::GET_VAL);
        return (float)(0.000001 * (float)table.depth_units);
    }

    option_range depth_scale_option::get_range() const
    {
        return *_range;
    }

    external_sync_mode::external_sync_mode( hw_monitor & hwm, const std::weak_ptr< sensor_base > & ep, int ver )
        : _hwm(hwm), _sensor(ep), _ver(ver)
    {
        _range = [this]()
        {
            return option_range{ ds::inter_cam_sync_mode::INTERCAM_SYNC_DEFAULT,
                                 static_cast<float>(_ver == 3 ? ds::inter_cam_sync_mode::INTERCAM_SYNC_MAX : (_ver == 2 ? 258 : 2)),
                                 1,
                                 ds::inter_cam_sync_mode::INTERCAM_SYNC_DEFAULT};
        };
    }

    void external_sync_mode::set(float value)
    {
        command cmd(ds::SET_CAM_SYNC);
        if (_ver == 1)
        {
            cmd.param1 = static_cast<int>(value);
        }
        else
        {
            auto strong = _sensor.lock();
            if( ! strong )
                throw std::runtime_error( "Cannot set Inter-camera HW synchronization, sensor is not alive" );

            if( strong->is_streaming() )
                throw std::runtime_error("Cannot change Inter-camera HW synchronization mode while streaming!");

            if (value < 4)
                cmd.param1 = static_cast<int>(value);
            else if (value == 259) // For Sending two frame - First with laser ON, and the other with laser OFF.
            {
                cmd.param1 = 0x00010204; // genlock, two frames, on-off
            }
            else if (value == 260) // For Sending two frame - First with laser OFF, and the other with laser ON.
            {
                cmd.param1 = 0x00030204; // genlock, two frames, off-on
            }
            else
            {
                cmd.param1 = 4;
                cmd.param1 |= (static_cast<int>(value - 3)) << 8;
            }
        }

        _hwm.send(cmd);
        _record_action(*this);
    }

    float external_sync_mode::query() const
    {
        command cmd(ds::GET_CAM_SYNC);
        auto res = _hwm.send(cmd);
        if (res.empty())
            throw invalid_value_exception("external_sync_mode::query result is empty!");

        if (res.front() < 4)
            return (res.front());
        else if (res[2] == 0x01)
        {
            return 259.0f;
        }
        else if (res[2] == 0x03)
        {
            return 260.0f;
        }
        else
            return (static_cast<float>(res[1]) + 3.0f);
    }

    option_range external_sync_mode::get_range() const
    {
        return *_range;
    }

    bool external_sync_mode::is_read_only() const
    {
        auto strong = _sensor.lock();
        return strong && strong->is_opened();
    }

    emitter_on_and_off_option::emitter_on_and_off_option( hw_monitor & hwm, const std::weak_ptr< sensor_base > & ep )
        : _hwm(hwm), _sensor(ep)
    {
        _range = [this]()
        {
            return option_range{ 0, 1, 1, 0 };
        };
    }

    void emitter_on_and_off_option::set(float value)
    {
        auto strong = _sensor.lock();
        if( ! strong )
            throw std::runtime_error( "Cannot set Emitter On/Off option, sensor is not alive" );

        if( strong->is_streaming() )
            throw std::runtime_error("Cannot change Emitter On/Off option while streaming!");

        command cmd(ds::SET_PWM_ON_OFF);
        cmd.param1 = static_cast<int>(value);

        _hwm.send(cmd);
        _record_action(*this);
    }

    float emitter_on_and_off_option::query() const
    {
        command cmd(ds::GET_PWM_ON_OFF);
        auto res = _hwm.send(cmd);
        if (res.empty())
            throw invalid_value_exception("emitter_on_and_off_option::query result is empty!");

        return (res.front());
    }

    option_range emitter_on_and_off_option::get_range() const
    {
        return *_range;
    }

    const char* external_sync_mode::get_description() const
    {
        if (_ver == 3)
            return "Inter-camera synchronization mode: 0:Default, 1:Master, 2:Slave, 3:Full Salve, 4-258:Genlock with burst count of 1-255 frames for each trigger, 259 and 260 for two frames per trigger with laser ON-OFF and OFF-ON.";
        else if (_ver == 2)
            return "Inter-camera synchronization mode: 0:Default, 1:Master, 2:Slave, 3:Full Salve, 4-258:Genlock with burst count of 1-255 frames for each trigger";
        else
            return "Inter-camera synchronization mode: 0:Default, 1:Master, 2:Slave";
    }

    alternating_emitter_option::alternating_emitter_option(hw_monitor& hwm, bool is_fw_version_using_id, hwmon_response_type no_data_to_return_opcode)
        : _hwm(hwm), _is_fw_version_using_id(is_fw_version_using_id), _no_data_to_return_opcode(no_data_to_return_opcode)
    {
        _range = [this]()
        {
            return option_range{ 0, 1, 1, 0 };
        };
    }

    void alternating_emitter_option::set(float value)
    {
        std::vector<uint8_t> pattern;

        if (static_cast<int>(value))
        {
            if (_is_fw_version_using_id)
                pattern = ds::alternating_emitter_pattern;
            else
                pattern = ds::alternating_emitter_pattern_with_name;
        }

        command cmd(ds::SETSUBPRESET, static_cast<int>(pattern.size()));
        cmd.data = pattern;
        auto res = _hwm.send(cmd);
        _record_action(*this);
    }

    float alternating_emitter_option::query() const
    {
        if (_is_fw_version_using_id)
        {
            float rv = 0.f;
            command cmd(ds::GETSUBPRESETID);
            try
            {
                hwmon_response_type response;
                auto res = _hwm.send( cmd, &response );  // avoid the throw
                if (response != _no_data_to_return_opcode) // If no subpreset is streaming, the firmware returns "NO_DATA_TO_RETURN" error
                {
                    // if a subpreset is streaming, checking this is the alternating emitter sub preset
                    if( res.size() )
                        rv = ( res[0] == ds::ALTERNATING_EMITTER_SUBPRESET_ID ) ? 1.0f : 0.f;
                    else
                        LOG_DEBUG( "alternating emitter query: " << _hwm.hwmon_error_string( cmd, response ) );
                }
            }
            catch (...)
            {
            }

            return rv;
        }
        else
        {
            command cmd(ds::GETSUBPRESETID);
            auto res = _hwm.send(cmd);
            if (res.size() > 20)
                throw invalid_value_exception("HWMON::GETSUBPRESETID invalid size");

            static std::vector<uint8_t> alt_emitter_name(ds::alternating_emitter_pattern_with_name.begin() + 2, ds::alternating_emitter_pattern_with_name.begin() + 22);
            return (alt_emitter_name == res);
        }
    }

    emitter_always_on_option::emitter_always_on_option( std::shared_ptr<hw_monitor> hwm, ds::fw_cmd _hmc_get_opcode, ds::fw_cmd _hmc_set_opcode )
        : _hwm(hwm), _hmc_get_opcode(_hmc_get_opcode), _hmc_set_opcode(_hmc_set_opcode)
    {
        // On d400 option, We use the same opcode both for set and get.
        _is_legacy = (_hmc_get_opcode == _hmc_set_opcode);

        _range = [this]()
        {
            return option_range{ 0, 1, 1, 0 };
        };
    }

    void emitter_always_on_option::set(float value)
    {
        command cmd( _hmc_set_opcode );
        // New FW opcode is different than the legacy opcode and has a reverse logic.
        // On legacy we query: `LASERONCONST` and in the new opcode we query 'APM_STROBE_ON'
        // Both return 'EMITTER_ALWAYES_ON' so they are handled differently
        bool always_on = _is_legacy ? value : ( value != 1.0f );
        cmd.param1 = static_cast<uint32_t>(always_on);

        auto strong_hwm = _hwm.lock();
        if ( !strong_hwm )
            throw camera_disconnected_exception("emitter alwayes on cannot communicate with the camera");

        strong_hwm->send(cmd);
        _record_action(*this);
    }


    float emitter_always_on_option::query() const
    {
        if( _is_legacy )
            return legacy_query();
        else
            return new_query();
    }

    float emitter_always_on_option::legacy_query() const
    {
       command cmd( _hmc_get_opcode );
            cmd.param1 = 2;
        
        auto strong_hwm = _hwm.lock();
        if ( !strong_hwm )
            throw camera_disconnected_exception("emitter alwayes on cannot communicate with the camera");

        auto res = strong_hwm->send(cmd);
        if (res.empty())
            throw invalid_value_exception("emitter_always_on_option::query result is empty!");

        return ( res.front() );
    }

    float emitter_always_on_option::new_query() const
    {
       command cmd( _hmc_get_opcode );

        auto strong_hwm = _hwm.lock();
        if ( !strong_hwm )
            throw camera_disconnected_exception("emitter alwayes on cannot communicate with the camera");

        auto res = strong_hwm->send(cmd);
        if (res.empty())
            throw invalid_value_exception("emitter_always_on_option::query result is empty!");

        // reverse logic as we query 'APM_STROBE_ON' and return 'EMITTER_ALWAYS_ON'
        bool always_on = res.front() != 1;
        return ( always_on );
    }

    option_range emitter_always_on_option::get_range() const
    {
        return *_range;
    }


    void hdr_option::set(float value)
    {
        _hdr_cfg->set(_option, value, _range);
        _record_action(*this);
    }

    float hdr_option::query() const
    {
        return _hdr_cfg->get(_option);
    }

    option_range hdr_option::get_range() const
    {
        return _range;
    }

    const char* hdr_option::get_value_description(float val) const
    {
        if (_description_per_value.find(val) != _description_per_value.end())
            return _description_per_value.at(val).c_str();
        return nullptr;
    }


    void hdr_conditional_option::set(float value)
    {
        if (_hdr_cfg->is_config_in_process())
        {
            if( _hdr_cfg->is_enabled() )
            {
                // Changing exposure while HDR is enabled was requested by customers.It is currently disabled by D400
                // FW, so we workaround it by disabling HDR,changing exposure and enabling again.Disabling/Enabling
                // resets the sequence index so we need to keep and restore it.
                auto _current_hdr_sequence_index = _hdr_cfg->get( RS2_OPTION_SEQUENCE_ID );
                _hdr_cfg->set( RS2_OPTION_HDR_ENABLED, 0, { 0, 1, 1, 0 } );
                _hdr_cfg->set( RS2_OPTION_SEQUENCE_ID, _current_hdr_sequence_index, { 0, 2, 1, 0 } );
                _hdr_option->set( value );
                _hdr_cfg->set( RS2_OPTION_HDR_ENABLED, 1, { 0, 1, 1, 0 } );
                _hdr_cfg->set( RS2_OPTION_SEQUENCE_ID, _current_hdr_sequence_index, { 0, 2, 1, 0 } );
            }
            else
                _hdr_option->set( value );
        }    
        else
        {
            if (_hdr_cfg->is_enabled())
                throw wrong_api_call_sequence_exception(
                    rsutils::string::from() << "The control - " << _uvc_option->get_description()
                                    << " - is locked while HDR mode is active.");
            else
                _uvc_option->set(value);
        }
    }

    float hdr_conditional_option::query() const
    {
        if (_hdr_cfg->is_config_in_process())
            return _hdr_option->query();
        else
            return _uvc_option->query();
    }

    option_range hdr_conditional_option::get_range() const
    {
        if (_hdr_cfg->is_config_in_process())
            return _hdr_option->get_range();
        else
            return _uvc_option->get_range();
    }

    const char* hdr_conditional_option::get_description() const
    {
        if (_hdr_cfg->is_config_in_process())
            return _hdr_option->get_description();
        else
            return _uvc_option->get_description();
    }

    bool hdr_conditional_option::is_enabled() const
    {
        if (_hdr_cfg->is_config_in_process())
            return _hdr_option->is_enabled();
        else
            return _uvc_option->is_enabled();
    }

    thermal_compensation::thermal_compensation( std::shared_ptr< ds_thermal_monitor > monitor,
                                                std::shared_ptr< option > toggle )
        : _thermal_monitor( monitor )
        , _thermal_toggle( toggle )
    {
    }

    float thermal_compensation::query(void) const
    {
        auto val = _thermal_toggle->query();
        return val;
    }

    void thermal_compensation::set(float value)
    {
        if (value < 0)
            throw invalid_value_exception("Invalid input for thermal compensation toggle: " + std::to_string(value));

        _thermal_toggle->set(value);
        _recording_function(*this);
    }

    const char* thermal_compensation::get_description() const
    {
        return "Toggle thermal compensation adjustments mechanism";
    }

    const char* thermal_compensation::get_value_description(float value) const
    {
        if (value == 0)
        {
            return "Disabled";
        }
        else
        {
            return "Enabled";
        }
    }

    //Work-around the control latency
    void thermal_compensation::create_snapshot(std::shared_ptr<option>& snapshot) const
    {
        snapshot = std::make_shared<const_value_option>(get_description(), 0.f);
    }
} // namespace librealsense
