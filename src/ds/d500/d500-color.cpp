// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022-4 RealSense, Inc. All Rights Reserved.

#include <cstddef>
#include "metadata.h"

#include <src/ds/ds-timestamp.h>
#include "proc/color-formats-converter.h"
#include "d500-options.h"
#include "d500-color.h"
#include "d500-info.h"
#include "backend.h"
#include "platform/platform-utils.h"
#include <src/metadata-parser.h>
#include <src/ds/ds-thermal-monitor.h>

#include <src/ds/features/auto-exposure-roi-feature.h>

#include <rsutils/type/fourcc.h>
using rsutils::type::fourcc;

namespace librealsense
{
    std::map<fourcc::value_type, rs2_format> d500_color_fourcc_to_rs2_format = {
         {fourcc('Y','U','Y','2'), RS2_FORMAT_YUYV},
         {fourcc('Y','U','Y','V'), RS2_FORMAT_YUYV},
         {fourcc('U','Y','V','Y'), RS2_FORMAT_UYVY},
         {fourcc('M','J','P','G'), RS2_FORMAT_MJPEG},
         {fourcc('R','W','1','6'), RS2_FORMAT_RAW16},
         {fourcc('B','Y','R','2'), RS2_FORMAT_RAW16},
         {fourcc('M','4','2','0'), RS2_FORMAT_M420}
    };
    std::map<fourcc::value_type, rs2_stream> d500_color_fourcc_to_rs2_stream = {
        {fourcc('Y','U','Y','2'), RS2_STREAM_COLOR},
        {fourcc('Y','U','Y','V'), RS2_STREAM_COLOR},
        {fourcc('U','Y','V','Y'), RS2_STREAM_COLOR},
        {fourcc('R','W','1','6'), RS2_STREAM_COLOR},
        {fourcc('B','Y','R','2'), RS2_STREAM_COLOR},
        {fourcc('M','J','P','G'), RS2_STREAM_COLOR},
        {fourcc('M','4','2','0'), RS2_STREAM_COLOR}
    };

    d500_color::d500_color( std::shared_ptr< const d500_info > const & dev_info, rs2_format native_format )
        : d500_device( dev_info )
        , device( dev_info )
        , _color_stream( new stream( RS2_STREAM_COLOR ) )
        , _separate_color( true )
        , _native_format( native_format )
    {
        try
        {
            create_color_device( dev_info->get_context(), dev_info->get_group() );
            init();
        }
        catch (...)
        {
            LOG_ERROR("Color Sensor Failure!");
        }
    }

    void d500_color::create_color_device(std::shared_ptr<context> ctx, const platform::backend_device_group& group)
    {
        using namespace ds;

        _color_calib_table_raw = [this]()
        {
            return get_d500_raw_calibration_table(d500_calibration_table_id::rgb_calibration_id);
        };

        _color_extrinsic = std::make_shared< rsutils::lazy< rs2_extrinsics > >(
            [this]() { return from_pose( get_d500_color_stream_extrinsic( *_color_calib_table_raw ) ); } );
        environment::get_instance().get_extrinsics_graph().register_extrinsics(*_color_stream, *_depth_stream, _color_extrinsic);
        register_stream_to_extrinsic_group(*_color_stream, 0);

        std::vector<platform::uvc_device_info> color_devs_info = filter_by_mi(group.uvc_devices, 3);

        if ( color_devs_info.empty() )
        {
            throw backend_exception("cannot access color sensor", RS2_EXCEPTION_TYPE_BACKEND);
        }

        std::unique_ptr< frame_timestamp_reader > ds_timestamp_reader_backup( new ds_timestamp_reader() );
        std::unique_ptr<frame_timestamp_reader> ds_timestamp_reader_metadata(new ds_timestamp_reader_from_metadata(std::move(ds_timestamp_reader_backup)));

        auto enable_global_time_option = std::shared_ptr<global_time_option>(new global_time_option());
        auto raw_color_ep = std::make_shared< uvc_sensor >(
            "Raw RGB Camera",
            get_backend()->create_uvc_device( color_devs_info.front() ),
            std::unique_ptr< frame_timestamp_reader >(
                new global_timestamp_reader( std::move( ds_timestamp_reader_metadata ),
                                             _tf_keeper,
                                             enable_global_time_option ) ),
            this );

        auto color_ep = std::make_shared<d500_color_sensor>(this,
            raw_color_ep,
            d500_color_fourcc_to_rs2_format,
            d500_color_fourcc_to_rs2_stream);

        color_ep->register_option(RS2_OPTION_GLOBAL_TIME_ENABLED, enable_global_time_option);

        color_ep->register_info(RS2_CAMERA_INFO_PHYSICAL_PORT, color_devs_info.front().device_path);

        _color_device_idx = add_sensor(color_ep);
    }

    void d500_color::register_color_features()
    {
        register_feature( std::make_shared< auto_exposure_roi_feature >( get_color_sensor(), _hw_monitor, true ) );
    }

    void d500_color::init()
    {
        auto& color_ep = get_color_sensor();
        auto raw_color_ep = get_raw_color_sensor();

        _ds_color_common = std::make_shared<ds_color_common>(raw_color_ep, color_ep, _fw_version, _hw_monitor, this);

        register_color_features();
        register_options();
        register_metadata();
        register_color_processing_blocks();

        d500_auto_calibrated::add_color_write_observer( [this]() {  _color_calib_table_raw.reset(); } );
    }

