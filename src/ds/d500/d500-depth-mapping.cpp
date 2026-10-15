// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#include "d500-depth-mapping.h"

#include "d500-safety.h"
#include "d500-info.h"
#include "d585s-md.h"
#include "mapping-timing.h"
#include "d500-types/safety-interface-config.h"

#include <algorithm>
#include <vector>
#include <map>
#include <cstddef>

#include "ds/ds-timestamp.h"
#include "ds/ds-options.h"
#include <src/backend.h>
#include <rsutils/type/fourcc.h>
using rs_fourcc = rsutils::type::fourcc;
#include "stream.h"

#include "platform/platform-utils.h"
#include "pose.h"   // identity_matrix

#include <src/metadata-parser.h>
#include <thread>

namespace librealsense
{
    // Use the same validated UVC capture identity as the Mapping timestamp reader.
    class mapping_capture_parser : public md_attribute_parser_base
    {
        rs2_frame_metadata_value const _attribute;
    public:
        explicit mapping_capture_parser( rs2_frame_metadata_value attribute ) : _attribute( attribute ) {}
        bool find( const frame & f, rs2_metadata_type * value ) const override
        {
            uint32_t counter = 0;
            uint64_t timestamp = 0;
            if( ! get_mapping_capture_timing( f, counter, timestamp ) )
                return false;
            if( _attribute == RS2_FRAME_METADATA_ACTUAL_FPS )
                return ds_md_attribute_actual_fps().find( f, value );
            if( value )
                *value = _attribute == RS2_FRAME_METADATA_SENSOR_TIMESTAMP ? timestamp : counter;
            return true;
        }
    };

    const std::map<uint32_t, rs2_format> mapping_fourcc_to_rs2_format = {
        {rs_fourcc('G','R','E','Y'), RS2_FORMAT_Y8},
        // point cloud - w/a done in backend in order to distinguish between occupancy
        // and labeled point cloud streams - PAL8 instead of GREY 
        // because both are received as GREY 
        {rs_fourcc('P','A','L','8'), RS2_FORMAT_Y8}
    };
    const std::map<uint32_t, rs2_stream> mapping_fourcc_to_rs2_stream = {
        {rs_fourcc('G','R','E','Y'), RS2_STREAM_OCCUPANCY},
        {rs_fourcc('P','A','L','8'), RS2_STREAM_LABELED_POINT_CLOUD}
    };

    d500_depth_mapping::d500_depth_mapping( std::shared_ptr< const d500_info > const & dev_info)
        : device( dev_info ), d500_device( dev_info ),
        _occupancy_stream(new stream(RS2_STREAM_OCCUPANCY)),
        _point_cloud_stream(new stream(RS2_STREAM_LABELED_POINT_CLOUD))
    {
        using namespace ds;

        const auto pid = dev_info->get_group().uvc_devices.front().pid;

        _is_safety_layout = ( pid == D585S_PID || pid == D585_LEGACY_PID );

        // Older FW needs running Depth for occupancy over GMSL, and produces none on GMSL 2C
        _standalone_occupancy = _fw_version >= firmware_version( "7.59.46596.16636" );
        if( _is_mipi_device && is_dual_rgb_pid( pid ) && ! _standalone_occupancy )
            return;

        const uint32_t mapping_stream_mi = _is_safety_layout ? 13 : 11;
        auto mapping_devs_info = filter_by_mi( dev_info->get_group().uvc_devices, mapping_stream_mi);

        // A missing interface most commonly means older FW that predates depth mapping.
        // Degrade gracefully (no occupancy/point-cloud streams) instead of failing the
        // whole device - some units in the field won't have this FW yet.
        if (mapping_devs_info.size() != 1)
        {
            LOG_WARNING( "depth mapping device not found (expected 1, found " << mapping_devs_info.size()
                << ") - occupancy/point-cloud streams will not be available" );
            return;
        }

        auto mapping_ep = create_depth_mapping_device( dev_info->get_context(), mapping_devs_info );
        _depth_mapping_idx = add_sensor(mapping_ep);
        _depth_mapping_active = true;
        if( ! _is_safety_layout )
            add_stream_combination_validator( [this]( const stream_profiles & requests )
                                              { depth_sensor_allowed_or_throw( requests ); } );
    }

