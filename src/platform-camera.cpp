// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023-4 RealSense, Inc. All Rights Reserved.

#include "platform-camera.h"
#include "ds/ds-timestamp.h"
#include "environment.h"
#include "stream.h"
#include "proc/color-formats-converter.h"
#include "backend.h"
#include "platform/platform-utils.h"
#include <src/metadata-parser.h>

#include <rsutils/type/fourcc.h>
using rsutils::type::fourcc;


namespace librealsense {
namespace {


const std::map< fourcc::value_type, rs2_format > platform_fourcc_to_rs2_format = {
    { fourcc( 'Y', 'U', 'Y', '2' ), RS2_FORMAT_YUYV },
    { fourcc( 'Y', 'U', 'Y', 'V' ), RS2_FORMAT_YUYV },
    { fourcc( 'M', 'J', 'P', 'G' ), RS2_FORMAT_MJPEG },
    { fourcc( 'N', 'V', '1', '2' ), RS2_FORMAT_NV12 },
    { fourcc( 'G', 'R', 'E', 'Y' ), RS2_FORMAT_Y8 },
};
const std::map< fourcc::value_type, rs2_stream > platform_fourcc_to_rs2_stream = {
    { fourcc( 'Y', 'U', 'Y', '2' ), RS2_STREAM_COLOR },
    { fourcc( 'Y', 'U', 'Y', 'V' ), RS2_STREAM_COLOR },
    { fourcc( 'M', 'J', 'P', 'G' ), RS2_STREAM_COLOR },
    { fourcc( 'N', 'V', '1', '2' ), RS2_STREAM_COLOR },
    { fourcc( 'G', 'R', 'E', 'Y' ), RS2_STREAM_INFRARED },
};


class platform_camera_sensor : public synthetic_sensor
{
public:
    platform_camera_sensor( device * owner, std::shared_ptr< uvc_sensor > uvc_sensor )
        : synthetic_sensor(
            "RGB Camera", uvc_sensor, owner, platform_fourcc_to_rs2_format, platform_fourcc_to_rs2_stream )
        , _color_stream( new stream( RS2_STREAM_COLOR ) )
        , _ir_stream( new stream( RS2_STREAM_INFRARED ) )
    {
    }