    void d500_color::register_color_processing_blocks()
    {
        auto & color_ep = get_color_sensor();

        switch( _native_format )
        {
        case RS2_FORMAT_YUYV:
            color_ep.register_processing_block( processing_block_factory::create_pbf_vector< yuy2_converter >(
                RS2_FORMAT_YUYV,
                map_supported_color_formats( RS2_FORMAT_YUYV ),
                RS2_STREAM_COLOR ) );
            break;
        case RS2_FORMAT_M420:
            color_ep.register_processing_block( processing_block_factory::create_pbf_vector< m420_converter >(
                RS2_FORMAT_M420,
                map_supported_color_formats( RS2_FORMAT_M420 ),
                RS2_STREAM_COLOR ) );
            break;
        default:
            throw invalid_value_exception( "invalid native color format "
                                           + std::string( get_string( _native_format ) ) );
        }
        color_ep.register_processing_block(  // Color Raw (Bayer 10-bit embedded in 16-bit) for calibration
            processing_block_factory::create_id_pbf( RS2_FORMAT_RAW16, RS2_STREAM_COLOR ) );
    }

    void d500_color::register_options()
    {
        auto& color_ep = get_color_sensor();
        auto raw_color_ep = get_raw_color_sensor();

        _ds_color_common->register_color_options();

        std::map< float, std::string > description_per_value = std::map<float, std::string>{ 
            { 0.f, "Disabled"},
            { 1.f, "50Hz" },
            { 2.f, "60Hz" } };

        color_ep.register_option(RS2_OPTION_POWER_LINE_FREQUENCY,
            std::make_shared<power_line_freq_option>(raw_color_ep, RS2_OPTION_POWER_LINE_FREQUENCY,
                description_per_value));

        color_ep.register_pu(RS2_OPTION_AUTO_EXPOSURE_PRIORITY);

        _ds_color_common->register_standard_options();

        color_ep.register_pu(RS2_OPTION_HUE);

        if( _thermal_monitor )
            _thermal_monitor->add_observer( [&]( float ) { _color_calib_table_raw.reset(); } );
    }

    void d500_color::register_metadata()
    {
        auto& color_ep = get_color_sensor();

        auto md_prop_offset = metadata_raw_mode_offset +
            offsetof(md_rgb_mode, rgb_mode) +
            offsetof(md_rgb_normal_mode, intel_rgb_control);

        color_ep.register_metadata(RS2_FRAME_METADATA_AUTO_EXPOSURE, make_attribute_parser(&md_rgb_control::ae_mode, md_rgb_control_attributes::ae_mode_attribute, md_prop_offset,
            [](rs2_metadata_type param) { return (param != 1); })); // OFF value via UVC is 1 (ON is 8)

        md_prop_offset = metadata_raw_mode_offset +
            offsetof(md_rgb_mode, rgb_mode) +
            offsetof(md_rgb_normal_mode, intel_capture_stats);

        color_ep.register_metadata(RS2_FRAME_METADATA_FRAME_TIMESTAMP, make_attribute_parser(&md_capture_stats::hw_timestamp, md_capture_stat_attributes::hw_timestamp_attribute, md_prop_offset));

        md_prop_offset = metadata_raw_mode_offset +
            offsetof(md_rgb_mode, rgb_mode) +
            offsetof(md_rgb_normal_mode, intel_capture_timing);

        // attributes of md_capture_stats
        auto md_prop_offset_stats = metadata_raw_mode_offset +
            offsetof(md_rgb_mode, rgb_mode) +
            offsetof(md_rgb_normal_mode, intel_capture_stats);

        color_ep.register_metadata(RS2_FRAME_METADATA_SENSOR_TIMESTAMP,
            make_rs400_sensor_ts_parser(make_attribute_parser(&md_capture_stats::hw_timestamp, md_capture_stat_attributes::hw_timestamp_attribute, md_prop_offset_stats),
                make_attribute_parser(&md_capture_timing::sensor_timestamp, md_capture_timing_attributes::sensor_timestamp_attribute, md_prop_offset)));

        _ds_color_common->register_metadata();
    }

    void d500_color::register_stream_to_extrinsic_group( const stream_interface & stream, uint32_t group_index )
    {
        device::register_stream_to_extrinsic_group( stream, group_index );
    }
    
    rs2_intrinsics d500_color_sensor::get_intrinsics(const stream_profile& profile) const
    {
        return get_d500_intrinsic_by_resolution(
            *_owner->_color_calib_table_raw,
            ds::d500_calibration_table_id::rgb_calibration_id,
            profile.width, profile.height);
    }

    stream_profiles d500_color_sensor::init_stream_profiles()
    {
        auto lock = environment::get_instance().get_extrinsics_graph().lock();
        auto&& results = synthetic_sensor::init_stream_profiles();

        for (auto&& p : results)
        {
            // Register stream types
            if (p->get_stream_type() == RS2_STREAM_COLOR)
            {
                assign_stream(_owner->_color_stream, p);
            }

            auto&& video = dynamic_cast<video_stream_profile_interface*>(p.get());
            const auto&& profile = to_profile(p.get());

            std::weak_ptr<d500_color_sensor> wp =
                std::dynamic_pointer_cast<d500_color_sensor>(this->shared_from_this());
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

    processing_blocks d500_color_sensor::get_recommended_processing_blocks() const
    {
        return get_color_recommended_proccesing_blocks();
    }
}
