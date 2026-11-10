// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#pragma once

#include "d400-private.h"
#include "ds/ds-options.h"

#include "algo.h"
#include "error-handling.h"
#include "../../hdr-config.h"

#include <rsutils/lazy.h>


namespace librealsense
{

    class asic_temperature_option_mipi : public readonly_option
    {
    public:
        float query() const override;

        option_range get_range() const override;

        inline bool is_enabled() const override {return true;}

        inline const char* get_description() const override {return "Current Asic Temperature (degree celsius)";}

        explicit asic_temperature_option_mipi(std::shared_ptr<hw_monitor> hwm, rs2_option opt);

    private:
        rs2_option                  _option;
        std::shared_ptr<hw_monitor>  _hw_monitor;
    };

    // MIPI has no XU transport for the thermal loop toggle, so it is driven by the TC_CMD HWM
    // opcode. The command is write-only, hence query() returns the last value set - defaulting to
    // on, as FW enables the loop by default on D45x.
    class thermal_compensation_option_mipi : public bool_option
    {
    public:
        explicit thermal_compensation_option_mipi( std::shared_ptr< hw_monitor > hwm );

        void set( float value ) override;

        const char * get_description() const override { return "Toggle Thermal Compensation Mechanism"; }

    private:
        std::weak_ptr< hw_monitor > _hw_monitor;
    };

    class projector_temperature_option_mipi : public readonly_option
    {
    public:
        float query() const override;

        option_range get_range() const override;

        inline bool is_enabled() const override {return true;}

        inline const char* get_description() const override {return "Current Projector Temperature (degree celsius)";}

        explicit projector_temperature_option_mipi(std::shared_ptr<hw_monitor> hwm, rs2_option opt);

    private:
        rs2_option                  _option;
        std::shared_ptr<hw_monitor>  _hw_monitor;
    };

    class limits_option;

    class auto_exposure_limit_option : public option_base
    {
    public:
        auto_exposure_limit_option( hw_monitor & hwm,
                                    const std::weak_ptr< sensor_base > & depth_ep,
                                    option_range range,
                                    std::shared_ptr< limits_option > exposure_limit_enable,
                                    bool ae_gain_limits_new_opcode = false );
        virtual ~auto_exposure_limit_option() = default;
        virtual void set(float value) override;
        virtual float query() const override;
        virtual option_range get_range() const override;
        virtual bool is_enabled() const override { return true; }
        virtual bool is_read_only() const override;
        virtual const char* get_description() const override
        {
            return "Exposure limit is in microseconds. If the requested exposure limit is greater than frame time, it will be set to frame time at runtime. Setting will not take effect until next streaming session.";
        }
        virtual void enable_recording(std::function<void(const option&)> record_action) override { _record_action = record_action; }
        void set_using_new_opcode( float value );
        void set_using_old_opcode( float value );
        float query_using_new_opcode() const;
        float query_using_old_opcode() const;

    private:
        std::function<void(const option&)> _record_action = [](const option&) {};
        rsutils::lazy< option_range > _range;
        hw_monitor& _hwm;
        std::weak_ptr< sensor_base > _sensor;
        std::weak_ptr<limits_option> _exposure_limit_toggle;
        bool _new_opcode;
    };

    class auto_gain_limit_option : public option_base
    {
    public:
        auto_gain_limit_option( hw_monitor & hwm,
                                const std::weak_ptr< sensor_base > & depth_ep,
                                option_range range,
                                std::shared_ptr< limits_option > gain_limit_enable,
                                bool new_opcode = false );
        virtual ~auto_gain_limit_option() = default;
        virtual void set(float value) override;
        virtual float query() const override;
        virtual option_range get_range() const override;
        virtual bool is_enabled() const override { return true; }
        virtual bool is_read_only() const override;
        virtual const char* get_description() const override
        {
            return "Gain limits ranges from 16 to 248. If the requested gain limit is less than 16, it will be set to 16. If the requested gain limit is greater than 248, it will be set to 248. Setting will not take effect until next streaming session.";
        }
        virtual void enable_recording(std::function<void(const option&)> record_action) override { _record_action = record_action; }
        void set_using_new_opcode( float value );
        void set_using_old_opcode( float value );
        float query_using_new_opcode() const;
        float query_using_old_opcode() const;

    private:
        std::function<void(const option&)> _record_action = [](const option&) {};
        rsutils::lazy< option_range > _range;
        hw_monitor& _hwm;
        std::weak_ptr< sensor_base > _sensor;
        std::weak_ptr<limits_option> _gain_limit_toggle;
        bool _new_opcode;
    };

    // Auto-Limits Enable/ Disable
    class limits_option : public option
    {
    public:

        limits_option( rs2_option option,
                       option_range range,
                       const char * description,
                       hw_monitor & hwm,
                       bool new_opcode = false );
        virtual void set(float value) override;
        virtual float query() const override;
        virtual option_range get_range() const override { return _toggle_range; };
        virtual bool is_enabled() const override { return true; }
        virtual const char* get_description() const override { return _description; };
        virtual void enable_recording(std::function<void(const option&)> record_action) override { _record_action = record_action; }
        virtual const char* get_value_description(float val) const override
        {
            if (_description_per_value.find(val) != _description_per_value.end())
                return _description_per_value.at(val).c_str();
            return nullptr;
        };
        void set_cached_limit(float value) { _cached_limit = value; };
        float get_cached_limit() { return _cached_limit; };
        void set_using_new_opcode( float value, float set_limit );
        void set_using_old_opcode( float value, float set_limit );
        std::vector< uint8_t > query_using_new_opcode() const;
        std::vector< uint8_t > query_using_old_opcode() const; 

    private:
        std::function<void(const option&)> _record_action = [](const option&) {};
        rs2_option _option;
        option_range _toggle_range;
        const std::map<float, std::string> _description_per_value;
        float _cached_limit;         // used to cache contol limit value when toggle is switched to off
        const char* _description;
        hw_monitor& _hwm;
        bool _new_opcode; 
    };

    class hid_sensor;
    class gyro_sensitivity_option: public option_base
    {
    public:
        gyro_sensitivity_option( const std::weak_ptr< hid_sensor > & sensor, const option_range & opt_range )
            : option_base( opt_range )
            , _value( opt_range.def )
            , _sensor( sensor )
        {
            // The default is only a fallback for reporting; nothing is written to FW until the user calls set()
        }
        virtual ~gyro_sensitivity_option() = default;
        virtual void set( float value ) override;
        virtual float query() const override;
        virtual bool is_enabled() const override { return true; }
        virtual const char * get_description() const override;
        const char * get_value_description( float value ) const override;
        virtual void enable_recording( std::function< void( const option & ) > record_action ) override
        {
            _record_action = record_action;
        }
        virtual bool is_read_only() const override;

    private:
        mutable float _value;  // user value, else last value read from FW, else the default
        bool _user_set = false;
        std::weak_ptr< hid_sensor > _sensor;
        std::function< void( const option & ) > _record_action = []( const option & ) {};

    };
}
