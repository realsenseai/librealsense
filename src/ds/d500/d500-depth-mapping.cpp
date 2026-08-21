// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#include "d500-depth-mapping.h"

#include "d500-safety.h"
#include "d500-info.h"
#include "d585s-md.h"
#include "d500-types/safety-interface-config.h"

#include <vector>
#include <map>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "ds/ds-timestamp.h"
#include "ds/ds-options.h"
#include <src/backend.h>
#include <rsutils/type/fourcc.h>
using rs_fourcc = rsutils::type::fourcc;
#include "stream.h"

#include "platform/platform-utils.h"

#include <src/metadata-parser.h>
#include <thread>

namespace librealsense
{
    namespace
    {
        const uint32_t map1_magic = 0x3150414d;
        const uint16_t map1_version_major = 1;
        const uint8_t map1_occupancy_type = 2;
        const uint16_t map1_occupancy_profile = 0x0201;
        const size_t map1_common_header_size = 20;
        const size_t map1_occupancy_header_size = 32;
        const uint16_t map1_occupancy_width = 320;
        const uint16_t map1_occupancy_height = 256;
        const uint16_t map1_occupancy_resolution_mm = 50;
        const uint32_t map1_occupancy_cell_count =
            map1_occupancy_width * map1_occupancy_height;
        const size_t map1_occupancy_frame_size =
            map1_common_header_size + map1_occupancy_header_size
            + map1_occupancy_cell_count;

        uint16_t read_le16( const uint8_t * p )
        {
            return static_cast< uint16_t >( p[0] )
                | ( static_cast< uint16_t >( p[1] ) << 8 );
        }

        uint32_t read_le32( const uint8_t * p )
        {
            return static_cast< uint32_t >( p[0] )
                | ( static_cast< uint32_t >( p[1] ) << 8 )
                | ( static_cast< uint32_t >( p[2] ) << 16 )
                | ( static_cast< uint32_t >( p[3] ) << 24 );
        }

        int32_t read_le_i32( const uint8_t * p )
        {
            return static_cast< int32_t >( read_le32( p ) );
        }

        uint32_t map1_crc32( const uint8_t * data, size_t size )
        {
            uint32_t crc = 0xffffffff;
            for( size_t i = 0; i < size; ++i )
            {
                crc ^= data[i];
                for( uint8_t bit = 0; bit < 8; ++bit )
                {
                    const uint32_t mask = 0u - ( crc & 1u );
                    crc = ( crc >> 1 ) ^ ( 0xedb88320u & mask );
                }
            }
            return ~crc;
        }

        class map1_occupancy_processing_block : public stream_filter_processing_block
        {
        public:
            map1_occupancy_processing_block()
                : stream_filter_processing_block( "D500 MAP1 Occupancy Grid" )
            {
                _stream_filter.stream = RS2_STREAM_OCCUPANCY;
                _stream_filter.format = RS2_FORMAT_Y8;
            }

        protected:
            rs2::frame process_frame( const rs2::frame_source & source,
                                      const rs2::frame & frame ) override
            {
                const uint8_t * data = static_cast< const uint8_t * >( frame.get_data() );
                const size_t size = static_cast< size_t >( frame.get_data_size() );
                const char * error = validate( data, size );
                if( error )
                {
                    ++_invalid_frames;
                    if( _invalid_frames <= 3 || ( _invalid_frames % 300 ) == 0 )
                        LOG_WARNING( "Dropping invalid D500 MAP1 Occupancy frame: " << error
                                     << " (size=" << size << ")" );
                    return {};
                }

                auto output = source.allocate_video_frame( frame.get_profile(),
                                                           frame,
                                                           1,
                                                           map1_occupancy_width,
                                                           map1_occupancy_height,
                                                           map1_occupancy_width,
                                                           RS2_EXTENSION_VIDEO_FRAME );
                if( ! output )
                    return {};

                std::memcpy( const_cast< void * >( output.get_data() ),
                             data + map1_common_header_size + map1_occupancy_header_size,
                             map1_occupancy_cell_count );
                return output;
            }

            rs2::frame prepare_output( const rs2::frame_source & source,
                                       rs2::frame input,
                                       std::vector< rs2::frame > results ) override
            {
                if( results.empty() && ! input.is< rs2::frameset >() )
                    return {};
                return generic_processing_block::prepare_output( source, input, results );
            }

