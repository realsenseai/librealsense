// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022-2024 RealSense, Inc. All Rights Reserved.

#include "d500-safety.h"
#include "d500-types/safety-preset.h"
#include "d500-types/safety-interface-config.h"
#include "d500-types/application-config.h"

#include <vector>
#include <map>
#include <cstddef>

#include "ds/ds-timestamp.h"
#include "ds/ds-options.h"
#include "d500-options.h"
#include "d500-info.h"
#include "d585s-md.h"
#include "stream.h"
#include "backend.h"
#include "platform/platform-utils.h"
#include <src/metadata-parser.h>
#include <src/core/advanced_mode.h>
#include <rsutils/type/fourcc.h>
using rs_fourcc = rsutils::type::fourcc;

namespace librealsense
{
    const std::map<uint32_t, rs2_format> safety_fourcc_to_rs2_format = {
        {rs_fourcc('G','R','E','Y'), RS2_FORMAT_Y8}
    };
    const std::map<uint32_t, rs2_stream> safety_fourcc_to_rs2_stream = {
        {rs_fourcc('G','R','E','Y'), RS2_STREAM_SAFETY}
    };

    d500_safety::d500_safety(std::shared_ptr< const d500_info > const & dev_info  )
        : device( dev_info ), d500_device( dev_info ),
        _safety_stream(new stream(RS2_STREAM_SAFETY))
    {
        using namespace ds;

        const uint32_t safety_stream_mi = 11;
        auto safety_devs_info = filter_by_mi( dev_info->get_group().uvc_devices, safety_stream_mi);
        
        if (safety_devs_info.size() != 1)
            throw invalid_value_exception(rsutils::string::from() << "RS5XX models with Safety are expected to include a single safety device! - "
                << safety_devs_info.size() << " found");

        auto safety_ep = create_safety_device( dev_info->get_context(), safety_devs_info );
        _safety_device_idx = add_sensor( safety_ep );
    }

    std::shared_ptr<synthetic_sensor> d500_safety::create_safety_device(std::shared_ptr<context> ctx,
        const std::vector<platform::uvc_device_info>& safety_devices_info)
    {
        using namespace ds;

        register_stream_to_extrinsic_group(*_safety_stream, 0);
        environment::get_instance().get_extrinsics_graph().register_same_extrinsics(*_depth_stream, *_safety_stream);

        std::unique_ptr< frame_timestamp_reader > ds_timestamp_reader_backup( new ds_timestamp_reader() );
        std::unique_ptr<frame_timestamp_reader> ds_timestamp_reader_metadata( new ds_timestamp_reader_from_metadata_safety(std::move(ds_timestamp_reader_backup)));

        auto enable_global_time_option = std::shared_ptr<global_time_option>(new global_time_option());

        auto raw_safety_ep = std::make_shared<uvc_sensor>("Raw Safety Device",
            get_backend()->create_uvc_device(safety_devices_info.front()),
            std::unique_ptr<frame_timestamp_reader>(new global_timestamp_reader(std::move(ds_timestamp_reader_metadata), _tf_keeper, enable_global_time_option)),
            this);

        raw_safety_ep->register_xu(safety_xu); // making sure the XU is initialized every time we power the camera

        auto safety_ep = std::make_shared<d500_safety_sensor>(this,
            raw_safety_ep,
            safety_fourcc_to_rs2_format,
            safety_fourcc_to_rs2_stream);

        safety_ep->register_option(RS2_OPTION_GLOBAL_TIME_ENABLED, enable_global_time_option);

        safety_ep->register_info(RS2_CAMERA_INFO_PHYSICAL_PORT, safety_devices_info.front().device_path);

        // register options
        register_options(safety_ep, raw_safety_ep);

        // register metadata
        register_metadata(raw_safety_ep);

        // register processing blocks
        register_processing_blocks(safety_ep);
        
        return safety_ep;
    }