    // The camera builds occupancy from depth at these sizes only, at the occupancy rate
    static bool is_occupancy_depth_source( const stream_profile_interface & p, uint32_t fps, bool fixed_source_only )
    {
        auto vp = dynamic_cast< const video_stream_profile_interface * >( &p );
        if( p.get_stream_type() != RS2_STREAM_DEPTH || ! vp || p.get_framerate() != fps )
            return false;
        bool const native = vp->get_width() == 640 && vp->get_height() == 360;
        return native || ( ! fixed_source_only && vp->get_width() == 1280 && vp->get_height() == 720 );
    }

    static bool has_stream( const stream_profiles & profiles, rs2_stream type )
    {
        return std::any_of( profiles.begin(), profiles.end(),
                            [type]( const std::shared_ptr< stream_profile_interface > & p ) { return p && p->get_stream_type() == type; } );
    }

    static std::shared_ptr< stream_profile_interface > find_stream( const stream_profiles & profiles, rs2_stream type )
    {
        auto it = std::find_if( profiles.begin(), profiles.end(),
                                [type]( const std::shared_ptr< stream_profile_interface > & p ) { return p && p->get_stream_type() == type; } );
        return it == profiles.end() ? nullptr : *it;
    }

    void d500_depth_mapping::occupancy_allowed_or_throw( const stream_profiles & requests ) const
    {
        auto const occupancy = find_stream( requests, RS2_STREAM_OCCUPANCY );
        auto const running = get_sensor( _depth_device_idx ).get_active_streams();
        if( _is_safety_layout || ! occupancy )
            return;
        uint32_t const fps = occupancy->get_framerate();
        if( running.empty() )
        {
            if( _is_mipi_device && ! _standalone_occupancy )
                throw wrong_api_call_sequence_exception( rsutils::string::from()
                    << "Occupancy needs Depth at 640x360 or 1280x720, " << fps
                    << " fps, streaming first; standalone occupancy needs FW 7.59.46596.16636 or newer" );
            return;
        }
        if( std::none_of( running.begin(), running.end(),
                          [fps]( const std::shared_ptr< stream_profile_interface > & p ) { return p && is_occupancy_depth_source( *p, fps, false ); } ) )
            throw wrong_api_call_sequence_exception( rsutils::string::from()
                << "Occupancy needs Depth at 640x360 or 1280x720, " << fps << " fps, while the depth sensor streams" );
        // Over USB the camera keeps the colors open but stops delivering them
        if( ! _is_mipi_device && has_stream( running, RS2_STREAM_COLOR ) )
            throw wrong_api_call_sequence_exception( "Occupancy cannot start while the depth sensor streams color" );
    }

    void d500_depth_mapping::depth_sensor_allowed_or_throw( const stream_profiles & requests ) const
    {
        auto const occupancy = _depth_mapping_idx < 0 ? nullptr
            : find_stream( get_sensor( _depth_mapping_idx ).get_active_streams(), RS2_STREAM_OCCUPANCY );
        if( ! occupancy )
            return;
        // Occupancy started first fixes its depth source at 640x360; colors and other streams cannot join it
        uint32_t const fps = occupancy->get_framerate();
        if( has_stream( requests, RS2_STREAM_DEPTH ) )
        {
            if( requests.size() != 1 || ! is_occupancy_depth_source( *requests.front(), fps, true ) )
                throw wrong_api_call_sequence_exception( rsutils::string::from()
                    << "While Occupancy streams, Depth can start only alone at 640x360, " << fps << " fps" );
        }
        else if( has_stream( requests, RS2_STREAM_COLOR ) )
            throw wrong_api_call_sequence_exception( "Color cannot start while Occupancy streams" );
    }

