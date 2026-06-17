// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#include "device.h"
#include "image.h"
#include "metadata-parser.h"

#include <src/core/matcher-factory.h>
#include <src/proc/color-formats-converter.h>
#include <src/core/advanced_mode.h>
#include <src/eth-config-device.h>

#include "d500-info.h"
#include "d500-private.h"
#include "ds/ds-options.h"
#include "ds/ds-timestamp.h"
#include "d500-active.h"
#include "d500-color.h"
#include "d500-dual-rgb.h"
#include "d500-motion.h"
#include "d500-safety.h"
#include "d500-depth-mapping.h"
#include "d500-object-detection.h"
#include "sync.h"
#include <src/ds/ds-thermal-monitor.h>
#include <src/ds/d500/d500-options.h>
#include <src/ds/d500/d500-auto-calibration.h>
#include <src/ds/features/close-range-filter-feature.h>

#include <src/platform/platform-utils.h>

#include "firmware_logger_device.h"
#include "device-calibration.h"

#include <rsutils/string/hexdump.h>
using rsutils::string::hexdump;

#include <mutex>
#include <chrono>
#include <vector>
#include <iterator>
#include <cstddef>


namespace librealsense
{
    std::shared_ptr< matcher > create_default_matcher( std::vector < std::shared_ptr< stream_interface > > const & streams )
    {
        // Create default matcher for all non-null streams
        std::vector< stream_interface * > streams_to_match;
        for( auto stream : streams )
        {
            if( stream )
                streams_to_match.push_back( stream.get() );
        }
        return matcher_factory::create( RS2_MATCHER_DEFAULT, streams_to_match );
    }

    // default d500 device
    // used only as fallback for partial device creation when enabled by config
    class rs500_device
        : public d500_device
        , public ds_advanced_mode_base
        , public extended_firmware_logger_device
    {
    public:
        rs500_device( std::shared_ptr< const d500_info > const & dev_info )
            : device( dev_info )
            , backend_device( dev_info )
            , d500_device( dev_info )
            , ds_advanced_mode_base()
            , extended_firmware_logger_device( dev_info, d500_device::_hw_monitor, get_firmware_logs_command() )
        {
            ds_advanced_mode_base::initialize_advanced_mode( this );
        }

        std::shared_ptr< matcher > create_matcher( const frame_holder & frame ) const override
        {
            std::vector< std::shared_ptr< stream_interface > > streams = { _depth_stream, _left_ir_stream, _right_ir_stream };
            return create_default_matcher( streams );
        }

        std::vector< tagged_profile > get_profiles_tags() const override
        {
            std::vector< tagged_profile > tags;

            auto usb_spec = get_usb_spec();
            if( usb_spec >= platform::usb3_type || usb_spec == platform::usb_undefined )
            {
                tags.push_back( { RS2_STREAM_DEPTH, -1, 1280, 720, RS2_FORMAT_Z16, 30,profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT } );
                tags.push_back( { RS2_STREAM_INFRARED, 1, 1280, 720, RS2_FORMAT_RGB8, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT } );
                tags.push_back( { RS2_STREAM_INFRARED, 2, 1280, 720, RS2_FORMAT_RGB8, 30, profile_tag::PROFILE_TAG_SUPERSET } );
            }
            else
            {
                tags.push_back( { RS2_STREAM_DEPTH, -1, 640, 480, RS2_FORMAT_Z16, 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT } );
                tags.push_back( { RS2_STREAM_INFRARED, 1, 640, 480, RS2_FORMAT_RGB8, 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT } );
                tags.push_back( { RS2_STREAM_INFRARED, 2, 640, 480, RS2_FORMAT_RGB8, 15, profile_tag::PROFILE_TAG_SUPERSET } );
            }

            return tags;
        };
    };


