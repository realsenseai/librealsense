// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/platform/uvc-option.h>
#include "ds-private.h"

#include "algo.h"
#include "error-handling.h"
#include "../hdr-config.h"
#include <rsutils/lazy.h>

namespace librealsense
{
    class emitter_option : public uvc_xu_option<uint8_t>
    {
    public:
        const char* get_value_description(float val) const override;
        explicit emitter_option( const std::weak_ptr< uvc_sensor > & ep );
    };

    class asic_and_projector_temperature_options : public readonly_option
    {
    public:
        float query() const override;

        option_range get_range() const override;

        bool is_enabled() const override;

        const char* get_description() const override;

        explicit asic_and_projector_temperature_options( const std::weak_ptr< uvc_sensor > & ep, rs2_option opt );

    private:
        std::weak_ptr<uvc_sensor>   _ep;
        rs2_option                  _option;
    };

    class hid_sensor;

    class motion_module_temperature_option : public readonly_option
    {
    public:
        float query() const override;

        option_range get_range() const override;

        bool is_enabled() const override;

        const char* get_description() const override;

        explicit motion_module_temperature_option(hid_sensor& ep);

    private:
        const std::string custom_sensor_name = "custom";
        const std::string report_name = "data-field-custom-value_2";
        hid_sensor& _ep;
    };

    class enable_auto_exposure_option : public option_base
    {
    public:
        void set(float value) override;

        float query() const override;

        bool is_enabled() const override { return true; }

        const char* get_description() const override
        {
            return "Enable/disable auto-exposure";
        }

        auto_exposure_mechanism* get_auto_exposure() { return _auto_exposure.get(); }
        bool to_add_frames() { return _to_add_frames.load(); }

        enable_auto_exposure_option(synthetic_sensor* fisheye_ep,
                                    std::shared_ptr<auto_exposure_mechanism> auto_exposure,
                                    std::shared_ptr<auto_exposure_state> auto_exposure_state,
                                    const option_range& opt_range);

    private:
        std::shared_ptr<auto_exposure_state>         _auto_exposure_state;
        std::atomic<bool>                            _to_add_frames;
        std::shared_ptr<auto_exposure_mechanism>     _auto_exposure;
    };

    class auto_exposure_mode_option : public option_base
    {
    public:
        auto_exposure_mode_option(std::shared_ptr<auto_exposure_mechanism> auto_exposure,
                                  std::shared_ptr<auto_exposure_state> auto_exposure_state,
                                  const option_range& opt_range,
                                  const std::map<float, std::string>& description_per_value);

        void set(float value) override;

        float query() const override;

        bool is_enabled() const override { return true; }

        const char* get_description() const override
        {
            return "Auto-Exposure Mode";
        }

        const char* get_value_description(float val) const override;

    private:
        const std::map<float, std::string>          _description_per_value;
        std::shared_ptr<auto_exposure_state>        _auto_exposure_state;
        std::shared_ptr<auto_exposure_mechanism>    _auto_exposure;
    };

    class auto_exposure_step_option : public option_base
    {
    public:
        auto_exposure_step_option(std::shared_ptr<auto_exposure_mechanism> auto_exposure,
                                  std::shared_ptr<auto_exposure_state> auto_exposure_state,
                                  const option_range& opt_range);

        void set(float value) override;

        float query() const override;

        bool is_enabled() const override { return true; }

        const char* get_description() const override
        {
            return "Auto-Exposure converge step";
        }

    private:
        std::shared_ptr<auto_exposure_state>        _auto_exposure_state;
        std::shared_ptr<auto_exposure_mechanism>    _auto_exposure;
    };

    class auto_exposure_antiflicker_rate_option : public option_base
    {
    public:
        auto_exposure_antiflicker_rate_option(std::shared_ptr<auto_exposure_mechanism> auto_exposure,
                                              std::shared_ptr<auto_exposure_state> auto_exposure_state,
                                              const option_range& opt_range,
                                              const std::map<float, std::string>& description_per_value);

        void set(float value) override;

        float query() const override;

        bool is_enabled() const override { return true; }

        const char* get_description() const override
        {
            return "Auto-Exposure anti-flicker";
        }

        const char* get_value_description(float val) const override;