    std::shared_ptr<synthetic_sensor> d500_depth_mapping::create_depth_mapping_device(std::shared_ptr<context> ctx,
        const std::vector<platform::uvc_device_info>& occupancy_devices_info)
    {
        using namespace ds;

        std::unique_ptr<frame_timestamp_reader> ds_timestamp_reader_backup(new ds_timestamp_reader());
        std::unique_ptr<frame_timestamp_reader> ds_timestamp_reader_metadata(new ds_timestamp_reader_from_metadata_depth_mapping(std::move(ds_timestamp_reader_backup)));

        auto enable_global_time_option = std::shared_ptr<global_time_option>(new global_time_option());

        auto raw_mapping_ep = std::make_shared<uvc_sensor>("Raw Depth Mapping Device",
            get_backend()->create_uvc_device(occupancy_devices_info.front()),
            std::unique_ptr<frame_timestamp_reader>(new global_timestamp_reader(std::move(ds_timestamp_reader_metadata), _tf_keeper, enable_global_time_option)),
            this);

        auto mapping_ep = std::make_shared<d500_depth_mapping_sensor>(this,
            raw_mapping_ep,
            mapping_fourcc_to_rs2_format,
            mapping_fourcc_to_rs2_stream);

        mapping_ep->register_option(RS2_OPTION_GLOBAL_TIME_ENABLED, enable_global_time_option);

        mapping_ep->register_info(RS2_CAMERA_INFO_PHYSICAL_PORT, occupancy_devices_info.front().device_path);

        // register_extrinsics
        register_extrinsics();

        // register options
        register_options(mapping_ep, raw_mapping_ep);

        // register metadata
        register_metadata(raw_mapping_ep);

        // register processing blocks
        register_processing_blocks(mapping_ep);
        
        return mapping_ep;
    }

    void d500_depth_mapping::register_extrinsics()
    {
        using rsutils::json;
        // Lazy because it reads the safety interface config table over the HW monitor,
        // and the depth mapping device may be constructed before the rest of the device
        // is fully up (though it may not be the case in the device contructor's order, in ds500-factory)
        _depth_to_depth_mapping_extrinsics = std::make_shared< rsutils::lazy< rs2_extrinsics > > ( [this]()
            {
                // Non-safety D5xx emit mapping payloads in ROS map axes (+X forward, +Y left,
                // +Z up); consumers expect depth/optical axes (+X right, +Y down, +Z forward).
                // Report the fixed conversion rather than identity:
                //   x_ros = z_opt   y_ros = -x_opt   z_ros = -y_opt
                // stored column-major, depth -> mapping.
                if( ! _is_safety_layout )
                {
                    rs2_extrinsics axes = {};
                    const float depth_to_mapping[9] = { 0.f, -1.f,  0.f,     // column 1
                                                        0.f,  0.f, -1.f,     // column 2
                                                        1.f,  0.f,  0.f };   // column 3
                    std::memcpy( axes.rotation, depth_to_mapping, sizeof( depth_to_mapping ) );
                    // Translation would be the mount height, which lives in the same safety
                    // config we cannot read here, so the ground plane passes through the
                    // camera origin rather than below it.
                    return axes;
                }

                // Pull extrinsic from safety interface config (HKR 0.9 QS) via the shared
                // HW-monitor read - depth mapping doesn't require a d500_safety sibling.
                rs2_extrinsics res;
                json sic_json;
                try
                {
                    sic_json = json::parse(read_safety_interface_config(_hw_monitor));
                }
                catch (const std::exception& e)
                {
                    throw std::runtime_error(rsutils::string::from() << "Could not read safety interface config: " << e.what());
                }
                camera_position extrinsics_from_preset(sic_json["safety_interface_config"]["camera_position"]);
                auto rot = extrinsics_from_preset.get_rotation();
                auto trans = extrinsics_from_preset.get_translation();

                // converting row-major matrix to column-major
                float rotation_matrix[9] = { rot[0][0], rot[1][0], rot[2][0],
                                             rot[0][1], rot[1][1], rot[2][1],
                                             rot[0][2], rot[1][2], rot[2][2] };
                std::memcpy(res.rotation, &rotation_matrix, sizeof rotation_matrix);
                std::memcpy(res.translation, trans.data(), trans.size() * sizeof(float));
                return res;
            });

        register_stream_to_extrinsic_group(*_occupancy_stream, 0);
        environment::get_instance().get_extrinsics_graph().register_extrinsics(*_depth_stream, *_occupancy_stream, _depth_to_depth_mapping_extrinsics);

        register_stream_to_extrinsic_group(*_point_cloud_stream, 0);
        environment::get_instance().get_extrinsics_graph().register_extrinsics(*_depth_stream, *_point_cloud_stream, _depth_to_depth_mapping_extrinsics);
    }

