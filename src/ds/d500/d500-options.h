// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/platform/uvc-option.h>
#include "ds/ds-private.h"
#include "ds/ds-options.h"
#include "core/options-container.h"
#include "option.h"

#include <rsutils/lazy.h>

#include <atomic>
#include <mutex>


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
            return "Inter-camera synchronization mode: 2:Internal, 3:External";
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
    
    // Dual-RGB rectification toggle, sent over the HWM CUSTOM_CMD with the DUAL_RGB_RECTIFY sub-command.
    // Temporary until FW exposes a dedicated XU: there is no matching read command, so query() returns
    // the last value set.
    class dual_rgb_rectification_option : public bool_option
    {
    public:
        dual_rgb_rectification_option( std::shared_ptr< hw_monitor > hwm, const std::weak_ptr< sensor_base > & ep );

        void set( float value ) override;
        const char * get_description() const override
        {
            return "Dual RGB rectification enabling ON (1) / OFF (0). Can only be set before streaming";
        }

        static uint32_t const DUAL_RGB_RECTIFY_SUB_CMD = 0x29;

    private:
        std::shared_ptr< hw_monitor > _hwm;
        std::weak_ptr< sensor_base > _sensor;
    };

    // Device-side depth-to-color alignment. Enabling it replaces the raw depth payload (over USB) with Z16 projected into the color viewport,
    // so the mode may only change while the sensor is closed. Last known value is cached because intrinsics lookups consult it per frame.
    class d500_enable_aligned_depth_option : public uvc_xu_option< uint8_t >
    {
    public:
        explicit d500_enable_aligned_depth_option( const std::weak_ptr< uvc_sensor > & raw_ep );

        void set( float value ) override;
        float query() const override;
        bool is_read_only() const override;

        // Cached state, free of a firmware round-trip
        bool is_aligned() const { return _aligned; }
        void add_observer( std::function< void( bool ) > observer ) { _observers.push_back( std::move( observer ) ); }

    private:
        void update( bool aligned ) const;

        std::weak_ptr< sensor_base > _sensor;
        mutable std::atomic< bool > _aligned;
        std::vector< std::function< void( bool ) > > _observers;
    };

    // Which exposure classes produce depth. The firmware applies the mode at stream start, and in Full Passive
    // it takes the laser and the AE policy over - the policy is moved to Color Priority along with the mode,
    // since the firmware keeps reporting whatever was selected before.
    class passive_depth_mode_option : public uvc_xu_option< uint8_t >
    {
    public:
        passive_depth_mode_option( const std::weak_ptr< uvc_sensor > & raw_ep,
                                   const std::map< float, std::string > & description_per_value );

        void set( float value ) override;
        bool is_read_only() const override;

        // Cached state, free of a firmware round-trip: the gated controls consult it on every UI refresh,
        // and it only changes under set() below.
        bool is_full_passive() const { return _full_passive; }

        // Wired once while the device is built, before it is reachable by any other thread - the same
        // contract as d500_enable_aligned_depth_option::add_observer.
        void set_ae_policy_option( const std::weak_ptr< option > & ae_policy ) { _ae_policy = ae_policy; }

    private:
        std::weak_ptr< option > _ae_policy;
        std::atomic< bool > _full_passive;
        // Pairs the write with its read-back, so concurrent sets cannot leave the cache on the losing value.
        std::mutex _set_mutex;
    };

    // A control that Full Passive Depth takes over: the firmware forces the laser off there, so the host
    // refuses changes and reports the control locked instead of letting a write silently do nothing.
    // raw_ep is passed for a control the firmware also fixes for the duration of a stream.
    class passive_depth_locked_option : public proxy_option
    {
    public:
        passive_depth_locked_option( std::shared_ptr< option > proxy,
                                     const std::weak_ptr< passive_depth_mode_option > & passive_depth_mode,
                                     const std::weak_ptr< uvc_sensor > & raw_ep = {} );

        void set( float value ) override;
        bool is_read_only() const override;

    private:
        std::weak_ptr< passive_depth_mode_option > _passive_depth_mode;
        std::weak_ptr< uvc_sensor > _raw_ep;
    };

    // Auto-exposure policy of a dual-RGB depth sensor. Full Passive Depth locks the policy to Color Priority in
    // firmware, so Hybrid drops out of the reported range and is refused while that mode is active.
    class colored_ir_ae_policy_option : public uvc_xu_option< uint8_t >
    {
    public:
        colored_ir_ae_policy_option( const std::weak_ptr< uvc_sensor > & raw_ep,
                                     const std::map< float, std::string > & description_per_value,
                                     const std::weak_ptr< passive_depth_mode_option > & passive_depth_mode );

        void set( float value ) override;
        option_range get_range() const override;
        bool is_read_only() const override;

    private:
        std::weak_ptr< passive_depth_mode_option > _passive_depth_mode;
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

    class d500_mipi_gyro_sensitivity_option : public uvc_pu_option
    {
    public:
        explicit d500_mipi_gyro_sensitivity_option( const std::weak_ptr< uvc_sensor > & ep );

        void set( float value ) override;
        bool is_read_only() const override;
        const char * get_description() const override;
        const char * get_value_description( float value ) const override;
    };

} // namespace librealsense