    private:
        const std::map<float, std::string>           _description_per_value;
        std::shared_ptr<auto_exposure_state>         _auto_exposure_state;
        std::shared_ptr<auto_exposure_mechanism>     _auto_exposure;
    };

    class depth_scale_option : public option, public observable_option
    {
    public:
        depth_scale_option(hw_monitor& hwm);
        virtual ~depth_scale_option() = default;
        virtual void set(float value) override;
        virtual float query() const override;
        virtual option_range get_range() const override;
        virtual bool is_enabled() const override { return true; }

        const char* get_description() const override
        {
            return "Number of meters represented by a single depth unit";
        }
        void enable_recording(std::function<void(const option &)> record_action) override
        {
            _record_action = record_action;
        }

    private:
        ds::depth_table_control get_depth_table(ds::advanced_query_mode mode) const;
        std::function<void(const option &)> _record_action = [](const option&) {};
        rsutils::lazy< option_range > _range;
        hw_monitor& _hwm;
    };

    class external_sync_mode : public option
    {
    public:
        external_sync_mode( hw_monitor & hwm,
                            const std::weak_ptr< sensor_base > & depth_ep = std::weak_ptr< sensor_base >(),
                            int ver = 1 );  // ver = 1, for firmware 5.9.15.1 and later, INTERCAM_SYNC_MAX is 2 with master and slave mode only.
                                            // ver = 2, for firmware 5.12.4.0 and later, INTERCAM_SYNC_MAX is 258 by adding FULL SLAVE mode and genlock with trigger frequency 1 - 255.
                                            // ver = 3  for firmware 5.12.12.100 and later, INTERCAM_SYNC_MAX is 260 by adding genlock with laser on-off and Off-On two frames.

        virtual ~external_sync_mode() = default;
        virtual void set(float value) override;
        virtual float query() const override;
        virtual option_range get_range() const override;
        virtual bool is_enabled() const override { return true; }
        virtual bool is_read_only() const override;
        const char* get_description() const override;

        void enable_recording(std::function<void(const option &)> record_action) override
        {
            _record_action = record_action;
        }
    private:
        std::function<void(const option &)> _record_action = [](const option&) {};
        rsutils::lazy< option_range > _range;
        hw_monitor& _hwm;
        std::weak_ptr< sensor_base > _sensor;
        int _ver;
    };

    class emitter_on_and_off_option : public option
    {
    public:
        emitter_on_and_off_option( hw_monitor & hwm, const std::weak_ptr< sensor_base > & depth_ep );
        virtual ~emitter_on_and_off_option() = default;
        virtual void set(float value) override;
        virtual float query() const override;
        virtual option_range get_range() const override;
        virtual bool is_enabled() const override { return true; }
        virtual const char* get_description() const override
        {
            return "Emitter On/Off Mode: 0:disabled(default), 1:enabled(emitter toggles between on and off). Can only be set before streaming";
        }
        virtual void enable_recording(std::function<void(const option &)> record_action) override {_record_action = record_action;}

    private:
        std::function<void(const option &)> _record_action = [](const option&) {};
        rsutils::lazy< option_range > _range;
        hw_monitor& _hwm;
        std::weak_ptr< sensor_base > _sensor;
    };

    class alternating_emitter_option : public option
    {
    public:
        alternating_emitter_option(hw_monitor& hwm, bool is_fw_version_using_id, bool expect_no_data_to_return = false, hwmon_response_type no_data_to_return_opcode = 0);
        virtual ~alternating_emitter_option() = default;
        virtual void set(float value) override;
        virtual float query() const override;
        virtual option_range get_range() const override { return *_range; }
        virtual bool is_enabled() const override { return true; }
        virtual const char* get_description() const override
        {
            return "Alternating emitter pattern, toggled on/off on per-frame basis";
        }
        virtual void enable_recording(std::function<void(const option &)> record_action) override { _record_action = record_action; }

    private:
        std::function<void(const option &)> _record_action = [](const option&) {};
        rsutils::lazy< option_range > _range;
        hw_monitor& _hwm;
        bool _is_fw_version_using_id;
        bool _expect_no_data_to_return;
        hwmon_response_type _no_data_to_return_opcode;
    };