    void d500_safety::set_advanced_mode_device( ds_advanced_mode_base * advanced_mode )
    {
        _advanced_mode = advanced_mode;
        block_advanced_mode_if_needed( _safety_camera_oper_mode->query() );
    }

    void d500_safety::block_advanced_mode_if_needed( float val )
    {
        // Note - requirement to block advanced mode was removed to allow validation checks. Gated by FW only.
        //if( ! _advanced_mode )
        //    throw std::runtime_error( "Advanced mode device not set" );
        //
        //if( val == static_cast< float >( RS2_SAFETY_MODE_SERVICE ) )
        //    _advanced_mode->unblock();
        //else
        //    _advanced_mode->block( std::string( "Option can be set only in safety service mode" ) );
    }

    void d500_safety::register_options(std::shared_ptr<d500_safety_sensor> safety_ep, std::shared_ptr<uvc_sensor> raw_safety_sensor)
    {
        // Register safety preset active index option
        static const std::chrono::milliseconds safety_preset_change_timeout( 1000 );
        auto active_safety_preset = std::make_shared<ensure_set_xu_option<uint16_t>>(
            raw_safety_sensor,
            safety_xu,
            xu_id::SAFETY_PRESET_ACTIVE_INDEX,
            "Safety Preset Active Index",
            safety_preset_change_timeout);

        safety_ep->register_option(RS2_OPTION_SAFETY_PRESET_ACTIVE_INDEX, active_safety_preset);
        
        // Register operational mode option
        static const std::chrono::milliseconds safety_mode_change_timeout( 2000 );
        auto safety_camera_oper_mode = std::make_shared< cascade_option< ensure_set_xu_option< uint16_t > > >(
            raw_safety_sensor,
            safety_xu,
            xu_id::SAFETY_CAMERA_OPER_MODE,
            "Safety camera operational mode",
            std::map< float, std::string >{ { float( RS2_SAFETY_MODE_RUN ),     "Run" },
                                            { float( RS2_SAFETY_MODE_STANDBY ), "Standby" },
                                            { float( RS2_SAFETY_MODE_SERVICE ), "Service" } },
            safety_mode_change_timeout );

        safety_camera_oper_mode->add_observer( [this]( float val ) { block_advanced_mode_if_needed( val ); } );
        _safety_camera_oper_mode = safety_camera_oper_mode;

        safety_ep->register_option( RS2_OPTION_SAFETY_MODE, safety_camera_oper_mode );

        safety_ep->register_option(RS2_OPTION_SAFETY_MCU_TEMPERATURE,
            std::make_shared<temperature_option>(_hw_monitor, temperature_option::temperature_component::SMCU, "Temperature reading for Safety MCU"));

    }

    void d500_safety::gate_depth_option( rs2_option opt,
                                         synthetic_sensor & depth_sensor,
                                         const std::vector < std::tuple< std::shared_ptr< option >, float, std::string > > & options_and_reasons )
    {
        // Replcaes the option registered in depth sensor with a `gated_by_value_option` that proxies the original
        // options and can block setting it if conditions are not met ( safety not in service mode ).
        depth_sensor.register_option( opt,
                                      std::make_shared< gated_by_value_option >( depth_sensor.get_option_handler( opt ),
                                                                                 options_and_reasons ) );
    }

    void d500_safety::gate_depth_options()
    {
        auto& ds = get_depth_sensor();
        std::vector< std::tuple< std::shared_ptr< option >, float, std::string > > options_and_reasons
            = { std::make_tuple( _safety_camera_oper_mode,
                                 static_cast<float>(RS2_SAFETY_MODE_SERVICE),
                                 std::string("Option can be set only in safety service mode")) };

        gate_depth_option(RS2_OPTION_ENABLE_AUTO_EXPOSURE, ds, options_and_reasons);
        gate_depth_option(RS2_OPTION_EXPOSURE, ds, options_and_reasons);
        gate_depth_option(RS2_OPTION_GAIN, ds, options_and_reasons);
        gate_depth_option(RS2_OPTION_EMITTER_ALWAYS_ON, ds, options_and_reasons);
        gate_depth_option(RS2_OPTION_INTER_CAM_SYNC_MODE, ds, options_and_reasons);
        gate_depth_option(RS2_OPTION_DEPTH_UNITS, ds, options_and_reasons);
        gate_depth_option(RS2_OPTION_EMITTER_ENABLED, ds, options_and_reasons);
        gate_depth_option(RS2_OPTION_LASER_POWER, ds, options_and_reasons);
        gate_depth_option(RS2_OPTION_VISUAL_PRESET, ds, options_and_reasons);
    }

