// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/platform/uvc-option.h>
#include "ds/ds-private.h"
#include "ds/ds-options.h"
#include "core/options-container.h"
#include "option.h"

#include <rsutils/lazy.h>


namespace librealsense
{
    class rgb_tnr_option : public option
    {
    public:
        rgb_tnr_option(std::shared_ptr<hw_monitor> hwm, const std::weak_ptr< sensor_base > & ep);
        virtual ~rgb_tnr_option() = default;
        virtual void set(float value) override;
        virtual float query() const override;
        virtual option_range get_range() const override;
        virtual bool is_enabled() const override { return true; }
        virtual const char* get_description() const override
        {
            return "RGB Temporal Noise Reduction enabling ON (1) / OFF (0). Can only be set before streaming";
        }
        virtual void enable_recording(std::function<void(const option&)> record_action) override { _record_action = record_action; }

        static int const GET_TNR_STATE = 0;
        static int const SET_TNR_STATE = 1;

    private:
        std::function<void(const option&)> _record_action = [](const option&) {};
        rsutils::lazy< option_range > _range;
        std::shared_ptr<hw_monitor> _hwm;
        std::weak_ptr< sensor_base > _sensor;
    };

    class d500_align_depth_option : public uvc_xu_option< uint8_t >
    {
    public:
        explicit d500_align_depth_option( const std::weak_ptr< uvc_sensor > & ep );

        option_range get_range() const override;
    };
    
    class temperature_option : public readonly_option
    {
    public:
        enum class temperature_component : uint8_t
        {
            LEFT_PROJ = 1,
            LEFT_IR,
            IMU,
            RGB,
            RIGHT_IR,
            RIGHT_PROJ,
            HKR_PVT,
            SHT4XX,
            SMCU,
            COUNT
        };
        explicit temperature_option( std::shared_ptr< hw_monitor > hwm,
                                     temperature_component component,
                                     const char * description );
        float query() const override;
        inline option_range get_range() const override { return *_range; }
        inline bool is_enabled() const override { return true; }
        
        inline const char* get_description() const override
        {
            return _description;
        }
        virtual void enable_recording(std::function<void(const option&)> record_action) override { _record_action = record_action; }


    private:
        std::function<void(const option&)> _record_action = [](const option&) {};
        rsutils::lazy< option_range > _range;
        std::shared_ptr<hw_monitor> _hwm;
        temperature_component _component;
        const char* _description;
    };

    class temperature_xu_option : public uvc_xu_option<int16_t>, 
        public readonly_option
    {
    public:

        explicit temperature_xu_option(const std::weak_ptr< uvc_sensor >& ep,
            platform::extension_unit xu,
            uint8_t id,
            std::string description);

        virtual float query() const override;
        virtual void set(float value) override;
        inline bool is_enabled() const override { return true; }
        virtual void enable_recording(std::function<void(const option&)> record_action) override 
        { uvc_xu_option<int16_t>::enable_recording(record_action); }
    };

    class d500_external_sync_mode : public option
    {
    public:
        d500_external_sync_mode( hw_monitor & hwm,
                                 const std::weak_ptr< sensor_base > & ep,
                                 const std::map< float, std::string > & description_per_value );

        virtual ~d500_external_sync_mode() = default;
        virtual void set( float value ) override;
        virtual float query() const override;
        virtual option_range get_range() const override { return _range; }
        virtual bool is_enabled() const override { return true; }
        virtual bool is_read_only() const override;
        const char * get_description() const override
        {
            return "Inter-camera synchronization mode: 0:No sync, 1:RGB Master, 2:PWM Master, 3:External Master";
        }
        const char * get_value_description( float val ) const override;

        void enable_recording( std::function< void( const option & ) > record_action ) override
        {
            _record_action = record_action;
        }

    private:
        std::function< void( const option & ) > _record_action = []( const option & ) {
        };
        option_range _range;
        const std::map< float, std::string > _description_per_value;
        hw_monitor & _hwm;
        std::weak_ptr< sensor_base > _sensor;
    };
    
    class d500_thermal_compensation_option : public bool_option
    {
    public:

        d500_thermal_compensation_option( std::shared_ptr< hw_monitor > hwm );

        virtual void set(float value) override;

    private:
        std::weak_ptr< hw_monitor > _hwm;
    };
    
    class power_line_freq_option : public uvc_pu_option
    {
    public:
        explicit power_line_freq_option(const std::weak_ptr< uvc_sensor >& ep, rs2_option id,
            const std::map< float, std::string >& description_per_value);

        virtual option_range get_range() const override
        {
            // this hardcoded max range has been done because 
            // some d500 devices do not support the "AUTO" value
            auto range = uvc_pu_option::get_range();
            range.max = 2.f;
            return range;
        }
    };

} // namespace librealsense