    class emitter_always_on_option : public option
    {
    public:
        emitter_always_on_option( std::shared_ptr< hw_monitor > hwm, uint8_t _hmc_get_opcode, uint8_t _hmc_set_opcode );
        virtual ~emitter_always_on_option() = default;
        virtual void set(float value) override;
        virtual float query() const override;
        virtual option_range get_range() const override;
        virtual bool is_enabled() const override { return true; }
        virtual const char* get_description() const override
        {
            return "Emitter always on mode: 0:disabled(default), 1:enabled";
        }
        virtual void enable_recording(std::function<void(const option &)> record_action) override { _record_action = record_action; }

    private:
        float legacy_query() const;
        float new_query() const;
        std::function<void(const option &)> _record_action = [](const option&) {};
        rsutils::lazy< option_range > _range;
        std::weak_ptr<hw_monitor> _hwm;
        // Stored as raw HWM byte, not fw_cmd: this option is instantiated with
        // opcodes from more than one family enum (ds::LASERONCONST for D400 vs
        // d500_fw_cmd::APM_STROBE_SET/GET for D500), and unscoped enums don't
        // cross-convert. The underlying byte is what the HWM cares about.
        uint8_t _hmc_get_opcode, _hmc_set_opcode;
        bool _is_legacy;
    };

    class hdr_option : public option
    {
    public:
        hdr_option(std::shared_ptr<hdr_config> hdr_cfg, rs2_option option, option_range range)
            : _hdr_cfg(hdr_cfg), _option(option), _range(range) {}

        hdr_option(std::shared_ptr<hdr_config> hdr_cfg, rs2_option option, option_range range, const std::map<float, std::string>& description_per_value)
            : _hdr_cfg(hdr_cfg), _option(option), _range(range), _description_per_value(description_per_value) {}

        virtual ~hdr_option() = default;
        virtual void set(float value) override;
        virtual float query() const override;
        virtual option_range get_range() const override;
        virtual bool is_enabled() const override { return true; }
        virtual const char* get_description() const override { return "HDR Option"; }
        virtual void enable_recording(std::function<void(const option&)> record_action) override { _record_action = record_action; }
        virtual const char* get_value_description(float) const override;

    private:
        std::function<void(const option&)> _record_action = [](const option&) {};
        std::shared_ptr<hdr_config> _hdr_cfg;
        rs2_option _option;
        option_range _range;
        const std::map<float, std::string> _description_per_value;
    };

    // used for options that change their behavior when hdr configuration is in process
    class hdr_conditional_option : public option
    {
    public:
        hdr_conditional_option(std::shared_ptr<hdr_config> hdr_cfg,
            std::shared_ptr<option> uvc_option,
            std::shared_ptr<option> hdr_option):
            _hdr_cfg(hdr_cfg),
            _uvc_option(uvc_option),
            _hdr_option(hdr_option) {}

        virtual ~hdr_conditional_option() = default;
        virtual void set(float value) override;
        virtual float query() const override;
        virtual option_range get_range() const override;
        virtual bool is_enabled() const override;
        virtual const char* get_description() const override;
        virtual void enable_recording(std::function<void(const option&)> record_action) override { _record_action = record_action; }

    private:
        std::function<void(const option&)> _record_action = [](const option&) {};
        std::shared_ptr<hdr_config> _hdr_cfg;
        std::shared_ptr<option> _uvc_option;
        std::shared_ptr<option> _hdr_option;
    };
    
    class ds_thermal_monitor;
    class thermal_compensation : public option
    {
    public:
        thermal_compensation( std::shared_ptr< ds_thermal_monitor > monitor, std::shared_ptr< option > toggle );

        void set( float value ) override;
        float query() const override;

        option_range get_range() const override { return option_range{ 0, 1, 1, 0 }; }
        bool is_enabled() const override { return true; }

        const char * get_description() const override;
        const char * get_value_description( float value ) const override;
        void create_snapshot( std::shared_ptr< option > & snapshot ) const override;

        void enable_recording( std::function< void( const option & ) > record_action ) override
        {
            _recording_function = record_action;
        }

    private:
        std::shared_ptr< ds_thermal_monitor > _thermal_monitor;
        std::shared_ptr< option > _thermal_toggle;

        std::function< void( const option & ) > _recording_function = []( const option & ) {};
    };
} // namespace librealsense