    stream_profiles init_stream_profiles() override
    {
        auto lock = environment::get_instance().get_extrinsics_graph().lock();

        auto results = synthetic_sensor::init_stream_profiles();

        for( auto && p : results )
        {
            auto & s = ( p->get_stream_type() == RS2_STREAM_INFRARED ) ? _ir_stream : _color_stream;
            assign_stream( s, p );
            environment::get_instance().get_extrinsics_graph().register_same_extrinsics( *s, *p );
        }

        return results;
    }


private:
    std::shared_ptr< stream_interface > _color_stream;
    std::shared_ptr< stream_interface > _ir_stream;
};


}  // namespace

void platform_camera::initialize()
{
    auto const n_sensors = get_sensors_count();
    for (auto i = 0; i < n_sensors; ++i)
    {
        if (auto sensor = dynamic_cast<platform_camera_sensor*>(&(get_sensor(i))))
        {
            if (sensor->get_device().get_info(RS2_CAMERA_INFO_NAME) == "Platform Camera")
            {
                auto options = std::vector< rs2_option >{ RS2_OPTION_BACKLIGHT_COMPENSATION,
                                                          RS2_OPTION_BRIGHTNESS,
                                                          RS2_OPTION_CONTRAST,
                                                          RS2_OPTION_EXPOSURE,
                                                          RS2_OPTION_GAMMA,
                                                          RS2_OPTION_HUE,
                                                          RS2_OPTION_SATURATION,
                                                          RS2_OPTION_SHARPNESS,
                                                          RS2_OPTION_WHITE_BALANCE,
                                                          RS2_OPTION_ENABLE_AUTO_EXPOSURE,
                                                          RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE };
                for (const auto& option : options)
                {
                    if (should_stop)
                        return;  // Stop initialization if requested
                    sensor->try_register_pu(option);
                }
            }
        }
    }
}

platform_camera::platform_camera( std::shared_ptr< const device_info > const & dev_info,
                                  const std::vector< platform::uvc_device_info > & uvc_infos,
                                  bool register_device_notifications )
    : device( dev_info, register_device_notifications )
    , backend_device( dev_info, register_device_notifications )
{
    std::vector< std::shared_ptr< platform::uvc_device > > devs;
    auto backend = get_backend();
    for( auto & info : uvc_infos )
        devs.push_back( backend->create_uvc_device( info ) );

    std::unique_ptr< frame_timestamp_reader > host_timestamp_reader_backup( new ds_timestamp_reader() );
    auto raw_color_ep = std::make_shared< uvc_sensor >(
        "Raw RGB Camera",
        std::make_shared< platform::multi_pins_uvc_device >( devs ),
        std::unique_ptr< frame_timestamp_reader >(
            new ds_timestamp_reader_from_metadata( std::move( host_timestamp_reader_backup ) ) ),
        this );
    auto color_ep = std::make_shared< platform_camera_sensor >( this, raw_color_ep );
    add_sensor( color_ep );

    register_info( RS2_CAMERA_INFO_NAME, "Platform Camera" );
    std::string pid_str( rsutils::string::from()
                         << std::setfill( '0' ) << std::setw( 4 ) << std::hex << uvc_infos.front().pid );
    std::transform( pid_str.begin(), pid_str.end(), pid_str.begin(), ::toupper );

    using namespace platform;
    auto usb_mode = raw_color_ep->get_usb_specification();
    std::string usb_type_str( "USB" );
    if( usb_spec_names.count( usb_mode ) && ( usb_undefined != usb_mode ) )
        usb_type_str = usb_spec_names.at( usb_mode );

    register_info(RS2_CAMERA_INFO_CONNECTION_TYPE, "USB");
    register_info( RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR, usb_type_str );
    register_info( RS2_CAMERA_INFO_SERIAL_NUMBER, uvc_infos.front().unique_id );
    register_info( RS2_CAMERA_INFO_PHYSICAL_PORT, uvc_infos.front().device_path );
    register_info( RS2_CAMERA_INFO_PRODUCT_ID, pid_str );

    color_ep->register_processing_block(
        processing_block_factory::create_pbf_vector< yuy2_converter >( RS2_FORMAT_YUYV,
                                                                       map_supported_color_formats( RS2_FORMAT_YUYV ),
                                                                       RS2_STREAM_COLOR ) );
    color_ep->register_processing_block(
        processing_block_factory::create_pbf_vector< nv12_converter >( RS2_FORMAT_NV12,
                                                                       map_supported_color_formats( RS2_FORMAT_NV12 ),
                                                                       RS2_STREAM_COLOR ) );
    color_ep->register_processing_block( { { RS2_FORMAT_MJPEG } },
                                         { { RS2_FORMAT_RGB8, RS2_STREAM_COLOR } },
                                         []() { return std::make_shared< mjpeg_converter >( RS2_FORMAT_RGB8 ); } );
    color_ep->register_processing_block(
        processing_block_factory::create_id_pbf( RS2_FORMAT_MJPEG, RS2_STREAM_COLOR ) );
    color_ep->register_processing_block(
        processing_block_factory::create_id_pbf( RS2_FORMAT_Y8, RS2_STREAM_INFRARED ) );

    // Timestamps are given in units set by device which may vary among the OEM vendors.
    // For consistent (msec) measurements use "time of arrival" metadata attribute
    color_ep->register_metadata( RS2_FRAME_METADATA_FRAME_TIMESTAMP,
                                 make_uvc_header_parser( &platform::uvc_header::timestamp ) );

    // Create a thread to call initialize after a delay
    _init_thread = std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::seconds(2)); // Delay for 2 seconds
        this->initialize();
    });
}

platform_camera::~platform_camera()
{
    should_stop = true;
    if( _init_thread.joinable() )
        _init_thread.join();
}

std::vector< tagged_profile > platform_camera::get_profiles_tags() const
{
    std::vector< tagged_profile > markers;
    markers.push_back( { RS2_STREAM_COLOR,
                         -1,
                         1280,
                         720,
                         RS2_FORMAT_RGB8,
                         30,
                         profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT } );
    return markers;
}


/*static*/ std::vector< std::shared_ptr< platform_camera_info > >
platform_camera_info::pick_uvc_devices( const std::shared_ptr< context > & ctx,
                                        const std::vector< platform::uvc_device_info > & uvc_devices )
{
    std::vector< std::shared_ptr< platform_camera_info > > list;
    auto groups = group_devices_by_unique_id( uvc_devices );

    for( auto && g : groups )
    {
        if( g.front().vid != VID_INTEL_CAMERA )
            list.push_back( std::make_shared< platform_camera_info >( ctx, std::move( g ) ) );
    }
    return list;
}


}  // namespace librealsense