        private:
            const char * validate( const uint8_t * data, size_t size ) const
            {
                if( ! data || size != map1_occupancy_frame_size )
                    return "unexpected frame size";
                if( read_le32( data ) != map1_magic )
                    return "bad magic";
                if( ( read_le16( data + 4 ) >> 8 ) != map1_version_major )
                    return "unsupported version";
                if( data[6] != map1_occupancy_type )
                    return "unexpected data type";
                if( ( data[7] & 1u ) == 0 )
                    return "CRC flag missing";
                if( read_le32( data + 8 ) != size - map1_common_header_size )
                    return "payload length mismatch";
                if( read_le16( data + 12 ) != map1_occupancy_profile )
                    return "unexpected profile";
                if( read_le16( data + 14 ) == 0 )
                    return "zero stream generation";
                if( map1_crc32( data + map1_common_header_size,
                                size - map1_common_header_size )
                    != read_le32( data + 16 ) )
                    return "CRC mismatch";

                const uint8_t * occupancy = data + map1_common_header_size;
                if( read_le16( occupancy ) != map1_occupancy_width
                    || read_le16( occupancy + 2 ) != map1_occupancy_height
                    || read_le16( occupancy + 4 ) != map1_occupancy_resolution_mm
                    || read_le16( occupancy + 6 ) != 1
                    || read_le_i32( occupancy + 8 ) != -8000
                    || read_le_i32( occupancy + 12 ) != 0
                    || read_le32( occupancy + 28 ) != map1_occupancy_cell_count )
                    return "invalid occupancy header";

                const int8_t * cells = reinterpret_cast< const int8_t * >(
                    occupancy + map1_occupancy_header_size );
                for( uint32_t i = 0; i < map1_occupancy_cell_count; ++i )
                {
                    if( cells[i] != -1 && cells[i] != 0 && cells[i] != 100 )
                        return "invalid cell value";
                }
                return nullptr;
            }

            uint32_t _invalid_frames = 0;
        };
    }

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

    d500_depth_mapping::d500_depth_mapping( std::shared_ptr< const d500_info > const & dev_info,
                                            bool versioned_mapping )
        : device( dev_info ), d500_device( dev_info ),
        _occupancy_stream(new stream(RS2_STREAM_OCCUPANCY)),
        _point_cloud_stream(new stream(RS2_STREAM_LABELED_POINT_CLOUD)),
        _versioned_mapping( versioned_mapping )
    {
        using namespace ds;
        const uint32_t legacy_mapping_stream_mi = 13;
        const uint32_t d500_mapping_stream_mi = 11;
        auto mapping_devs_info = filter_by_mi(
            dev_info->get_group().uvc_devices,
            _versioned_mapping ? d500_mapping_stream_mi : legacy_mapping_stream_mi );

        // Some early D500 descriptors exposed Mapping at the legacy MI while
        // keeping the versioned MAP1 payload. Accept it only as a fallback.
        if( mapping_devs_info.empty() && _versioned_mapping )
            mapping_devs_info = filter_by_mi( dev_info->get_group().uvc_devices,
                                              legacy_mapping_stream_mi );
        
        if( mapping_devs_info.empty() && _versioned_mapping )
            return;
        if (mapping_devs_info.size() != 1)
            throw invalid_value_exception(rsutils::string::from() << "RS5XX models with Safety are expected to include a single depth mapping device! - "
                << mapping_devs_info.size() << " found");

        auto mapping_ep = create_depth_mapping_device( dev_info->get_context(), mapping_devs_info );
        _depth_mapping_device_idx = add_sensor(mapping_ep);
        _has_depth_mapping = true;
    }