    void d500_depth_mapping::add_streams_if_active( std::vector< std::shared_ptr< stream_interface > > & streams ) const
    {
        if( is_depth_mapping_active() )
        {
            streams.push_back( _occupancy_stream );
            streams.push_back( _point_cloud_stream );
        }
    }

    void d500_depth_mapping::add_profile_tag_if_active( std::vector< tagged_profile > & tags ) const
    {
        if( is_depth_mapping_active() && _is_safety_layout )
        {
            // The occupancy canvas is transposed on this layout.
            tags.push_back( { RS2_STREAM_OCCUPANCY, -1, 256, 320, RS2_FORMAT_Y8, 30,
                              profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT } );
        }
    }

    void d500_depth_mapping::register_options(std::shared_ptr<d500_depth_mapping_sensor> occupancy_ep, std::shared_ptr<uvc_sensor> raw_mapping_sensor)
    {

    }

    void d500_depth_mapping::register_metadata(std::shared_ptr<uvc_sensor> raw_mapping_ep)
    {
        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_FRAME_TIMESTAMP, 
            make_uvc_header_parser(&platform::uvc_header::timestamp));

        register_occupancy_metadata(raw_mapping_ep);
        register_point_cloud_metadata(raw_mapping_ep);

        std::shared_ptr< md_attribute_parser_base > counter_parser
            = std::make_shared< mapping_capture_parser >( RS2_FRAME_METADATA_FRAME_COUNTER );
        if( _is_mipi_device )
        {
            // GMSL counter validity is independent of unavailable source timing on 2C.
            auto const offset = metadata_raw_mode_offset + offsetof( md_mapping_mode, intel_occupancy );
            counter_parser = std::make_shared< md_layout_selecting_parser >(
                make_attribute_parser( &md_occupancy::frame_counter,
                                       md_occupancy_attributes::frame_counter_attribute, offset ),
                make_attribute_parser( &md_point_cloud::frame_counter,
                                       md_point_cloud_attributes::frame_counter_attribute, offset ) );
        }
        raw_mapping_ep->register_metadata( RS2_FRAME_METADATA_FRAME_COUNTER, counter_parser );
        raw_mapping_ep->register_metadata( RS2_FRAME_METADATA_SENSOR_TIMESTAMP,
            std::make_shared< mapping_capture_parser >( RS2_FRAME_METADATA_SENSOR_TIMESTAMP ) );
        raw_mapping_ep->register_metadata( RS2_FRAME_METADATA_ACTUAL_FPS,
            std::make_shared< mapping_capture_parser >( RS2_FRAME_METADATA_ACTUAL_FPS ) );
    }


    void d500_depth_mapping::register_occupancy_metadata(std::shared_ptr<uvc_sensor> raw_mapping_ep)
    {
        // attributes of md_occupancy
        auto md_prop_offset = metadata_raw_mode_offset +
            offsetof(md_mapping_mode, intel_occupancy);

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_DEPTH_FRAME_COUNTER,
            make_attribute_parser(&md_occupancy::depth_frame_counter,
                md_occupancy_attributes::depth_frame_counter_attribute, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_FLOOR_DETECTION,
            make_attribute_parser(&md_occupancy::floor_detection,
                md_occupancy_attributes::floor_detection_attribute, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_FILL_RATE,
            make_attribute_parser(&md_occupancy::diagnostic_zone_fill_rate,
                md_occupancy_attributes::diagnostic_zone_fill_rate_attribute, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DEPTH_FILL_RATE,
            make_attribute_parser(&md_occupancy::depth_fill_rate,
                md_occupancy_attributes::depth_fill_rate_attribute, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_SENSOR_ANGLE_ROLL,
            make_attribute_parser(&md_occupancy::sensor_roll_angle,
                md_occupancy_attributes::sensor_roll_angle_attribute, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_SENSOR_ANGLE_PITCH,
            make_attribute_parser(&md_occupancy::sensor_pitch_angle,
                md_occupancy_attributes::sensor_pitch_angle_attribute, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_MEDIAN_HEIGHT,
            make_attribute_parser(&md_occupancy::diagnostic_zone_median_height,
                md_occupancy_attributes::diagnostic_zone_median_height_attribute, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DEPTH_STDEV,
            make_attribute_parser(&md_occupancy::depth_stdev,
                md_occupancy_attributes::depth_stdev_attribute, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_PRESET_ID,
            make_attribute_parser(&md_occupancy::safety_preset_id,
                md_occupancy_attributes::safety_preset_info, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_PRESET_ERROR_TYPE,
            make_attribute_parser(&md_occupancy::safety_preset_error_type,
                md_occupancy_attributes::safety_preset_info, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_PRESET_ERROR_PARAM_1,
            make_attribute_parser(&md_occupancy::safety_preset_error_param_1,
                md_occupancy_attributes::safety_preset_info, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_PRESET_ERROR_PARAM_2,
            make_attribute_parser(&md_occupancy::safety_preset_error_param_2,
                md_occupancy_attributes::safety_preset_info, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DANGER_ZONE_POINT_0_X_CORD,
           make_attribute_parser(&md_occupancy::danger_zone_point_0_x_cord,
               md_occupancy_attributes::danger_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DANGER_ZONE_POINT_0_Y_CORD,
           make_attribute_parser(&md_occupancy::danger_zone_point_0_y_cord,
               md_occupancy_attributes::danger_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DANGER_ZONE_POINT_1_X_CORD,
           make_attribute_parser(&md_occupancy::danger_zone_point_1_x_cord,
               md_occupancy_attributes::danger_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DANGER_ZONE_POINT_1_Y_CORD,
           make_attribute_parser(&md_occupancy::danger_zone_point_1_y_cord,
               md_occupancy_attributes::danger_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DANGER_ZONE_POINT_2_X_CORD,
           make_attribute_parser(&md_occupancy::danger_zone_point_2_x_cord,
               md_occupancy_attributes::danger_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DANGER_ZONE_POINT_2_Y_CORD,
           make_attribute_parser(&md_occupancy::danger_zone_point_2_y_cord,
               md_occupancy_attributes::danger_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DANGER_ZONE_POINT_3_X_CORD,
           make_attribute_parser(&md_occupancy::danger_zone_point_3_x_cord,
               md_occupancy_attributes::danger_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DANGER_ZONE_POINT_3_Y_CORD,
           make_attribute_parser(&md_occupancy::danger_zone_point_3_y_cord,
               md_occupancy_attributes::danger_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_WARNING_ZONE_POINT_0_X_CORD,
           make_attribute_parser(&md_occupancy::warning_zone_point_0_x_cord,
               md_occupancy_attributes::warning_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_WARNING_ZONE_POINT_0_Y_CORD,
           make_attribute_parser(&md_occupancy::warning_zone_point_0_y_cord,
               md_occupancy_attributes::warning_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_WARNING_ZONE_POINT_1_X_CORD,
           make_attribute_parser(&md_occupancy::warning_zone_point_1_x_cord,
               md_occupancy_attributes::warning_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_WARNING_ZONE_POINT_1_Y_CORD,
           make_attribute_parser(&md_occupancy::warning_zone_point_1_y_cord,
               md_occupancy_attributes::warning_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_WARNING_ZONE_POINT_2_X_CORD,
           make_attribute_parser(&md_occupancy::warning_zone_point_2_x_cord,
               md_occupancy_attributes::warning_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_WARNING_ZONE_POINT_2_Y_CORD,
           make_attribute_parser(&md_occupancy::warning_zone_point_2_y_cord,
               md_occupancy_attributes::warning_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_WARNING_ZONE_POINT_3_X_CORD,
           make_attribute_parser(&md_occupancy::warning_zone_point_3_x_cord,
               md_occupancy_attributes::warning_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_WARNING_ZONE_POINT_3_Y_CORD,
           make_attribute_parser(&md_occupancy::warning_zone_point_3_y_cord,
               md_occupancy_attributes::warning_zone, md_prop_offset));  

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_POINT_0_X_CORD,
           make_attribute_parser(&md_occupancy::diagnostic_zone_point_0_x_cord,
               md_occupancy_attributes::diagnostic_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_POINT_0_Y_CORD,
           make_attribute_parser(&md_occupancy::diagnostic_zone_point_0_y_cord,
               md_occupancy_attributes::diagnostic_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_POINT_1_X_CORD,
           make_attribute_parser(&md_occupancy::diagnostic_zone_point_1_x_cord,
               md_occupancy_attributes::diagnostic_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_POINT_1_Y_CORD,
           make_attribute_parser(&md_occupancy::diagnostic_zone_point_1_y_cord,
               md_occupancy_attributes::diagnostic_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_POINT_2_X_CORD,
           make_attribute_parser(&md_occupancy::diagnostic_zone_point_2_x_cord,
               md_occupancy_attributes::diagnostic_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_POINT_2_Y_CORD,
           make_attribute_parser(&md_occupancy::diagnostic_zone_point_2_y_cord,
               md_occupancy_attributes::diagnostic_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_POINT_3_X_CORD,
           make_attribute_parser(&md_occupancy::diagnostic_zone_point_3_x_cord,
               md_occupancy_attributes::diagnostic_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_POINT_3_Y_CORD,
           make_attribute_parser(&md_occupancy::diagnostic_zone_point_3_y_cord,
               md_occupancy_attributes::diagnostic_zone, md_prop_offset));  

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_OCCUPANCY_GRID_ROWS,
            make_attribute_parser(&md_occupancy::grid_rows,
                md_occupancy_attributes::grid_rows_attribute, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_OCCUPANCY_GRID_COLUMNS,
            make_attribute_parser(&md_occupancy::grid_columns,
                md_occupancy_attributes::grid_columns_attribute, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_OCCUPANCY_CELL_SIZE,
            make_attribute_parser(&md_occupancy::cell_size,
                md_occupancy_attributes::cell_size_attribute, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_OCCUPANCY_GRID_ORIGIN_X,
            make_attribute_parser(&md_occupancy::grid_origin_x_mm,
                md_occupancy_attributes::grid_origin_x_attribute, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_OCCUPANCY_GRID_ORIGIN_Y,
            make_attribute_parser(&md_occupancy::grid_origin_y_mm,
                md_occupancy_attributes::grid_origin_y_attribute, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_CRC,
            make_attribute_parser(&md_occupancy::payload_crc32,
                md_occupancy_attributes::payload_crc32_attribute, md_prop_offset));
    }

    void d500_depth_mapping::register_point_cloud_metadata(std::shared_ptr<uvc_sensor> raw_mapping_ep)
    {
        // attributes of md_point_cloud
        auto md_prop_offset = metadata_raw_mode_offset +
            offsetof(md_mapping_mode, intel_point_cloud);

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_DEPTH_FRAME_COUNTER,
            make_attribute_parser(&md_point_cloud::depth_frame_counter,
                md_point_cloud_attributes::depth_frame_counter_attribute, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_FLOOR_DETECTION,
            make_attribute_parser(&md_point_cloud::floor_detection,
                md_point_cloud_attributes::floor_detection_attribute, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_FILL_RATE,
            make_attribute_parser(&md_point_cloud::diagnostic_zone_fill_rate,
                md_point_cloud_attributes::diagnostic_zone_fill_rate, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DEPTH_FILL_RATE,
            make_attribute_parser(&md_point_cloud::depth_fill_rate,
                md_point_cloud_attributes::depth_fill_rate_attribute, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_SENSOR_ANGLE_ROLL,
            make_attribute_parser(&md_point_cloud::sensor_roll_angle,
                md_point_cloud_attributes::sensor_roll_angle_attribute, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_SENSOR_ANGLE_PITCH,
            make_attribute_parser(&md_point_cloud::sensor_pitch_angle,
                md_point_cloud_attributes::sensor_pitch_angle_attribute, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_MEDIAN_HEIGHT,
            make_attribute_parser(&md_point_cloud::diagnostic_zone_median_height,
                md_point_cloud_attributes::diagnostic_zone_median_height_attribute, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DEPTH_STDEV,
            make_attribute_parser(&md_point_cloud::depth_stdev,
                md_point_cloud_attributes::depth_stdev_attribute, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_PRESET_ID,
            make_attribute_parser(&md_point_cloud::safety_preset_id,
                md_point_cloud_attributes::safety_preset_info, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_PRESET_ERROR_TYPE,
            make_attribute_parser(&md_point_cloud::safety_preset_error_type,
                md_point_cloud_attributes::safety_preset_info, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_PRESET_ERROR_PARAM_1,
            make_attribute_parser(&md_point_cloud::safety_preset_error_param_1,
                md_point_cloud_attributes::safety_preset_info, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_PRESET_ERROR_PARAM_2,
            make_attribute_parser(&md_point_cloud::safety_preset_error_param_2,
                md_point_cloud_attributes::safety_preset_info, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DANGER_ZONE_POINT_0_X_CORD,
           make_attribute_parser(&md_point_cloud::danger_zone_point_0_x_cord,
               md_point_cloud_attributes::danger_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DANGER_ZONE_POINT_0_Y_CORD,
           make_attribute_parser(&md_point_cloud::danger_zone_point_0_y_cord,
               md_point_cloud_attributes::danger_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DANGER_ZONE_POINT_1_X_CORD,
           make_attribute_parser(&md_point_cloud::danger_zone_point_1_x_cord,
               md_point_cloud_attributes::danger_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DANGER_ZONE_POINT_1_Y_CORD,
           make_attribute_parser(&md_point_cloud::danger_zone_point_1_y_cord,
               md_point_cloud_attributes::danger_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DANGER_ZONE_POINT_2_X_CORD,
           make_attribute_parser(&md_point_cloud::danger_zone_point_2_x_cord,
               md_point_cloud_attributes::danger_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DANGER_ZONE_POINT_2_Y_CORD,
           make_attribute_parser(&md_point_cloud::danger_zone_point_2_y_cord,
               md_point_cloud_attributes::danger_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DANGER_ZONE_POINT_3_X_CORD,
           make_attribute_parser(&md_point_cloud::danger_zone_point_3_x_cord,
               md_point_cloud_attributes::danger_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DANGER_ZONE_POINT_3_Y_CORD,
           make_attribute_parser(&md_point_cloud::danger_zone_point_3_y_cord,
               md_point_cloud_attributes::danger_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_WARNING_ZONE_POINT_0_X_CORD,
           make_attribute_parser(&md_point_cloud::warning_zone_point_0_x_cord,
               md_point_cloud_attributes::warning_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_WARNING_ZONE_POINT_0_Y_CORD,
           make_attribute_parser(&md_point_cloud::warning_zone_point_0_y_cord,
               md_point_cloud_attributes::warning_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_WARNING_ZONE_POINT_1_X_CORD,
           make_attribute_parser(&md_point_cloud::warning_zone_point_1_x_cord,
               md_point_cloud_attributes::warning_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_WARNING_ZONE_POINT_1_Y_CORD,
           make_attribute_parser(&md_point_cloud::warning_zone_point_1_y_cord,
               md_point_cloud_attributes::warning_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_WARNING_ZONE_POINT_2_X_CORD,
           make_attribute_parser(&md_point_cloud::warning_zone_point_2_x_cord,
               md_point_cloud_attributes::warning_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_WARNING_ZONE_POINT_2_Y_CORD,
           make_attribute_parser(&md_point_cloud::warning_zone_point_2_y_cord,
               md_point_cloud_attributes::warning_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_WARNING_ZONE_POINT_3_X_CORD,
           make_attribute_parser(&md_point_cloud::warning_zone_point_3_x_cord,
               md_point_cloud_attributes::warning_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_WARNING_ZONE_POINT_3_Y_CORD,
           make_attribute_parser(&md_point_cloud::warning_zone_point_3_y_cord,
               md_point_cloud_attributes::warning_zone, md_prop_offset));  

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_POINT_0_X_CORD,
           make_attribute_parser(&md_point_cloud::diagnostic_zone_point_0_x_cord,
               md_point_cloud_attributes::diagnostic_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_POINT_0_Y_CORD,
           make_attribute_parser(&md_point_cloud::diagnostic_zone_point_0_y_cord,
               md_point_cloud_attributes::diagnostic_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_POINT_1_X_CORD,
           make_attribute_parser(&md_point_cloud::diagnostic_zone_point_1_x_cord,
               md_point_cloud_attributes::diagnostic_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_POINT_1_Y_CORD,
           make_attribute_parser(&md_point_cloud::diagnostic_zone_point_1_y_cord,
               md_point_cloud_attributes::diagnostic_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_POINT_2_X_CORD,
           make_attribute_parser(&md_point_cloud::diagnostic_zone_point_2_x_cord,
               md_point_cloud_attributes::diagnostic_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_POINT_2_Y_CORD,
           make_attribute_parser(&md_point_cloud::diagnostic_zone_point_2_y_cord,
               md_point_cloud_attributes::diagnostic_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_POINT_3_X_CORD,
           make_attribute_parser(&md_point_cloud::diagnostic_zone_point_3_x_cord,
               md_point_cloud_attributes::diagnostic_zone, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_DIAGNOSTIC_ZONE_POINT_3_Y_CORD,
           make_attribute_parser(&md_point_cloud::diagnostic_zone_point_3_y_cord,
               md_point_cloud_attributes::diagnostic_zone, md_prop_offset));  

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_NUMBER_OF_3D_VERTICES,
            make_attribute_parser(&md_point_cloud::number_of_3d_vertices,
                md_point_cloud_attributes::number_of_3d_vertices_attribute, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_CRC,
            make_attribute_parser(&md_point_cloud::payload_crc32,
                md_point_cloud_attributes::payload_crc32_attribute, md_prop_offset));
    }

void d500_depth_mapping::register_processing_blocks( std::shared_ptr< d500_depth_mapping_sensor > mapping_ep )
    {
        processing_block_factory occ_pbf
            = { { { RS2_FORMAT_Y8, RS2_STREAM_OCCUPANCY } },
                { { RS2_FORMAT_Y8, RS2_STREAM_OCCUPANCY } },
                []() {
                    return std::make_shared< identity_processing_block >();
                } };
        mapping_ep->register_processing_block( occ_pbf );

        processing_block_factory lpc_pbf
            = { { { RS2_FORMAT_Y8, RS2_STREAM_LABELED_POINT_CLOUD } },
                { { RS2_FORMAT_Y8, RS2_STREAM_LABELED_POINT_CLOUD } },
                []() {
                    return std::make_shared< identity_processing_block >();
                } };
        mapping_ep->register_processing_block( lpc_pbf );
    }

    void d500_depth_mapping_sensor::open( const stream_profiles & requests )
    {
        // Reject combinations the camera refuses or starves before the device is touched
        _owner->occupancy_allowed_or_throw( requests );
        synthetic_sensor::open( requests );
    }

    stream_profiles d500_depth_mapping_sensor::init_stream_profiles()
    {
        auto lock = environment::get_instance().get_extrinsics_graph().lock();
        auto results = synthetic_sensor::init_stream_profiles();
        stream_profiles relevant_results;
        for (auto p : results)
        {
            if (p->get_stream_type() == RS2_STREAM_OCCUPANCY)
            {
                const auto&& profile = to_profile(p.get());
                // The mapping interface also advertises the plain point-cloud selectors
                // (640x480, 1280x720), which have no rs2 stream of their own and would
                // otherwise surface as bogus occupancy profiles. Keep only the canvas.
                if (_owner->_is_safety_layout ? (profile.width == 2880)
                                              : (profile.width != 320 || profile.height != 256))
                    continue;
                relevant_results.push_back(std::move(p));
            }
            else if (p->get_stream_type() == RS2_STREAM_LABELED_POINT_CLOUD)
            {
                const auto&& profile = to_profile(p.get());
                if (_owner->_is_safety_layout ? (profile.width == 256)
                                              : (profile.width != 640 || profile.height != 360))
                    continue;
                relevant_results.push_back(std::move(p));
            }
        }

        for (auto p : relevant_results)
        {
            // Register stream types
            if (p->get_stream_type() == RS2_STREAM_OCCUPANCY)
                assign_stream(_owner->_occupancy_stream, p);
            else if (p->get_stream_type() == RS2_STREAM_LABELED_POINT_CLOUD)
                assign_stream(_owner->_point_cloud_stream, p);

            auto&& video = dynamic_cast<video_stream_profile_interface*>(p.get());
            const auto&& profile = to_profile(p.get());

            std::weak_ptr<d500_depth_mapping_sensor> wp =
                std::dynamic_pointer_cast<d500_depth_mapping_sensor>(this->shared_from_this());
            video->set_intrinsics([profile, wp]()
                {
                    auto sp = wp.lock();
                    if (sp)
                        return sp->get_intrinsics(profile);
                    else
                        return rs2_intrinsics{};
                });
        }

        return relevant_results;
    }

    rs2_intrinsics d500_depth_mapping_sensor::get_intrinsics(const stream_profile& profile) const
    {
        return rs2_intrinsics();
    }
}