    // D585 or D535, dual RGB variant. No dedicated color sensor.
    class rs5x5_device
        : public d500_active
        , public d500_motion
        , public d500_dual_rgb
        , public ds_advanced_mode_base
        , public extended_firmware_logger_device
    {
    public:
        rs5x5_device( std::shared_ptr< const d500_info > const & dev_info )
            : device( dev_info )
            , backend_device( dev_info )
            , d500_device( dev_info )
            , d500_active( dev_info )
            , d500_motion( dev_info )
            , d500_dual_rgb( dev_info )
            , ds_advanced_mode_base()
            , extended_firmware_logger_device( dev_info, d500_device::_hw_monitor, get_firmware_logs_command() )
        {
            ds_advanced_mode_base::initialize_advanced_mode( this );
        }

        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override
        {

            std::vector< std::shared_ptr< stream_interface > > streams = { _depth_stream, _left_ir_stream, _right_ir_stream,
                                                                           _color_stream_1, _color_stream_2,
                                                                           _ds_motion_common->get_accel_stream(),
                                                                           _ds_motion_common->get_gyro_stream() };
            return create_default_matcher( streams );
        }

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;

            // TODO - Infrared streams should be RGB8 once dual RGB phase 2 FW is ready.
            tags.push_back({ RS2_STREAM_DEPTH, -1, 1280, 720, RS2_FORMAT_Z16, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({ RS2_STREAM_INFRARED, -1, 1280, 720, RS2_FORMAT_Y8, 30, profile_tag::PROFILE_TAG_SUPERSET });
            tags.push_back({ RS2_STREAM_COLOR, 1, 1280, 720, RS2_FORMAT_RGB8, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({ RS2_STREAM_COLOR, 2, 1280, 720, RS2_FORMAT_RGB8, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({ RS2_STREAM_GYRO, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_200, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({ RS2_STREAM_ACCEL, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_100, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });

            return tags;
        };
    };


    // D585 or D535 with dedicated color sensor. Can be with IR filter on lens or without.
    class rs5x5_dedicated_color_device
        : public d500_active
        , public d500_color
        , public d500_motion
        , public d500_object_detection
        , public ds_advanced_mode_base
        , public extended_firmware_logger_device
    {
    public:
        rs5x5_dedicated_color_device( std::shared_ptr< const d500_info > const & dev_info )
            : device( dev_info )
            , backend_device( dev_info )
            , d500_device( dev_info )
            , d500_active( dev_info )
            , d500_color( dev_info, RS2_FORMAT_M420 )
            , d500_motion( dev_info )
            , d500_object_detection( dev_info )
            , ds_advanced_mode_base()
            , extended_firmware_logger_device( dev_info, d500_device::_hw_monitor, get_firmware_logs_command() )
        {
            ds_advanced_mode_base::initialize_advanced_mode( this );

            // Improved Close Range Depth - USB toggle
            register_feature( std::make_shared< close_range_filter_feature >(
                    dynamic_cast< d500_depth_sensor & >( get_depth_sensor() ) ) );
        }

        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override
        {

            std::vector< std::shared_ptr< stream_interface > > streams = { _depth_stream, _left_ir_stream, _right_ir_stream, _color_stream,
                                                                           _ds_motion_common->get_accel_stream(),
                                                                           _ds_motion_common->get_gyro_stream(),
                                                                           _object_detection_stream };
            return create_default_matcher( streams );
        }

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;

            tags.push_back({ RS2_STREAM_COLOR, -1, 1280, 720, RS2_FORMAT_RGB8, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({ RS2_STREAM_DEPTH, -1, 1280, 720, RS2_FORMAT_Z16, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({ RS2_STREAM_INFRARED, -1, 1280, 720, RS2_FORMAT_Y8, 30, profile_tag::PROFILE_TAG_SUPERSET });
            tags.push_back({ RS2_STREAM_GYRO, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_200, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({ RS2_STREAM_ACCEL, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_100, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({ RS2_STREAM_OBJECT_DETECTION, -1, -1, -1, RS2_FORMAT_Y8, -1, profile_tag::PROFILE_TAG_SUPERSET });
            
            return tags;
        };
    };


    class rs585_legacy_device // Used for demo to customers. D585S without the safety.
        : public d500_active
        , public d500_color
        , public d500_motion
        , public d500_object_detection
        , public ds_advanced_mode_base
        , public extended_firmware_logger_device
    {
    public:
        rs585_legacy_device( std::shared_ptr< const d500_info > const & dev_info )
            : device( dev_info )
            , backend_device( dev_info )
            , d500_device( dev_info )
            , d500_active( dev_info )
            , d500_color( dev_info, RS2_FORMAT_M420 )
            , d500_motion( dev_info )
            , d500_object_detection( dev_info )
            , ds_advanced_mode_base()
            , extended_firmware_logger_device( dev_info, d500_device::_hw_monitor, get_firmware_logs_command() )
        {
            ds_advanced_mode_base::initialize_advanced_mode( this );
        }

        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override
        {

            std::vector< std::shared_ptr< stream_interface > > streams = { _depth_stream, _left_ir_stream, _right_ir_stream, _color_stream,
                                                                           _ds_motion_common->get_accel_stream(),
                                                                           _ds_motion_common->get_gyro_stream(),
                                                                           _object_detection_stream };
            return create_default_matcher( streams );
        }

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;

            tags.push_back({ RS2_STREAM_COLOR, -1, 1280, 720, RS2_FORMAT_RGB8, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({ RS2_STREAM_DEPTH, -1, 1280, 720, RS2_FORMAT_Z16, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({ RS2_STREAM_INFRARED, -1, 1280, 720, RS2_FORMAT_Y8, 30, profile_tag::PROFILE_TAG_SUPERSET });
            tags.push_back({ RS2_STREAM_GYRO, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_200, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({ RS2_STREAM_ACCEL, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_100, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({ RS2_STREAM_OBJECT_DETECTION, -1, -1, -1, RS2_FORMAT_Y8, -1, profile_tag::PROFILE_TAG_SUPERSET });
            
            return tags;
        };
    };
    

    class rs585s_device
        : public d500_active
        , public d500_color
        , public d500_safety
        , public d500_depth_mapping
        , public d500_motion
        , public ds_advanced_mode_base
        , public extended_firmware_logger_device
    {
    public:
        rs585s_device( std::shared_ptr< const d500_info > const & dev_info )
            : device( dev_info )
            , backend_device( dev_info )
            , d500_device( dev_info )
            , d500_active( dev_info )
            , d500_color( dev_info, RS2_FORMAT_M420 )
            , d500_safety( dev_info )
            , d500_depth_mapping( dev_info )
            , d500_motion( dev_info )
            , ds_advanced_mode_base()
            , extended_firmware_logger_device( dev_info, d500_device::_hw_monitor, get_firmware_logs_command() )
        {
            ds_advanced_mode_base::initialize_advanced_mode( this );
            set_advanced_mode_device( this );

            std::map< int, std::string > versions;
            versions[0] = get_info( RS2_CAMERA_INFO_FIRMWARE_VERSION );
            versions[1] = get_info( RS2_CAMERA_INFO_SMCU_FW_VERSION );
            set_expected_source_versions( std::move( versions ) );

            auto emitter_always_on_opt = std::make_shared<emitter_always_on_option>( d500_device::_hw_monitor, ds::APM_STROBE_GET, ds::APM_STROBE_SET );
            get_depth_sensor().register_option( RS2_OPTION_EMITTER_ALWAYS_ON, emitter_always_on_opt );

            // Note - requirement to gate depth options was removed to allow validation checks. Gated by FW only.
            // This should be last as we wish to protect the depth options setting when not in service safety mode
            // d500_safety::gate_depth_options();
        }

        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override
        {
            std::vector< std::shared_ptr< stream_interface > > streams = { _depth_stream, _left_ir_stream, _right_ir_stream, _color_stream,
                                                                           _safety_stream, _occupancy_stream, _point_cloud_stream,
                                                                           _ds_motion_common->get_accel_stream(),
                                                                           _ds_motion_common->get_gyro_stream() };
            return create_default_matcher( streams );
        }

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;

            tags.push_back( { RS2_STREAM_COLOR, -1, 1280, 720, RS2_FORMAT_RGB8, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT } );
            tags.push_back( { RS2_STREAM_DEPTH, -1, 1280, 720, RS2_FORMAT_Z16, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT } );
            tags.push_back( { RS2_STREAM_INFRARED, -1, 1280, 720, RS2_FORMAT_Y8, 30, profile_tag::PROFILE_TAG_SUPERSET } );
            tags.push_back( { RS2_STREAM_GYRO, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_200, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT } );
            tags.push_back( { RS2_STREAM_ACCEL, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_100, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT } );
            tags.push_back( { RS2_STREAM_OCCUPANCY, -1, 256, 320, RS2_FORMAT_Y8, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT } );

            return tags;
        };
    };
    

    class rs555_device
        : public d500_active
        //, public d500_color
        , public d500_motion
        , public d500_dual_rgb
        , public d500_object_detection
        , public ds_advanced_mode_base
        , public extended_firmware_logger_device
        , public eth_config_device
    {
    public:
        rs555_device( std::shared_ptr< const d500_info > dev_info )
            : device( dev_info )
            , backend_device( dev_info )
            , d500_device( dev_info )
            , d500_active( dev_info )
            //, d500_color( dev_info, RS2_FORMAT_YUYV )
            , d500_motion( dev_info )
            , d500_dual_rgb( dev_info )
            , d500_object_detection( dev_info )
            , ds_advanced_mode_base()
            , extended_firmware_logger_device( dev_info, d500_device::_hw_monitor, get_firmware_logs_command() )
        {
            eth_config_device::init( static_cast< debug_interface * >( this ) );
            ds_advanced_mode_base::initialize_advanced_mode( this );

            auto & depth_sensor = get_depth_sensor();
            group_multiple_fw_calls(depth_sensor, [&]()
            {
                auto emitter_always_on_opt = std::make_shared<emitter_always_on_option>( d500_device::_hw_monitor,
                                                                                         ds::LASERONCONST, ds::LASERONCONST);
                depth_sensor.register_option( RS2_OPTION_EMITTER_ALWAYS_ON, emitter_always_on_opt );

                auto thermal_compensation_toggle = std::make_shared< d500_thermal_compensation_option >( d500_device::_hw_monitor );

                // Monitoring SOC PVT (not OHM) because it correlates to D400 ASIC temperature and we keep the model the same.
                auto temperature_sensor = depth_sensor.get_option_handler( RS2_OPTION_SOC_PVT_TEMPERATURE );

                _thermal_monitor = std::make_shared< ds_thermal_monitor >( temperature_sensor, thermal_compensation_toggle );

                depth_sensor.register_option( RS2_OPTION_THERMAL_COMPENSATION,
                                              std::make_shared< thermal_compensation >( _thermal_monitor, thermal_compensation_toggle ) );
            } );  // group_multiple_fw_calls

            // Improved Close Range Depth - D555 only, USB toggle gated on FW support.
            if( d500_device::_fw_version >= firmware_version( "7.58.39807.10573" ) )
            {
                register_feature( std::make_shared< close_range_filter_feature >(
                    dynamic_cast< d500_depth_sensor & >( depth_sensor ) ) );
            }
        }

        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override
        {

            std::vector< std::shared_ptr< stream_interface > > streams = { _depth_stream, _left_ir_stream, _right_ir_stream, //_color_stream,
                                                                           _color_stream_1,
                                                                           _color_stream_2,
                                                                           _ds_motion_common->get_accel_stream(),
                                                                           _ds_motion_common->get_gyro_stream(),
                                                                           _object_detection_stream };
            return create_default_matcher( streams );
        }

        std::vector< tagged_profile > get_profiles_tags() const override
        {
            std::vector< tagged_profile > tags;

            //tags.push_back( { RS2_STREAM_COLOR, -1, 896, 504, RS2_FORMAT_RGB8, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT } );
            // Dual-RGB M420 color streams default to RGB8 @ 1280x720 (a resolution both color pins offer; the 896x504
            // default above only covers a dedicated color sensor / the full-resolution color pin).
            tags.push_back( { RS2_STREAM_COLOR, -1, 1280, 720, RS2_FORMAT_RGB8, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT } );
            tags.push_back( { RS2_STREAM_DEPTH, -1, 896, 504, RS2_FORMAT_Z16, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT } );
            tags.push_back( { RS2_STREAM_INFRARED, -1, 896, 504, RS2_FORMAT_Y8, 30, profile_tag::PROFILE_TAG_SUPERSET } );
            tags.push_back( { RS2_STREAM_GYRO, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_200, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT } );
            tags.push_back( { RS2_STREAM_ACCEL, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_100, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT } );
            tags.push_back({ RS2_STREAM_OBJECT_DETECTION, -1, -1, -1, RS2_FORMAT_Y8, -1, profile_tag::PROFILE_TAG_SUPERSET });

            return tags;
        };
    };


    std::shared_ptr< device_interface > d500_info::create_device()
    {
        using namespace ds;

        if( _group.uvc_devices.empty() )
            throw std::runtime_error("Depth Camera not found!");

        auto dev_info = std::dynamic_pointer_cast< const d500_info >( shared_from_this() );

        auto pid = _group.uvc_devices.front().pid;

        try
        {
            switch( pid )
            {
            case ds::D555_PID:
                return std::make_shared< rs555_device >( dev_info );
            case ds::D585_LEGACY_PID:
                return std::make_shared< rs585_legacy_device >( dev_info );
            case ds::D585S_PID:
                return std::make_shared< rs585s_device >( dev_info );
            case ds::D535_2C_PID:
            case ds::D585_2C_PID:
            case ds::D585_2C_PROTO_PID:
                return std::make_shared< rs5x5_device >( dev_info );
            case ds::D535_3C_PID:
            case ds::D535F_PID:
            case ds::D585_3C_PID:
            case ds::D585F_PID:
            case ds::D585_3C_PROTO_PID:
                return std::make_shared< rs5x5_dedicated_color_device >( dev_info );
            default:
                throw std::runtime_error( rsutils::string::from() << "unsupported D500 PID 0x" << hexdump( pid ) );
            }
        }
        catch( const std::exception & e )
        {
            // Create a device with partial capabilities instead of failing,
            // but only if the caller opted in via `partial-device-allowed`.
            if( ! ds::is_partial_device_allowed( get_context() ) )
            {
                LOG_ERROR( rsutils::string::from() << "Failed to create device for PID 0x" << std::hex << std::setw( 4 )
                                                   << std::setfill( '0' ) << (int)pid << "! (" << e.what() << ")" );
                throw;
            }
            LOG_WARNING( "PID 0x" << std::hex << std::setw( 4 ) << std::setfill( '0' ) << (int)pid
                                  << " - falling back to partial device (partial-device-allowed=true): " << e.what() );
            return std::make_shared< rs500_device >( dev_info );
        }
    }

    std::vector< std::shared_ptr< d500_info > > d500_info::pick_d500_devices( std::shared_ptr< context > ctx,
                                                                              platform::backend_device_group & group )
    {
        std::vector< platform::uvc_device_info > chosen;
        std::vector< std::shared_ptr< d500_info > > results;

        auto valid_pid = filter_by_product( group.uvc_devices, ds::rs500_sku_pid );
        auto group_devices = group_devices_and_hids_by_unique_id( group_devices_by_unique_id( valid_pid ), group.hid_devices );

        for( auto & g : group_devices )
        {
            auto & devices = g.first;
            auto & hids = g.second;

            bool is_mi_0_present = mi_present( devices, 0 );

            if( ! devices.empty() && is_mi_0_present )
            {
                platform::usb_device_info hwm;

                std::vector< platform::usb_device_info > hwm_devices;
                if( ds::d500_try_fetch_usb_device( group.usb_devices, devices.front(), hwm ) )
                {
                    hwm_devices.push_back( hwm );
                }

                auto info = std::make_shared< d500_info >( ctx,
                                                           std::move( devices ),
                                                           std::move( hwm_devices ),
                                                           std::move( hids ) );
                chosen.insert( chosen.end(), devices.begin(), devices.end() );
                results.push_back( info );
            }
        }

        trim_device_list( group.uvc_devices, chosen );

        return results;
    }
}