    std::shared_ptr<synthetic_sensor> d500_depth_mapping::create_depth_mapping_device(std::shared_ptr<context> ctx,
        const std::vector<platform::uvc_device_info>& occupancy_devices_info)
    {
        using namespace ds;

        std::unique_ptr<frame_timestamp_reader> timestamp_reader(new ds_timestamp_reader());
        if( ! _versioned_mapping )
        {
            std::unique_ptr< frame_timestamp_reader > metadata_reader(
                new ds_timestamp_reader_from_metadata_depth_mapping(
                    std::move( timestamp_reader ) ) );
            timestamp_reader = std::move( metadata_reader );
        }

        auto enable_global_time_option = std::shared_ptr<global_time_option>(new global_time_option());

        auto raw_mapping_ep = std::make_shared<uvc_sensor>("Raw Depth Mapping Device",
            get_backend()->create_uvc_device(occupancy_devices_info.front()),
            std::unique_ptr<frame_timestamp_reader>(new global_timestamp_reader(std::move(timestamp_reader), _tf_keeper, enable_global_time_option)),
            this);
        if( _versioned_mapping )
            raw_mapping_ep->set_variable_frame_size( true );

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
        if( _versioned_mapping )
        {
            register_stream_to_extrinsic_group( *_occupancy_stream, 0 );
            return;
        }
        using rsutils::json;
        // extrinsics to depth lazy, becasue safety sensor's api is used and it may be constructed later
        // than the depth mapping device (though it may not be the case in the device contructor's order, in ds500-factory)
        _depth_to_depth_mapping_extrinsics = std::make_shared< rsutils::lazy< rs2_extrinsics > > ( [this]()
            {
                // getting access to safety sensor api
                auto safety_device = dynamic_cast<d500_safety*>(this);
                if (!safety_device)
                    throw invalid_value_exception("null pointer recieved from dynamic pointer casting.");
                auto& safety_sensor = dynamic_cast<d500_safety_sensor&>(safety_device->get_safety_sensor());
                
                // Pull extrinsic from safety interface config, according to HKR 0.9 QS
                rs2_extrinsics res;
                json sic_json;
                try 
                {
                    sic_json = json::parse(safety_sensor.get_safety_interface_config());
                }
                catch (...)
                {
                    throw std::runtime_error("Could not read safety interface config");
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

    void d500_depth_mapping::register_options(std::shared_ptr<d500_depth_mapping_sensor> occupancy_ep, std::shared_ptr<uvc_sensor> raw_mapping_sensor)
    {

    }

    void d500_depth_mapping::register_metadata(std::shared_ptr<uvc_sensor> raw_mapping_ep)
    {
        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_FRAME_TIMESTAMP, 
            make_uvc_header_parser(&platform::uvc_header::timestamp));

        if( _versioned_mapping )
            return;

        register_occupancy_metadata(raw_mapping_ep);
        register_point_cloud_metadata(raw_mapping_ep);
    }


    void d500_depth_mapping::register_occupancy_metadata(std::shared_ptr<uvc_sensor> raw_mapping_ep)
    {
        // attributes of md_occupancy
        auto md_prop_offset = metadata_raw_mode_offset +
            offsetof(md_mapping_mode, intel_occupancy);

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_FRAME_COUNTER,
            make_attribute_parser(&md_occupancy::frame_counter,
                md_occupancy_attributes::frame_counter_attribute, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_DEPTH_FRAME_COUNTER,
            make_attribute_parser(&md_occupancy::depth_frame_counter,
                md_occupancy_attributes::depth_frame_counter_attribute, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_SENSOR_TIMESTAMP,
            make_attribute_parser(&md_occupancy::frame_timestamp,
                md_occupancy_attributes::frame_timestamp_attribute, md_prop_offset));

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

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_CRC,
            make_attribute_parser(&md_occupancy::payload_crc32,
                md_occupancy_attributes::payload_crc32_attribute, md_prop_offset));
    }

    void d500_depth_mapping::register_point_cloud_metadata(std::shared_ptr<uvc_sensor> raw_mapping_ep)
    {
        // attributes of md_point_cloud
        auto md_prop_offset = metadata_raw_mode_offset +
            offsetof(md_mapping_mode, intel_point_cloud);

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_FRAME_COUNTER,
            make_attribute_parser(&md_point_cloud::frame_counter,
                md_point_cloud_attributes::frame_counter_attribute, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_SAFETY_DEPTH_FRAME_COUNTER,
            make_attribute_parser(&md_point_cloud::depth_frame_counter,
                md_point_cloud_attributes::depth_frame_counter_attribute, md_prop_offset));

        raw_mapping_ep->register_metadata(RS2_FRAME_METADATA_SENSOR_TIMESTAMP,
            make_attribute_parser(&md_point_cloud::frame_timestamp,
                md_point_cloud_attributes::frame_timestamp_attribute, md_prop_offset));

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
                [this]() {
                    if( _versioned_mapping )
                        return std::shared_ptr< processing_block >(
                            std::make_shared< map1_occupancy_processing_block >() );
                    return std::shared_ptr< processing_block >(
                        std::make_shared< identity_processing_block >() );
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

    stream_profiles d500_depth_mapping_sensor::init_stream_profiles()
    {
        auto lock = environment::get_instance().get_extrinsics_graph().lock();
        auto results = synthetic_sensor::init_stream_profiles();
        stream_profiles relevant_results;
        for (auto p : results)
        {
            if (p->get_stream_type() == RS2_STREAM_OCCUPANCY)
            {
                auto&& video = dynamic_cast<video_stream_profile_interface*>(p.get());
                const auto&& profile = to_profile(p.get());
                if( _owner->_versioned_mapping
                    && ( profile.width != map1_occupancy_width
                         || profile.height != map1_occupancy_height ) )
                    continue;
                if (profile.width == 2880)
                    continue;
                relevant_results.push_back(std::move(p));
            }
            else if (p->get_stream_type() == RS2_STREAM_LABELED_POINT_CLOUD)
            {
                auto&& video = dynamic_cast<video_stream_profile_interface*>(p.get());
                const auto&& profile = to_profile(p.get());
                if (profile.width == 256)
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