    void d500_safety::register_metadata(std::shared_ptr<uvc_sensor> raw_safety_ep)
    {
        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_FRAME_TIMESTAMP, 
            make_uvc_header_parser(&platform::uvc_header::timestamp));

        // attributes of md_safety_mode
        auto md_prop_offset = metadata_raw_mode_offset +
            offsetof(md_safety_mode, intel_safety_info);

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_FRAME_COUNTER, 
            make_attribute_parser(&md_safety_info::frame_counter, 
                md_safety_info_attributes::frame_counter_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_DEPTH_FRAME_COUNTER,
            make_attribute_parser(&md_safety_info::depth_frame_counter, 
                md_safety_info_attributes::depth_frame_counter_attribute, md_prop_offset));
        
        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SENSOR_TIMESTAMP,
            make_attribute_parser(&md_safety_info::frame_timestamp, 
                md_safety_info_attributes::frame_timestamp_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_LEVEL1,
            make_attribute_parser(&md_safety_info::level1_signal, 
                md_safety_info_attributes::level1_signal_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_LEVEL1_ORIGIN,
            make_attribute_parser(&md_safety_info::level1_frame_counter_origin, 
                md_safety_info_attributes::level1_frame_counter_origin_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_LEVEL2,
            make_attribute_parser(&md_safety_info::level2_signal, 
                md_safety_info_attributes::level2_signal_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_LEVEL2_ORIGIN,
            make_attribute_parser(&md_safety_info::level2_frame_counter_origin, 
                md_safety_info_attributes::level2_frame_counter_origin_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_LEVEL1_VERDICT,
            make_attribute_parser(&md_safety_info::level1_verdict, 
                md_safety_info_attributes::level1_verdict_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_LEVEL2_VERDICT,
            make_attribute_parser(&md_safety_info::level2_verdict, 
                md_safety_info_attributes::level2_verdict_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_OPERATIONAL_MODE,
            make_attribute_parser(&md_safety_info::operational_mode,
                md_safety_info_attributes::operational_mode_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_VISION_VERDICT,
            make_attribute_parser(&md_safety_info::vision_safety_verdict,
                md_safety_info_attributes::vision_safety_verdict_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_HARA_EVENTS,
            make_attribute_parser( &md_safety_info::vision_hara_status,
                md_safety_info_attributes::vision_hara_status_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_PRESET_INTEGRITY,
            make_attribute_parser(&md_safety_info::safety_preset_integrity,
                md_safety_info_attributes::safety_preset_integrity_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_PRESET_ID_SELECTED,
            make_attribute_parser(&md_safety_info::safety_preset_id_selected,
                md_safety_info_attributes::safety_preset_id_selected_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_PRESET_ID_USED,
            make_attribute_parser(&md_safety_info::safety_preset_id_used,
                md_safety_info_attributes::safety_preset_id_used_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_SIP_DEGRADATION_USED,
            make_attribute_parser(&md_safety_info::sip_degradation_diagnostics,
                md_safety_info_attributes::sip_metrics_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_SIP_GENERIC_METRICS_ACTIVATE,
            make_attribute_parser(&md_safety_info::sip_generic_metrics_activate,
                md_safety_info_attributes::sip_metrics_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_SIP_GENERIC_METRICS_STATE,
            make_attribute_parser(&md_safety_info::sip_generic_metrics_state,
                md_safety_info_attributes::sip_metrics_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_SIP_GENERIC_METRICS_VALUE1,
            make_attribute_parser(&md_safety_info::sip_generic_metrics_value1,
                md_safety_info_attributes::sip_metrics_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_SIP_GENERIC_METRICS_THRESHOLD1,
            make_attribute_parser(&md_safety_info::sip_generic_metrics_threshold1,
                md_safety_info_attributes::sip_metrics_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_SIP_GENERIC_METRICS_VALUE2,
            make_attribute_parser(&md_safety_info::sip_generic_metrics_value2,
                md_safety_info_attributes::sip_metrics_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_SIP_GENERIC_METRICS_THRESHOLD2,
            make_attribute_parser(&md_safety_info::sip_generic_metrics_threshold2,
                md_safety_info_attributes::sip_metrics_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_ZERO_MONITORING_ENABLED,
            make_always_enabled_param_parser(&md_safety_info::zero_safety_monitoring_enabled, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_HARA_HISTORY_MODE,
            make_always_enabled_param_parser(&md_safety_info::hara_history_mode, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_SOC_FUSA_EVENTS,
            make_attribute_parser(&md_safety_info::soc_fusa_events, 
                md_safety_info_attributes::soc_fusa_events_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_SOC_FUSA_ACTION,
            make_attribute_parser(&md_safety_info::soc_fusa_action, 
                md_safety_info_attributes::soc_fusa_action_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_SOC_L0_COUNTER,
            make_always_enabled_param_parser(&md_safety_info::soc_l_0_counter, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_SOC_L0_RATE,
            make_always_enabled_param_parser(&md_safety_info::soc_l_0_rate, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_SOC_L1_COUNTER,
            make_always_enabled_param_parser(&md_safety_info::soc_l_1_counter, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_SOC_L1_RATE,
            make_always_enabled_param_parser(&md_safety_info::soc_l_1_rate, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_SOC_GMT_STATUS,
            make_always_enabled_param_parser(&md_safety_info::soc_gmt_status, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_SOC_HKR_CRITICAL_ERROR_GPIO,
            make_always_enabled_param_parser(&md_safety_info::soc_hkr_critical_error_gpio, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_SOC_MONITOR_L2_ERROR_TYPE,
            make_always_enabled_param_parser(&md_safety_info::soc_monitor_l2_error_type, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_SOC_MONITOR_L3_ERROR_TYPE,
            make_always_enabled_param_parser(&md_safety_info::soc_monitor_l3_error_type, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_SOC_SAFETY_AND_SECURITY,
            make_always_enabled_param_parser(&md_safety_info::soc_safety_and_security, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_DEPTH_FRAME_TIMESTAMP,
            make_attribute_parser(&md_safety_info::depth_frame_timestamp,
                md_safety_info_attributes::frame_timestamp_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_SMCU_PROCESSING_TIMESTAMP,
            make_attribute_parser(&md_safety_info::smcu_processing_timestamp,
                md_safety_info_attributes::frame_timestamp_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_PIPELINE_PROPAGATION_DELAY,
            make_attribute_parser(&md_safety_info::safety_pipeline_propagation_delay,
                md_safety_info_attributes::timing_kpi_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_MB_FUSA_EVENT,
            make_attribute_parser(&md_safety_info::mb_fusa_event, 
                md_safety_info_attributes::mb_fusa_event_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_MB_FUSA_ACTION,
            make_attribute_parser(&md_safety_info::mb_fusa_action, 
                md_safety_info_attributes::mb_fusa_action_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_MB_STATUS,
            make_attribute_parser(&md_safety_info::mb_status,
                md_safety_info_attributes::mb_status_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_SMCU_LIVELINESS,
            make_attribute_parser(&md_safety_info::smcu_liveliness,
                md_safety_info_attributes::smcu_liveliness_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_SMCU_STATE,
            make_attribute_parser(&md_safety_info::smcu_state,
                md_safety_info_attributes::smcu_state_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_SMCU_DEBUG_STATUS_BITMASK,
            make_attribute_parser(&md_safety_info::smcu_status_bitmask,
                md_safety_info_attributes::smcu_debug_info_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_SMCU_DEBUG_INFO_INTERNAL_STATE,
            make_attribute_parser(&md_safety_info::smcu_internal_state,
                md_safety_info_attributes::smcu_debug_info_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_SMCU_DEBUG_INFO_BIST_STATUS,
            make_attribute_parser(&md_safety_info::smcu_bist_status,
                md_safety_info_attributes::smcu_debug_info_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_NON_FUSA_GPIO_OUT,
            make_attribute_parser(&md_safety_info::non_fusa_gpio_out,
                md_safety_info_attributes::non_fusa_gpio_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_SMCU_HW_MONITOR_STATUS,
            make_attribute_parser(&md_safety_info::smcu_hw_monitor_status,
                md_safety_info_attributes::smcu_hw_report_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_SMCU_SW_MONITOR_STATUS,
            make_attribute_parser(&md_safety_info::smcu_sw_monitor_status,
                md_safety_info_attributes::smcu_hw_report_attribute, md_prop_offset));

        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_NON_FUSA_GPIO_IN,
            make_attribute_parser(&md_safety_info::non_fusa_gpio_in,
                md_safety_info_attributes::non_fusa_gpio_attribute, md_prop_offset));

        // calc CRC for safety MD payload, starting from "version" field (not including the uvc and md headers),
        // and without the crc field itself.
        raw_safety_ep->register_metadata(RS2_FRAME_METADATA_CRC,
            make_attribute_parser_with_crc(&md_safety_info::crc32,
                md_safety_info_attributes::crc32_attribute, md_prop_offset, offsetof(md_safety_info, version), offsetof(md_safety_info,crc32)));
    }

    void d500_safety::register_processing_blocks(std::shared_ptr<d500_safety_sensor> safety_ep)
    {
        safety_ep->register_processing_block(processing_block_factory::create_id_pbf(RS2_FORMAT_Y8, RS2_STREAM_SAFETY));
    }


    stream_profiles d500_safety_sensor::init_stream_profiles()
    {
        auto lock = environment::get_instance().get_extrinsics_graph().lock();
        auto results = synthetic_sensor::init_stream_profiles();

        for (auto p : results)
        {
            // Register stream types
            if (p->get_stream_type() == RS2_STREAM_SAFETY)
                assign_stream(_owner->_safety_stream, p);

            auto&& video = dynamic_cast<video_stream_profile_interface*>(p.get());
            const auto&& profile = to_profile(p.get());

            std::weak_ptr<d500_safety_sensor> wp =
                std::dynamic_pointer_cast<d500_safety_sensor>(this->shared_from_this());
            video->set_intrinsics([profile, wp]()
                {
                    auto sp = wp.lock();
                    if (sp)
                        return sp->get_intrinsics(profile);
                    else
                        return rs2_intrinsics{};
                });
        }

        return results;
    }

    rs2_intrinsics d500_safety_sensor::get_intrinsics(const stream_profile& profile) const
    {
        return rs2_intrinsics();
    }

    std::string d500_safety_sensor::get_safety_preset(int index) const
    {
        safety_preset_with_header* result;

        // prepare command
        command cmd(ds::SAFETY_PRESET_READ);
        cmd.require_response = true;
        cmd.param1 = index;

        // send command to device and get response (safety_preset entry + safety_preset_header)
        std::vector< uint8_t > response = _owner->_hw_monitor->send(cmd);
        if (response.size() < sizeof(safety_preset_with_header))
        {
            throw io_exception(rsutils::string::from() << "Safety preset read at index=" << index << " failed");
        }

        // check CRC before returning result       
        auto computed_crc32 = rsutils::number::calc_crc32(response.data() + sizeof(safety_preset_header),
            sizeof(safety_preset));
        result = reinterpret_cast<safety_preset_with_header*>(response.data());
        if (computed_crc32 != result->get_table_header().get_crc32())
        {
            throw invalid_value_exception(rsutils::string::from() << "Safety preset invalid CRC value");
        }

        rsutils::json j = result->get_safety_preset().to_json();
        return j.dump();
    }

    void d500_safety_sensor::set_safety_preset(int index, const std::string& sp_json_str) const
    {
        rsutils::json json_data = rsutils::json::parse(sp_json_str);
        safety_preset sp(json_data["safety_preset"]);

        //calculate CRC
        auto computed_crc32 = rsutils::number::calc_crc32(reinterpret_cast<const uint8_t*>(&sp), sizeof(safety_preset));

        // prepare vector of data to be sent (header + sp)
        uint16_t version = ((uint16_t)0x03 << 8) | 0x02;  // major=0x03, minor=0x02 --> ver = major.minor
        uint32_t calib_version = 0;  // ignoring this field, as requested by sw architect
        safety_preset_header header(version, static_cast<uint16_t>(ds::d500_calibration_table_id::safety_preset_id), sizeof(safety_preset_with_header), computed_crc32);
        safety_preset_with_header sp_with_header(header, sp);
        auto data_as_ptr = reinterpret_cast<const uint8_t*>(&sp_with_header);

        // prepare command
        command cmd(ds::SAFETY_PRESET_WRITE);
        cmd.param1 = index;
        cmd.data.insert(cmd.data.end(), data_as_ptr, data_as_ptr + sizeof(safety_preset_with_header));
        cmd.require_response = true;

        // send command 
        _owner->_hw_monitor->send(cmd);
    }

    std::string d500_safety_sensor::get_safety_interface_config(rs2_calib_location loc) const
    {
        if (loc != RS2_CALIB_LOCATION_FLASH && loc != RS2_CALIB_LOCATION_RAM)
            throw io_exception(rsutils::string::from() << "Safety Interface Config can be read only from Flash or RAM");
        ds::d500_calib_location d500_loc = (loc == RS2_CALIB_LOCATION_RAM) ? ds::d500_calib_location::d500_calib_ram_memory :
            ds::d500_calib_location::d500_calib_flash_memory;

        safety_interface_config_with_header* result;

        // prepare command
        command cmd(ds::GET_HKR_CONFIG_TABLE,
            static_cast<int>(d500_loc),
            static_cast<int>(ds::d500_calibration_table_id::safety_interface_cfg_id),
            static_cast<int>(ds::d500_calib_type::d500_calib_gold));
        cmd.require_response = true;

        // send command to device and get response (safety_interface_config entry + header)
        std::vector< uint8_t > response = _owner->_hw_monitor->send(cmd);
        if (response.size() < sizeof(safety_interface_config_with_header))
        {
            throw io_exception(rsutils::string::from() << "Safety Interface Config failed");
        }

        // check CRC before returning result       
        auto computed_crc32 = rsutils::number::calc_crc32(response.data() + sizeof(table_header), sizeof(safety_interface_config));
        result = reinterpret_cast<safety_interface_config_with_header*>(response.data());
        if (computed_crc32 != result->get_table_header().get_crc32())
        {
            throw invalid_value_exception(rsutils::string::from() << "Safety Interface Config invalid CRC value");
        }
        
        rsutils::json j = result->get_safety_interface_config().to_json();
        return j.dump();
    }

    void d500_safety_sensor::set_safety_interface_config(const std::string& sic_json_str) const
    {
        rsutils::json json_data = rsutils::json::parse(sic_json_str);
        safety_interface_config sic(json_data["safety_interface_config"]);

        // calculate CRC
        uint32_t computed_crc32 = rsutils::number::calc_crc32(reinterpret_cast<const uint8_t*>(&sic), sizeof(safety_interface_config));

        // prepare vector of data to be sent (header + safety_interface_config)
        uint16_t version = ((uint16_t)0x04 << 8) | 0x00;  // major=0x04, minor=0x00 --> ver = major.minor
        uint32_t calib_version = 0;  // ignoring this field, as requested by sw architect
        table_header header(version, static_cast<uint16_t>(ds::d500_calibration_table_id::safety_interface_cfg_id), sizeof(safety_interface_config), calib_version, computed_crc32);
        safety_interface_config_with_header sic_with_header(header, sic);
        auto data_as_ptr = reinterpret_cast<const uint8_t*>(&sic_with_header);

        // prepare command
        command cmd(ds::SET_HKR_CONFIG_TABLE,
            static_cast<int>(ds::d500_calib_location::d500_calib_flash_memory),
            static_cast<int>(ds::d500_calibration_table_id::safety_interface_cfg_id),
            static_cast<int>(ds::d500_calib_type::d500_calib_gold));
        cmd.data.insert(cmd.data.end(), data_as_ptr, data_as_ptr + sizeof(safety_interface_config_with_header));
        cmd.require_response = true;

        // send command 
        _owner->_hw_monitor->send(cmd);
    }

    std::string d500_safety_sensor::get_application_config() const
    { 
        application_config_with_header* result;

        // prepare command
        command cmd(ds::GET_HKR_CONFIG_TABLE,
            static_cast<int>(ds::d500_calib_location::d500_calib_flash_memory),
            static_cast<int>(ds::d500_calibration_table_id::app_config_table_id),
            static_cast<int>(ds::d500_calib_type::d500_calib_dynamic));
        cmd.require_response = true;

        // send command to device and get response (safety_interface_config entry + header)
        std::vector< uint8_t > response = _owner->_hw_monitor->send(cmd);
        if (response.size() < sizeof(application_config_with_header))
        {
            throw io_exception(rsutils::string::from() << "Applicaion Config Read Failed");
        }

        // check CRC before returning result
        auto computed_crc32 = rsutils::number::calc_crc32(response.data() + sizeof(table_header), sizeof(application_config));
        result = reinterpret_cast<application_config_with_header*>(response.data());
        if (computed_crc32 != result->get_table_header().get_crc32())
        {
            throw invalid_value_exception(rsutils::string::from() << "Applicaion Config Invalid CRC Value");
        }

        rsutils::json j = result->get_application_config().to_json();
        return j.dump();
    }

    void d500_safety_sensor::set_application_config(const std::string& application_config_json_str) const
    {
        rsutils::json json_data = rsutils::json::parse(application_config_json_str);
        application_config app_config(json_data["application_config"]);

        // calculate CRC
        uint32_t computed_crc32 = rsutils::number::calc_crc32(reinterpret_cast<const uint8_t*>(&app_config), sizeof(application_config));

        // prepare vector of data to be sent (header + application_config)
        uint16_t version = ((uint16_t)0x01 << 8) | 0x02;  // major=0x01, minor=0x02 --> ver = major.minor
        uint32_t calib_version = 0;  // ignoring this field, as requested by sw architect
        table_header header(version, static_cast<uint16_t>(ds::d500_calibration_table_id::app_config_table_id), sizeof(application_config), calib_version, computed_crc32);
        application_config_with_header app_config_with_header(header, app_config);
        auto data_as_ptr = reinterpret_cast<const uint8_t*>(&app_config_with_header);

        // prepare command
        command cmd(ds::SET_HKR_CONFIG_TABLE,
            static_cast<int>(ds::d500_calib_location::d500_calib_flash_memory),
            static_cast<int>(ds::d500_calibration_table_id::app_config_table_id),
            static_cast<int>(ds::d500_calib_type::d500_calib_dynamic));
        cmd.data.insert(cmd.data.end(), data_as_ptr, data_as_ptr + sizeof(application_config_with_header));
        cmd.require_response = true;

        // send command 
        _owner->_hw_monitor->send(cmd);
    }
}
