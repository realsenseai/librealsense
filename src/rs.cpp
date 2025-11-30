// License: Apache 2.0 See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#include <functional>   // For function

#include "api.h"
#include "log.h"
#include "context.h"
#include "device.h"
#include "algo.h"
#include "core/debug.h"
#include "core/motion.h"
#include "core/extension.h"
#include "media/playback/playback-device-info.h"
#include "media/record/record_device.h"
#include <media/ros2/ros2_writer.h>
#include <media/ros2/ros2_reader.h>
#include "core/advanced_mode.h"
#include "core/pose-frame.h"
#include "core/motion-frame.h"
#include "core/disparity-frame.h"
#include "source.h"
#include "proc/synthetic-stream.h"
#include "proc/processing-blocks-factory.h"
#include "proc/colorizer.h"
#include "proc/pointcloud.h"
#include "proc/align.h"
#include "proc/threshold.h"
#include "proc/units-transform.h"
#include "proc/disparity-transform.h"
#include "proc/syncer-processing-block.h"
#include "proc/decimation-filter.h"
#include "proc/rotation-filter.h"
#include "proc/spatial-filter.h"
#include "proc/hole-filling-filter.h"
#include "proc/color-formats-converter.h"
#include "proc/y411-converter.h"
#include "proc/rates-printer.h"
#include "proc/hdr-merge.h"
#include "proc/sequence-id-filter.h"
#include "proc/decimation-embedded-filter.h"
#include "proc/temporal-embedded-filter.h"
#include "media/playback/playback_device.h"
#include "stream.h"
#include <librealsense2/h/rs_types.h>
#include "pipeline/pipeline.h"
#include "environment.h"
#include "proc/temporal-filter.h"
#include "software-device.h"
#include "software-device-info.h"
#include "software-sensor.h"
#include "global_timestamp_reader.h"
#include "auto-calibrated-device.h"
#include "terminal-parser.h"
#include "firmware_logger_device.h"
#include "device-calibration.h"
#include <librealsense2/h/rs_internal.h>
#include "debug-stream-sensor.h"
#include "max-usable-range-sensor.h"
#include "fw-update/fw-update-device-interface.h"
#include "core/frame-callback.h"
#include "color-sensor.h"
#include "safety-sensor.h"
#include "depth-mapping-sensor.h"
#include "composite-frame.h"
#include "points.h"
#include "labeled-points.h"
#include "eth-config-device.h"
#include "embedded-filter-interface.h"

#include <src/core/time-service.h>
#include <rsutils/string/from.h>
#include <rsutils/type/eth-config.h>

////////////////////////
// API implementation //
////////////////////////

using namespace librealsense;
using rsutils::json;

struct rs2_stream_profile_list
{
    std::vector<std::shared_ptr<stream_profile_interface>> list;
};

struct rs2_processing_block_list
{
    processing_blocks list;
};

struct rs2_device_list
{
    std::shared_ptr< librealsense::context > ctx;
    std::vector< std::shared_ptr< librealsense::device_info > > list;
};

struct rs2_option_value_wrapper : rs2_option_value
{
    // Keep the original json value, so we can refer to it (e.g., with as_string)
    std::shared_ptr< const json > p_json;

    // Add a reference count to control lifetime
    mutable std::atomic< int > ref_count;

    rs2_option_value_wrapper( rs2_option option_id,
                              rs2_option_type option_type,
                              std::shared_ptr< const json > const & p_json_value )
        : ref_count( 1 )
        , p_json( p_json_value )
    {
        id = option_id;
        type = option_type;
        is_valid = false;
        if( p_json && ! p_json->is_null() )
        {
            switch( type )
            {
            case RS2_OPTION_TYPE_FLOAT:
                if( ! p_json->is_number() )
                    throw invalid_value_exception( get_string( option_id )
                                                   + " value is not a float: " + p_json->dump() );
                p_json->get_to( as_float );
                break;

            case RS2_OPTION_TYPE_INTEGER:
                if( ! p_json->is_number_integer() )
                    throw invalid_value_exception( get_string( option_id )
                                                   + " value is not an integer: " + p_json->dump() );
                p_json->get_to( as_integer );
                break;

            case RS2_OPTION_TYPE_BOOLEAN:
                if( ! p_json->is_boolean() )
                    throw invalid_value_exception( get_string( option_id )
                                                   + " value is not a boolean: " + p_json->dump() );
                as_integer = p_json->get< bool >();
                break;

            case RS2_OPTION_TYPE_STRING:
                if( ! p_json->is_string() )
                    throw invalid_value_exception( get_string( option_id )
                                                   + " value is not a string: " + p_json->dump() );
                as_string = p_json->string_ref().c_str();
                break;

            case RS2_OPTION_TYPE_RECT:
                if( ! p_json->is_array() || 4 != p_json->size() )
                    throw invalid_value_exception( get_string( option_id )
                                                   + " value is not a rect: " + p_json->dump() );
                try
                {
                    p_json->at( 0 ).get_to( as_rect.x1 );
                    p_json->at( 1 ).get_to( as_rect.y1 );
                    p_json->at( 2 ).get_to( as_rect.x2 );
                    p_json->at( 3 ).get_to( as_rect.y2 );
                }
                catch( json::exception const & e )
                {
                    throw invalid_value_exception( get_string( option_id )
                                                   + " value is not a rect: " + e.what() );
                }
                break;

            default:
                throw invalid_value_exception( "invalid " + get_string( option_id ) + " type "
                                               + get_string( option_type ) );
            }
            is_valid = true;
        }
    }
};

struct rs2_options_list
{
    std::vector< rs2_option_value_wrapper const * > list;
};

struct rs2_sensor : public rs2_options
{
    rs2_sensor( std::shared_ptr<librealsense::device_interface> parent,
        librealsense::sensor_interface* sensor) :
        rs2_options((librealsense::options_interface*)sensor),
        parent(parent), sensor(sensor)
    {}

    std::shared_ptr<librealsense::device_interface> parent;
    librealsense::sensor_interface* sensor;

    rs2_sensor& operator=(const rs2_sensor&) = delete;
    rs2_sensor(const rs2_sensor&) = delete;
};

struct rs2_context
{
    std::shared_ptr<librealsense::context> ctx;
    mutable rsutils::subscription devices_changed_subscription;
};

struct rs2_device_hub
{
    std::shared_ptr<librealsense::device_hub> hub;
};

struct rs2_pipeline
{
    std::shared_ptr<librealsense::pipeline::pipeline> pipeline;
};

struct rs2_config
{
    std::shared_ptr<librealsense::pipeline::config> config;
};

struct rs2_pipeline_profile
{
    std::shared_ptr<librealsense::pipeline::profile> profile;
};

struct rs2_frame_queue
{
    explicit rs2_frame_queue(int cap)
        : queue( cap, [cap]( librealsense::frame_holder const & fh ) {
            LOG_DEBUG( "DROPPED queue (capacity= " << cap << ") frame " << fh );
        } )
    {
    }

    single_consumer_frame_queue<librealsense::frame_holder> queue;
};

struct rs2_sensor_list
{
    std::shared_ptr<librealsense::device_interface> device;
};

struct rs2_terminal_parser
{
    std::shared_ptr<librealsense::terminal_parser> terminal_parser;
};

struct rs2_firmware_log_message
{
    std::shared_ptr<librealsense::fw_logs::fw_logs_binary_data> firmware_log_binary_data;
};

struct rs2_firmware_log_parsed_message
{
    std::shared_ptr<librealsense::fw_logs::fw_log_data> firmware_log_parsed;
};

struct rs2_error
{
    std::string message;
    std::string function;
    std::string args;
    rs2_exception_type exception_type;
};

struct rs2_embedded_filter : public rs2_options
{
    rs2_embedded_filter(std::shared_ptr<librealsense::embedded_filter_interface> embedded_filter)
        : rs2_options((librealsense::options_interface*)embedded_filter.get())
        , _embedded_filter(embedded_filter)
    {}

    std::shared_ptr<librealsense::embedded_filter_interface> _embedded_filter;

    rs2_embedded_filter& operator=(const rs2_embedded_filter&) = delete;
    rs2_embedded_filter(const rs2_embedded_filter&) = delete;
};

struct rs2_embedded_filter_list
{
    std::vector<std::shared_ptr<librealsense::embedded_filter_interface>> list;
};

rs2_error *rs2_create_error(const char* what, const char* name, const char* args, rs2_exception_type type) BEGIN_API_CALL
{
    LOG_ERROR( "[" << name << "( " << args << " ) " << rs2_exception_type_to_string( type ) << "] " << what );
    return new rs2_error{ what, name, args, type };
}
NOEXCEPT_RETURN(nullptr, what, name, args, type)

void notifications_processor::raise_notification(const notification n)
{
    _dispatcher.invoke([this, n](dispatcher::cancellable_timer ct)
    {
        std::lock_guard<std::mutex> lock(_callback_mutex);
        rs2_notification noti(&n);
        if (_callback)_callback->on_notification(&noti);
    });
}

rs2_context* rs2_create_context(int api_version, rs2_error** error) BEGIN_API_CALL
{
    verify_version_compatibility(api_version);

    json settings = json::object();
    return new rs2_context{ context::make( settings ) };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, api_version)

rs2_context* rs2_create_context_ex(int api_version, const char * json_settings, rs2_error** error) BEGIN_API_CALL
{
    verify_version_compatibility(api_version);

    return new rs2_context{ context::make( json_settings ) };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, api_version, json_settings)

void rs2_delete_context(rs2_context* context) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(context);
    delete context;
}
NOEXCEPT_RETURN(, context)

rs2_device_hub* rs2_create_device_hub(const rs2_context* context, rs2_error** error) BEGIN_API_CALL
{
    return new rs2_device_hub{ device_hub::make( context->ctx ) };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, context)

void rs2_delete_device_hub(const rs2_device_hub* hub) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(hub);
    delete hub;
}
NOEXCEPT_RETURN(, hub)

rs2_device* rs2_device_hub_wait_for_device(const rs2_device_hub* hub, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(hub);
    auto dev = hub->hub->wait_for_device();
    return new rs2_device{ dev };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, hub)

int rs2_device_hub_is_device_connected(const rs2_device_hub* hub, const rs2_device* device, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(hub);
    VALIDATE_NOT_NULL(device);
    auto res = hub->hub->is_connected(*device->device);
    return res ? 1 : 0;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, hub, device)

rs2_device_list* rs2_query_devices(const rs2_context* context, rs2_error** error) BEGIN_API_CALL
{
    return rs2_query_devices_ex(context, RS2_PRODUCT_LINE_ANY_INTEL, error);
}
HANDLE_EXCEPTIONS_AND_RETURN(0, context)

rs2_device_list* rs2_query_devices_ex(const rs2_context* context, int product_mask, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(context);

    auto results = context->ctx->query_devices( product_mask );
    return new rs2_device_list{ context->ctx, results };
}
HANDLE_EXCEPTIONS_AND_RETURN(0, context, product_mask)

rs2_sensor_list* rs2_query_sensors(const rs2_device* device, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);

    return new rs2_sensor_list{ device->device };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device)

int rs2_get_device_count(const rs2_device_list* list, rs2_error** error) BEGIN_API_CALL
{
    if (list == nullptr)
        return 0;
    return static_cast<int>(list->list.size());
}
HANDLE_EXCEPTIONS_AND_RETURN(0, list)

int rs2_get_sensors_count(const rs2_sensor_list* list, rs2_error** error) BEGIN_API_CALL
{
    if (list == nullptr)
        return 0;
    return static_cast<int>(list->device->get_sensors_count());
}
HANDLE_EXCEPTIONS_AND_RETURN(0, list)

void rs2_delete_device_list(rs2_device_list* list) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(list);
    delete list;
}
NOEXCEPT_RETURN(, list)

void rs2_delete_sensor_list(rs2_sensor_list* list) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(list);
    delete list;
}
NOEXCEPT_RETURN(, list)

rs2_device* rs2_create_device(const rs2_device_list* info_list, int index, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(info_list);
    VALIDATE_RANGE(index, 0, (int)info_list->list.size() - 1);

    return new rs2_device{ info_list->list[index]->create_device() };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info_list, index)

void rs2_delete_device(rs2_device* device) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    delete device;
}
NOEXCEPT_RETURN(, device)

int rs2_device_is_connected( const rs2_device * device, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( device );
    VALIDATE_NOT_NULL( device->device );
    return device->device->is_valid();
}
HANDLE_EXCEPTIONS_AND_RETURN( 0, device )

rs2_sensor* rs2_create_sensor(const rs2_sensor_list* list, int index, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(list);
    VALIDATE_RANGE(index, 0, (int)list->device->get_sensors_count() - 1);

    return new rs2_sensor{
            list->device,
            &list->device->get_sensor(index)
    };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

void rs2_delete_sensor(rs2_sensor* sensor) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    delete sensor;
}
NOEXCEPT_RETURN(, sensor)

rs2_stream_profile_list* rs2_get_stream_profiles(rs2_sensor* sensor, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    return new rs2_stream_profile_list{ sensor->sensor->get_stream_profiles() };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, sensor)

rs2_stream_profile_list * rs2_get_debug_stream_profiles( rs2_sensor * sensor,
                                                   rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( sensor );
    auto debug_streaming
        = VALIDATE_INTERFACE( sensor->sensor, librealsense::debug_stream_sensor );
    return new rs2_stream_profile_list{ debug_streaming->get_debug_stream_profiles() };
}
HANDLE_EXCEPTIONS_AND_RETURN( nullptr, sensor )

rs2_embedded_filter_list* rs2_query_embedded_filters(const rs2_sensor* sensor, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);

    return new rs2_embedded_filter_list{ sensor->sensor->get_supported_embedded_filters() };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, sensor)


void rs2_delete_embedded_filter_list(rs2_embedded_filter_list* list) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(list);
    delete list;
}
NOEXCEPT_RETURN(, list)


int rs2_get_embedded_filters_count(const rs2_embedded_filter_list* list, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(list);
    return static_cast<int>(list->list.size());
}
HANDLE_EXCEPTIONS_AND_RETURN(0, list)

rs2_embedded_filter* rs2_create_embedded_filter(const rs2_embedded_filter_list* list, int index, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(list);
    VALIDATE_RANGE(index, 0, (int)list->list.size() - 1);

    return new rs2_embedded_filter{ (list->list[index]) };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

void rs2_delete_embedded_filter(rs2_embedded_filter* embedded_filter) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(embedded_filter);
    delete embedded_filter;
}
NOEXCEPT_RETURN(, embedded_filter)

rs2_embedded_filter_type rs2_get_embedded_filter_type(const rs2_embedded_filter* embedded_filter, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(embedded_filter);
    return embedded_filter->_embedded_filter->get_type();
}
HANDLE_EXCEPTIONS_AND_RETURN(RS2_EMBEDDED_FILTER_TYPE_COUNT, embedded_filter)

rs2_stream_profile_list* rs2_get_active_streams(rs2_sensor* sensor, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    return new rs2_stream_profile_list{ sensor->sensor->get_active_streams() };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, sensor)

const rs2_stream_profile* rs2_get_stream_profile(const rs2_stream_profile_list* list, int index, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(list);
    VALIDATE_RANGE(index, 0, (int)list->list.size() - 1);

    return list->list[index]->get_c_wrapper();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

int rs2_get_stream_profiles_count(const rs2_stream_profile_list* list, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(list);
    return static_cast<int>(list->list.size());
}
HANDLE_EXCEPTIONS_AND_RETURN(0, list)

void rs2_delete_stream_profiles_list(rs2_stream_profile_list* list) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(list);
    delete list;
}
NOEXCEPT_RETURN(, list)


std::ostream & operator<<( std::ostream & os, rs2_stream_profile const & p )
{
    if( ! p.profile )
    {
        os << "NULL";
    }
    else
    {
        os << "[ " << librealsense::get_abbr_string( p.profile->get_stream_type() );
        os << " " << librealsense::get_string( p.profile->get_format() );
        os << " " << p.profile->get_stream_index();
        if( auto vsp = As< video_stream_profile, stream_profile_interface >( p.profile ) )
        {
            os << " " << vsp->get_width();
            os << "x" << vsp->get_height();
        }
        os << " @ " << p.profile->get_framerate();
        os << " ]";
    }
    return os;
}


void rs2_get_video_stream_intrinsics(const rs2_stream_profile* profile, rs2_intrinsics* intr, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( profile );
    VALIDATE_NOT_NULL(intr);

    auto vid = VALIDATE_INTERFACE( profile->profile, librealsense::video_stream_profile_interface);

    *intr = vid->get_intrinsics();
}
EXPECTED_EXCEPTION( not_implemented_exception, , profile, output_arg( intr ) )  // only accepted way of checking if profile has intrinsics
HANDLE_EXCEPTIONS_AND_RETURN(, profile, output_arg( intr ) )

// librealsense wrapper around a C function
class calibration_change_callback : public rs2_calibration_change_callback
{
    rs2_calibration_change_callback_ptr _callback;
    void* _user_arg;

public:
    calibration_change_callback( rs2_calibration_change_callback_ptr callback, void* user_arg )
        : _callback( callback ), _user_arg( user_arg ) {}

    void on_calibration_change( rs2_calibration_status status ) noexcept override
    {
        if( _callback )
        {
            try
            {
                _callback( status, _user_arg );
            }
            catch( ... )
            {
                std::cerr << "Received an execption from profile intrinsics callback!" << std::endl;
            }
        }
    }
    void release() override
    {
        // Shouldn't get called...
        throw std::runtime_error( "calibration_change_callback::release() ?!?!?!" );
    }
};

void rs2_register_calibration_change_callback( rs2_device* dev, rs2_calibration_change_callback_ptr callback, void * user, rs2_error** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( dev );
    VALIDATE_NOT_NULL( callback );

    auto d2r = VALIDATE_INTERFACE( dev->device, librealsense::calibration_change_device );

    // Wrap the C function with a callback interface that will get deleted when done
    d2r->register_calibration_change_callback(
        std::make_shared< calibration_change_callback >( callback, user )
        );
}
HANDLE_EXCEPTIONS_AND_RETURN( , dev, callback, user )

void rs2_register_calibration_change_callback_cpp( rs2_device* dev, rs2_calibration_change_callback* callback, rs2_error** error ) BEGIN_API_CALL
{
    // Take ownership of the callback ASAP or else memory leaks could result if we throw! (the caller usually does a
    // 'new' when calling us)
    VALIDATE_NOT_NULL( callback );
    rs2_calibration_change_callback_sptr callback_ptr{ callback,
                                                       []( rs2_calibration_change_callback * p )
                                                       {
                                                           p->release();
                                                       } };

    VALIDATE_NOT_NULL( dev );

    auto d2r = VALIDATE_INTERFACE( dev->device, librealsense::calibration_change_device );
    d2r->register_calibration_change_callback( callback_ptr );
}
HANDLE_EXCEPTIONS_AND_RETURN( , dev, callback )

void rs2_trigger_device_calibration( rs2_device * dev, rs2_calibration_type type, rs2_error** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( dev );
    
    auto cal = VALIDATE_INTERFACE( dev->device, librealsense::device_calibration );

    cal->trigger_device_calibration( type );
}
HANDLE_EXCEPTIONS_AND_RETURN( , dev, type )

void rs2_get_motion_intrinsics(const rs2_stream_profile* mode, rs2_motion_device_intrinsic * intrinsics, rs2_error ** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(mode);
    VALIDATE_NOT_NULL(intrinsics);

    auto motion = VALIDATE_INTERFACE(mode->profile, librealsense::motion_stream_profile_interface);
    *intrinsics = motion->get_intrinsics();
}
HANDLE_EXCEPTIONS_AND_RETURN(, mode, intrinsics)


void rs2_get_video_stream_resolution(const rs2_stream_profile* from, int* width, int* height, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(from);

    auto vid = VALIDATE_INTERFACE(from->profile, librealsense::video_stream_profile_interface);

    if (width)  *width = vid->get_width();
    if (height) *height = vid->get_height();
}
HANDLE_EXCEPTIONS_AND_RETURN(, from, width, height)

int rs2_is_stream_profile_default(const rs2_stream_profile* profile, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(profile);
    return profile->profile->get_tag() & profile_tag::PROFILE_TAG_DEFAULT ? 1 : 0;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, profile)

void rs2_get_stream_profile_data(const rs2_stream_profile* profile, rs2_stream* stream, rs2_format* format, int* index, int* unique_id, int* framerate, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(profile);
    VALIDATE_NOT_NULL(stream);
    VALIDATE_NOT_NULL(format);
    VALIDATE_NOT_NULL(index);
    VALIDATE_NOT_NULL(unique_id);

    *framerate = profile->profile->get_framerate();
    *format = profile->profile->get_format();
    *index = profile->profile->get_stream_index();
    *stream = profile->profile->get_stream_type();
    *unique_id = profile->profile->get_unique_id();
}
HANDLE_EXCEPTIONS_AND_RETURN(, profile, stream, format, index, framerate)

void rs2_set_stream_profile_data(rs2_stream_profile* mode, rs2_stream stream, int index, rs2_format format, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(mode);
    VALIDATE_ENUM(stream);
    VALIDATE_ENUM(format);

    mode->profile->set_format(format);
    mode->profile->set_stream_type(stream);
    mode->profile->set_stream_index(index);
}
HANDLE_EXCEPTIONS_AND_RETURN(, mode, stream, format)

void rs2_delete_stream_profile(rs2_stream_profile* p) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(p);
    delete p;
}
NOEXCEPT_RETURN(, p)

rs2_stream_profile* rs2_clone_stream_profile(const rs2_stream_profile* mode, rs2_stream stream, int stream_idx, rs2_format fmt, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(mode);
    VALIDATE_ENUM(stream);
    VALIDATE_ENUM(fmt);

    auto sp = mode->profile->clone();
    sp->set_stream_type(stream);
    sp->set_stream_index(stream_idx);
    sp->set_format(fmt);
    return new rs2_stream_profile{ sp.get(), sp };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, mode, stream, stream_idx, fmt)

rs2_stream_profile* rs2_clone_video_stream_profile(const rs2_stream_profile* mode, rs2_stream stream, int index, rs2_format format, int width, int height, const rs2_intrinsics* intr, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(mode);
    VALIDATE_ENUM(stream);
    VALIDATE_ENUM(format);
    VALIDATE_NOT_NULL(intr);

    auto sp = mode->profile->clone();
    sp->set_stream_type(stream);
    sp->set_stream_index(index);
    sp->set_format(format);

    auto vid = std::dynamic_pointer_cast<video_stream_profile_interface>(sp);
    auto i = *intr;

    VALIDATE_NOT_NULL(vid)
    vid->set_intrinsics([i]() { return i; });
    vid->set_dims(width, height);

    return new rs2_stream_profile{ sp.get(), sp };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, mode, stream, index, format, width, height, intr)

const rs2_raw_data_buffer* rs2_build_debug_protocol_command(rs2_device* device, unsigned opcode, unsigned param1, unsigned param2,
    unsigned param3, unsigned param4, void* data, unsigned size_of_data, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);

    auto debug_interface = VALIDATE_INTERFACE(device->device, librealsense::debug_interface);
    auto ret_data = debug_interface->build_command(opcode, param1, param2, param3, param4, static_cast<uint8_t*>(data), size_of_data);
    
    return new rs2_raw_data_buffer{ std::move(ret_data) };

}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device)

const rs2_raw_data_buffer* rs2_send_and_receive_raw_data(rs2_device* device, void* raw_data_to_send, unsigned size_of_raw_data_to_send, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);

    auto debug_interface = VALIDATE_INTERFACE(device->device, librealsense::debug_interface);

    auto raw_data_buffer = static_cast<uint8_t*>(raw_data_to_send);
    std::vector<uint8_t> buffer_to_send(raw_data_buffer, raw_data_buffer + size_of_raw_data_to_send);
    auto ret_data = debug_interface->send_receive_raw_data(buffer_to_send);
    return new rs2_raw_data_buffer{ std::move(ret_data) };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device)

const unsigned char* rs2_get_raw_data(const rs2_raw_data_buffer* buffer, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(buffer);
    return buffer->buffer.data();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, buffer)

int rs2_get_raw_data_size(const rs2_raw_data_buffer* buffer, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(buffer);
    return static_cast<int>(buffer->buffer.size());
}
HANDLE_EXCEPTIONS_AND_RETURN(0, buffer)

void rs2_delete_raw_data(const rs2_raw_data_buffer* buffer) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(buffer);
    delete buffer;
}
NOEXCEPT_RETURN(, buffer)

void rs2_open(rs2_sensor* sensor, const rs2_stream_profile* profile, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    VALIDATE_NOT_NULL(profile);

    std::vector<std::shared_ptr<stream_profile_interface>> request;
    request.push_back(std::dynamic_pointer_cast<stream_profile_interface>(profile->profile->shared_from_this()));
    sensor->sensor->open(request);
}
HANDLE_EXCEPTIONS_AND_RETURN(, sensor, profile)

void rs2_open_multiple(rs2_sensor* sensor,
    const rs2_stream_profile** profiles, int count, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    VALIDATE_NOT_NULL(profiles);

    std::vector<std::shared_ptr<stream_profile_interface>> request;
    for (auto i = 0; i < count; i++)
    {
        request.push_back(std::dynamic_pointer_cast<stream_profile_interface>(profiles[i]->profile->shared_from_this()));
    }
    sensor->sensor->open(request);
}
HANDLE_EXCEPTIONS_AND_RETURN(, sensor, profiles, count)

void rs2_close(const rs2_sensor* sensor, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    sensor->sensor->close();
}
HANDLE_EXCEPTIONS_AND_RETURN(, sensor)

int rs2_is_option_read_only(const rs2_options* options, rs2_option option, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(options);
    return options->options->get_option(option).is_read_only();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, options, option)

float rs2_get_option(const rs2_options* options, rs2_option option_id, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(options);
    VALIDATE_OPTION_ENABLED(options, option_id);
    auto & option = options->options->get_option( option_id );
    switch( option.get_value_type() )
    {
    case RS2_OPTION_TYPE_FLOAT:
    case RS2_OPTION_TYPE_INTEGER:
        return option.query();

    case RS2_OPTION_TYPE_BOOLEAN:
        return (float)option.get_value().get< bool >();

    case RS2_OPTION_TYPE_STRING: {
        // We can convert "enum" options to a float value
        auto r = option.get_range();
        if( r.min == 0.f && r.step == 1.f )
        {
            json value = option.get_value();
            for( auto i = 0.f; i <= r.max; i += r.step )
            {
                auto desc = option.get_value_description( i );
                if( ! desc )
                    break;
                if( value == desc )
                    return i;
            }
        }
        throw not_implemented_exception( "use rs2_get_option_value to get string values" );
    }

    case RS2_OPTION_TYPE_RECT:
        throw not_implemented_exception( "use rs2_get_option_value to get rect values" );
    }
    return option.query();
}
HANDLE_EXCEPTIONS_AND_RETURN(0.f, options, option_id)

rs2_option_value const * rs2_get_option_value( const rs2_options * options, rs2_option option_id, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( options );
    auto & option = options->options->get_option( option_id );  // throws
    std::shared_ptr< const json > value;
    if( option.is_enabled() )
        value = std::make_shared< const json >( option.get_value() );
    auto wrapper = new rs2_option_value_wrapper( option_id, option.get_value_type(), value );
    return wrapper;
}
HANDLE_EXCEPTIONS_AND_RETURN( nullptr, options, option_id )

void rs2_delete_option_value( rs2_option_value const * p_value ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( p_value );
    auto wrapper = static_cast< rs2_option_value_wrapper const * >( p_value );
    if( wrapper && ! --wrapper->ref_count )
        delete wrapper;
}
NOEXCEPT_RETURN( , p_value )

void rs2_set_option(const rs2_options* options, rs2_option option, float value, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(options);
    VALIDATE_OPTION_ENABLED(options, option);
    auto& option_ref = options->options->get_option(option);
    auto range = option_ref.get_range();
    switch (option_ref.get_value_type())
    {
    case RS2_OPTION_TYPE_FLOAT:
        if (range.min != range.max && range.step)
            VALIDATE_RANGE(value, range.min, range.max);
        option_ref.set(value);
        break;

    case RS2_OPTION_TYPE_INTEGER:
        if (range.min != range.max && range.step)
            VALIDATE_RANGE(value, range.min, range.max);
        if ((int)value != value)
            throw invalid_value_exception(rsutils::string::from() << "not an integer: " << value);
        option_ref.set(value);
        break;

    case RS2_OPTION_TYPE_BOOLEAN:
        if (value == 0.f)
            option_ref.set_value(false);
        else if (value == 1.f)
            option_ref.set_value(true);
        else
            throw invalid_value_exception(rsutils::string::from() << "not a boolean: " << value);
        break;

    case RS2_OPTION_TYPE_STRING:
        // We can convert "enum" options to a float value
        if ((int)value == value && range.min == 0.f && range.step == 1.f)
        {
            auto desc = option_ref.get_value_description(value);
            if (desc)
            {
                option_ref.set_value(desc);
                break;
            }
        }
        throw not_implemented_exception("use rs2_set_option_value to set string values");
    }
}
HANDLE_EXCEPTIONS_AND_RETURN(, options, option, value)

void rs2_set_option_value( rs2_options const * options, rs2_option_value const * option_value, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( options );
    VALIDATE_NOT_NULL( option_value );
    auto & option = options->options->get_option( option_value->id );  // throws
    if( ! option_value->is_valid )
    {
        option.set_value( rsutils::null_json );
        return;
    }
    rs2_option_type const option_type = option.get_value_type();
    if( option_value->type != option_type )
        throw invalid_value_exception( "expected " + get_string( option_type ) + " type" );
    auto range = option.get_range();
    switch( option_type )
    {
    case RS2_OPTION_TYPE_FLOAT:
        VALIDATE_RANGE( option_value->as_float, range.min, range.max );
        option.set_value( option_value->as_float );
        break;

    case RS2_OPTION_TYPE_INTEGER:
        VALIDATE_RANGE( option_value->as_integer, range.min, range.max );
        option.set_value( option_value->as_integer );
        break;

    case RS2_OPTION_TYPE_BOOLEAN:
        VALIDATE_RANGE( option_value->as_integer, range.min, range.max );
        option.set_value( (bool)option_value->as_integer );
        break;

    case RS2_OPTION_TYPE_STRING:
        option.set_value( option_value->as_string );
        break;

    case RS2_OPTION_TYPE_RECT:
        option.set_value( json::array( { option_value->as_rect.x1,
                                         option_value->as_rect.y1,
                                         option_value->as_rect.x2,
                                         option_value->as_rect.y2 } ) );
        break;

    default:
        throw not_implemented_exception( "unexpected option type " + get_string( option_type ) );
    }
}
HANDLE_EXCEPTIONS_AND_RETURN( , options, option_value )

rs2_options_list* rs2_get_options_list(const rs2_options* options, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(options);
    rsutils::deferred bulk_op;
    if( auto sensor = dynamic_cast<sensor_base *>(options->options) )
        bulk_op = sensor->bulk_operation();
    auto rs2_list = new rs2_options_list;
    auto option_ids = options->options->get_supported_options();
    rs2_list->list.reserve( option_ids.size() );
    for( auto option_id : option_ids )
    {
        auto & option = options->options->get_option( option_id );
        std::shared_ptr< const json > value;
        if( option.is_enabled() )
            value = std::make_shared< const json >( option.get_value() );
        auto wrapper = new rs2_option_value_wrapper( option_id, option.get_value_type(), value );
        rs2_list->list.push_back( wrapper );
    }
    return rs2_list;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, options)

const char* rs2_get_option_name(const rs2_options* options,rs2_option option, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(options);
    return options->options->get_option_name(option).c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, option, options)

int rs2_get_options_list_size(const rs2_options_list* options, rs2_error** error)BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(options);
    return int(options->list.size());
}
HANDLE_EXCEPTIONS_AND_RETURN(0, options)

rs2_option rs2_get_option_from_list(const rs2_options_list* options, int i, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(options);
    return options->list.at( i )->id;
}
HANDLE_EXCEPTIONS_AND_RETURN(RS2_OPTION_COUNT, options, i)

rs2_option_value const * rs2_get_option_value_from_list( const rs2_options_list * options, int i, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( options );
    auto const p_option_value = options->list.at( i );
    ++p_option_value->ref_count;
    return p_option_value;
}
HANDLE_EXCEPTIONS_AND_RETURN( nullptr, options, i )

void rs2_delete_options_list(rs2_options_list* list) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(list);
    for( auto wrapper : list->list )
        rs2_delete_option_value( wrapper );
    delete list;
}
NOEXCEPT_RETURN(, list)

int rs2_supports_option(const rs2_options* options, rs2_option option, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(options);
    return options->options->supports_option(option);
}
HANDLE_EXCEPTIONS_AND_RETURN(0, options, option)

void rs2_get_option_range(const rs2_options* options, rs2_option option,
    float* min, float* max, float* step, float* def, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(options);
    VALIDATE_NOT_NULL(min);
    VALIDATE_NOT_NULL(max);
    VALIDATE_NOT_NULL(step);
    VALIDATE_NOT_NULL(def);
    auto range = options->options->get_option(option).get_range();
    *min = range.min;
    *max = range.max;
    *def = range.def;
    *step = range.step;
}
HANDLE_EXCEPTIONS_AND_RETURN(, options, option, min, max, step, def)

const char* rs2_get_device_info(const rs2_device* dev, rs2_camera_info info, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_ENUM(info);
    if (dev->device->supports_info(info))
    {
        return dev->device->get_info(info).c_str();
    }
    throw librealsense::invalid_value_exception( rsutils::string::from() << "info " << rs2_camera_info_to_string( info )
                                                                         << " not supported by the device!" );
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, dev, info)

int rs2_supports_device_info(const rs2_device* dev, rs2_camera_info info, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(dev->device);
    VALIDATE_ENUM(info);
    return dev->device->supports_info(info);
}
HANDLE_EXCEPTIONS_AND_RETURN(false, dev, info)

const char* rs2_get_sensor_info(const rs2_sensor* sensor, rs2_camera_info info, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    VALIDATE_ENUM(info);
    if (sensor->sensor->supports_info(info))
    {
        return sensor->sensor->get_info(info).c_str();
    }
    throw librealsense::invalid_value_exception( rsutils::string::from() << "info " << rs2_camera_info_to_string( info )
                                                                         << " not supported by the sensor!" );
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, sensor, info)

int rs2_supports_sensor_info(const rs2_sensor* sensor, rs2_camera_info info, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    VALIDATE_ENUM(info);
    return sensor->sensor->supports_info(info);
}
HANDLE_EXCEPTIONS_AND_RETURN(false, sensor, info)


rs2_frame_callback_sptr make_user_frame_callback( rs2_frame_callback_ptr on_frame, void * user )
{
    return librealsense::make_frame_callback(
        [on_frame, user]( frame_interface * f )
        {
            try
            {
                on_frame( (rs2_frame *)f, user );
            }
            catch( ... )
            {
                LOG_ERROR( "Received an exception from frame callback!" );
            }
        } );
}


void rs2_start(const rs2_sensor* sensor, rs2_frame_callback_ptr on_frame, void* user, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    VALIDATE_NOT_NULL(on_frame);
    auto callback = make_user_frame_callback( on_frame, user );
    sensor->sensor->start(std::move(callback));
}
HANDLE_EXCEPTIONS_AND_RETURN(, sensor, on_frame, user)

void rs2_start_queue(const rs2_sensor* sensor, rs2_frame_queue* queue, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    VALIDATE_NOT_NULL(queue);
    auto callback = make_user_frame_callback( rs2_enqueue_frame, queue );
    sensor->sensor->start(std::move(callback));
}
HANDLE_EXCEPTIONS_AND_RETURN(, sensor, queue)


class notifications_callback : public rs2_notifications_callback
{
    rs2_notification_callback_ptr nptr;
    void * user;

public:
    notifications_callback( rs2_notification_callback_ptr on_notification, void * user )
        : nptr( on_notification )
        , user( user )
    {
    }

    void on_notification( rs2_notification * notification ) override
    {
        if( nptr )
        {
            try
            {
                nptr( notification, user );
            }
            catch( ... )
            {
                LOG_ERROR( "Received an exception from notification callback!" );
            }
        }
    }
 
    void release() override { delete this; }
};


void rs2_set_notifications_callback(const rs2_sensor* sensor, rs2_notification_callback_ptr on_notification, void* user, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    VALIDATE_NOT_NULL(on_notification);
    rs2_notifications_callback_sptr callback(
        new notifications_callback(on_notification, user),
        [](rs2_notifications_callback* p) { delete p; });
    sensor->sensor->register_notifications_callback(std::move(callback));
}
HANDLE_EXCEPTIONS_AND_RETURN(, sensor, on_notification, user)


class software_device_destruction_callback : public rs2_software_device_destruction_callback
{
    rs2_software_device_destruction_callback_ptr nptr;
    void * user;

public:
    software_device_destruction_callback( rs2_software_device_destruction_callback_ptr on_destruction,
                                          void * user )
        : nptr( on_destruction )
        , user( user )
    {
    }

    void on_destruction() override
    {
        if( nptr )
        {
            try
            {
                nptr( user );
            }
            catch( ... )
            {
                LOG_ERROR( "Received an exception from software device destruction callback!" );
            }
        }
    }

    void release() override { delete this; }
};


void rs2_software_device_set_destruction_callback(const rs2_device* dev, rs2_software_device_destruction_callback_ptr on_destruction, void* user, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    auto swdev = VALIDATE_INTERFACE(dev->device, librealsense::software_device);
    VALIDATE_NOT_NULL(on_destruction);
    rs2_software_device_destruction_callback_sptr callback(
        new software_device_destruction_callback( on_destruction, user ),
        [](rs2_software_device_destruction_callback* p) { delete p; });
    swdev->register_destruction_callback(std::move(callback));
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, on_destruction, user)


void rs2_set_devices_changed_callback(const rs2_context* context, rs2_devices_changed_callback_ptr callback, void* user, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(context);
    VALIDATE_NOT_NULL(callback);
    context->devices_changed_subscription = context->ctx->on_device_changes(
        [ctx = context->ctx, callback, user]( std::vector< std::shared_ptr< device_info > > const & removed,
                                              std::vector< std::shared_ptr< device_info > > const & added )
        {
            try
            {
                callback( new rs2_device_list{ ctx, removed }, new rs2_device_list{ ctx, added }, user );
            }
            catch( std::exception const & e )
            {
                LOG_ERROR( "Exception thrown from user devices-changed callback: " << e.what() );
            }
            catch( ... )
            {
                LOG_ERROR( "Exception thrown from user devices-changed callback!" );
            }
        } );
}
HANDLE_EXCEPTIONS_AND_RETURN(, context, callback, user)

void rs2_start_cpp(const rs2_sensor* sensor, rs2_frame_callback* callback, rs2_error** error) BEGIN_API_CALL
{
    // Take ownership of the callback ASAP or else memory leaks could result if we throw! (the caller usually does a
    // 'new' when calling us)
    VALIDATE_NOT_NULL( callback );
    rs2_frame_callback_sptr callback_ptr{ callback,
                                          []( rs2_frame_callback * p )
                                          {
                                              p->release();
                                          } };

    VALIDATE_NOT_NULL(sensor);
    sensor->sensor->start( callback_ptr );
}
HANDLE_EXCEPTIONS_AND_RETURN(, sensor, callback)

void rs2_set_notifications_callback_cpp(const rs2_sensor* sensor, rs2_notifications_callback* callback, rs2_error** error) BEGIN_API_CALL
{
    // Take ownership of the callback ASAP or else memory leaks could result if we throw! (the caller usually does a
    // 'new' when calling us)
    VALIDATE_NOT_NULL( callback );
    rs2_notifications_callback_sptr callback_ptr{ callback,
                                                  []( rs2_notifications_callback * p )
                                                  {
                                                      p->release();
                                                  } };

    VALIDATE_NOT_NULL(sensor);
    sensor->sensor->register_notifications_callback( callback_ptr );
}
HANDLE_EXCEPTIONS_AND_RETURN(, sensor, callback)

void rs2_software_device_set_destruction_callback_cpp(const rs2_device* dev, rs2_software_device_destruction_callback* callback, rs2_error** error) BEGIN_API_CALL
{
    // Take ownership of the callback ASAP or else memory leaks could result if we throw! (the caller usually does a
    // 'new' when calling us)
    VALIDATE_NOT_NULL( callback );
    rs2_software_device_destruction_callback_sptr callback_ptr{ callback,
                                                                []( rs2_software_device_destruction_callback * p )
                                                                {
                                                                    p->release();
                                                                } };

    VALIDATE_NOT_NULL(dev);
    auto swdev = VALIDATE_INTERFACE(dev->device, librealsense::software_device);
    swdev->register_destruction_callback( callback_ptr );
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, callback)

void rs2_set_devices_changed_callback_cpp(rs2_context* context, rs2_devices_changed_callback* callback, rs2_error** error) BEGIN_API_CALL
{
    // Take ownership of the callback ASAP or else memory leaks could result if we throw! (the caller usually does a
    // 'new' when calling us)
    VALIDATE_NOT_NULL( callback );
    rs2_devices_changed_callback_sptr cb{ callback,
                                          []( rs2_devices_changed_callback * p )
                                          {
                                              p->release();
                                          } };

    VALIDATE_NOT_NULL(context);
    context->devices_changed_subscription = context->ctx->on_device_changes(
        [ctx = context->ctx, cb]( std::vector< std::shared_ptr< device_info > > const & removed,
                                  std::vector< std::shared_ptr< device_info > > const & added )
    {
        try
        {
            cb->on_devices_changed( new rs2_device_list{ ctx, removed },
                                    new rs2_device_list{ ctx, added } );
        }
        catch( std::exception const & e )
        {
            LOG_ERROR( "Exception thrown from user callback handler: " << e.what() );
        }
    } );
}
HANDLE_EXCEPTIONS_AND_RETURN(, context, callback)

void rs2_stop(const rs2_sensor* sensor, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    sensor->sensor->stop();
}
HANDLE_EXCEPTIONS_AND_RETURN(, sensor)

int rs2_supports_frame_metadata(const rs2_frame* frame, rs2_frame_metadata_value frame_metadata, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame);
    VALIDATE_ENUM(frame_metadata);
    return ((frame_interface*)frame)->find_metadata( frame_metadata, nullptr );
}
HANDLE_EXCEPTIONS_AND_RETURN(0, frame, frame_metadata)

rs2_metadata_type rs2_get_frame_metadata(const rs2_frame* frame, rs2_frame_metadata_value frame_metadata, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame);
    VALIDATE_ENUM(frame_metadata);
    auto frame_ifc = (frame_interface *)frame;
    rs2_metadata_type value;
    if( frame_ifc->find_metadata( frame_metadata, &value ) )
        return value;
    throw invalid_value_exception( rsutils::string::from()
                                   << get_string( frame_ifc->get_stream()->get_stream_type() )
                                   << " frame does not support metadata \"" << get_string( frame_metadata ) << "\"" );
}
HANDLE_EXCEPTIONS_AND_RETURN(0, frame, frame_metadata)

const char* rs2_get_notification_description(rs2_notification* notification, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(notification);
    return notification->_notification->description.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, notification)

rs2_time_t rs2_get_notification_timestamp(rs2_notification* notification, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(notification);
    return notification->_notification->timestamp;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, notification)

rs2_log_severity rs2_get_notification_severity(rs2_notification* notification, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(notification);
    return (rs2_log_severity)notification->_notification->severity;
}
HANDLE_EXCEPTIONS_AND_RETURN(RS2_LOG_SEVERITY_NONE, notification)

rs2_notification_category rs2_get_notification_category(rs2_notification* notification, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(notification);
    return (rs2_notification_category)notification->_notification->category;
}
HANDLE_EXCEPTIONS_AND_RETURN(RS2_NOTIFICATION_CATEGORY_UNKNOWN_ERROR, notification)

const char* rs2_get_notification_serialized_data(rs2_notification* notification, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(notification);
    return notification->_notification->serialized_data.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, notification)


int rs2_device_list_contains(const rs2_device_list* info_list, const rs2_device* device, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(info_list);
    VALIDATE_NOT_NULL(device);

    auto dev_info = device->device->get_device_info();
    if( ! dev_info )
        return 0;

    for (auto info : info_list->list)
    {
        if( dev_info->is_same_as( info ) )
            return 1;
    }
    return 0;
}
HANDLE_EXCEPTIONS_AND_RETURN(false, info_list, device)

rs2_time_t rs2_get_frame_timestamp(const rs2_frame* frame_ref, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame_ref);
    return ((frame_interface*)frame_ref)->get_frame_timestamp();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, frame_ref)

rs2_timestamp_domain rs2_get_frame_timestamp_domain(const rs2_frame* frame_ref, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame_ref);
    return ((frame_interface*)frame_ref)->get_frame_timestamp_domain();
}
HANDLE_EXCEPTIONS_AND_RETURN(RS2_TIMESTAMP_DOMAIN_COUNT, frame_ref)

rs2_sensor* rs2_get_frame_sensor(const rs2_frame* frame, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame);
    std::shared_ptr<librealsense::sensor_interface> sensor( ((frame_interface*)frame)->get_sensor() );
    device_interface& dev = sensor->get_device();
    return new rs2_sensor( dev.shared_from_this(), sensor.get());
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, frame)

int rs2_get_frame_data_size(const rs2_frame* frame_ref, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame_ref);
    return ((frame_interface*)frame_ref)->get_frame_data_size();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, frame_ref)

const void* rs2_get_frame_data(const rs2_frame* frame_ref, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame_ref);
    return ((frame_interface*)frame_ref)->get_frame_data();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, frame_ref)

int rs2_get_frame_width(const rs2_frame* frame_ref, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame_ref);
    auto vf = VALIDATE_INTERFACE(((frame_interface*)frame_ref), librealsense::video_frame);
    return vf->get_width();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, frame_ref)

int rs2_get_frame_height(const rs2_frame* frame_ref, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame_ref);
    auto vf = VALIDATE_INTERFACE(((frame_interface*)frame_ref), librealsense::video_frame);
    return vf->get_height();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, frame_ref)

int rs2_get_frame_stride_in_bytes(const rs2_frame* frame_ref, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame_ref);
    auto vf = VALIDATE_INTERFACE(((frame_interface*)frame_ref), librealsense::video_frame);
    return vf->get_stride();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, frame_ref)

const rs2_stream_profile* rs2_get_frame_stream_profile(const rs2_frame* frame_ref, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame_ref);
    return ((frame_interface*)frame_ref)->get_stream()->get_c_wrapper();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, frame_ref)

const char * rs2_get_stream_profile_name( const rs2_stream_profile * profile, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( profile );
    return profile->profile->get_name();
}
HANDLE_EXCEPTIONS_AND_RETURN( nullptr, profile )

int rs2_get_frame_bits_per_pixel(const rs2_frame* frame_ref, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame_ref);
    auto vf = VALIDATE_INTERFACE(((frame_interface*)frame_ref), librealsense::video_frame);
    return vf->get_bpp();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, frame_ref)

unsigned long long rs2_get_frame_number(const rs2_frame* frame, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame);
    return ((frame_interface*)frame)->get_frame_number();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, frame)

void rs2_release_frame(rs2_frame* frame) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame);
    ((frame_interface*)frame)->release();
}
NOEXCEPT_RETURN(, frame)

void rs2_keep_frame(rs2_frame* frame) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame);
    ((frame_interface*)frame)->keep();
}
NOEXCEPT_RETURN(, frame)

const char* rs2_get_option_description(const rs2_options* options, rs2_option option, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(options);
    return options->options->get_option(option).get_description();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, options, option)

void rs2_frame_add_ref(rs2_frame* frame, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame);
    ((frame_interface*)frame)->acquire();
}
HANDLE_EXCEPTIONS_AND_RETURN(, frame)

const char* rs2_get_option_value_description(const rs2_options* options, rs2_option option, float value, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(options);
    return options->options->get_option(option).get_value_description(value);
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, options, option, value)

static void populate_options_list( rs2_options_list * updated_options_list,
                                   options_watcher::options_and_values const & updated_options )
{
    for( auto & id_value : updated_options )
    {
        options_watcher::option_and_value const & option_and_value = id_value.second;
        updated_options_list->list.push_back(
            new rs2_option_value_wrapper( id_value.first,
                                          option_and_value.sptr->get_value_type(),
                                          option_and_value.p_last_known_value ) );
    }
}

void rs2_set_options_changed_callback( rs2_options * options,
                                       rs2_options_changed_callback_ptr callback,
                                       rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( options );
    VALIDATE_NOT_NULL( callback );
    options->subscription = options->options->register_options_changed_callback(
        [callback]( options_watcher::options_and_values const & updated_options )
        {
            rs2_options_list * updated_options_list = new rs2_options_list(); // Should be on heap if user will choose to save for later use.
            populate_options_list( updated_options_list, updated_options );
            try
            {
                callback( updated_options_list );
            }
            catch( ... )
            {
                LOG_ERROR( "Caught exception from options-changed callback" );
            }
        } );
}
HANDLE_EXCEPTIONS_AND_RETURN( , options, callback )

void rs2_set_options_changed_callback_cpp( rs2_options * options,
                                           rs2_options_changed_callback * callback,
                                           rs2_error ** error ) BEGIN_API_CALL
{
    // Take ownership of the callback ASAP or else memory leaks could result if we throw! (the caller usually does a 'new' when calling us)
    VALIDATE_NOT_NULL( callback );
    rs2_options_changed_callback_sptr cb{ callback,
                                          []( rs2_options_changed_callback * p )
                                          {
                                              p->release();
                                          } };
    VALIDATE_NOT_NULL( options );
    options->subscription = options->options->register_options_changed_callback(
        [cb]( options_watcher::options_and_values const & updated_options )
        {
            rs2_options_list * updated_options_list = new rs2_options_list(); // Should be on heap if user will choose to save for later use.
            populate_options_list( updated_options_list, updated_options );
            try
            {
                cb->on_value_changed( updated_options_list );
            }
            catch( ... )
            {
                LOG_ERROR( "Caught exception from options-changed callback" );
            }
        } );
}
HANDLE_EXCEPTIONS_AND_RETURN( , options, callback )

rs2_frame_queue* rs2_create_frame_queue(int capacity, rs2_error** error) BEGIN_API_CALL
{
    return new rs2_frame_queue(capacity);
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, capacity)

void rs2_delete_frame_queue(rs2_frame_queue* queue) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(queue);
    delete queue;
}
NOEXCEPT_RETURN(, queue)

rs2_frame* rs2_wait_for_frame(rs2_frame_queue* queue, unsigned int timeout_ms, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(queue);
    librealsense::frame_holder fh;
    if (!queue->queue.dequeue(&fh, timeout_ms))
    {
        throw std::runtime_error("Frame did not arrive in time!");
    }

    frame_interface* result = nullptr;
    std::swap(result, fh.frame);
    return (rs2_frame*)result;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, queue)

int rs2_poll_for_frame(rs2_frame_queue* queue, rs2_frame** output_frame, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(queue);
    VALIDATE_NOT_NULL(output_frame);
    librealsense::frame_holder fh;
    if (queue->queue.try_dequeue(&fh))
    {
        frame_interface* result = nullptr;
        std::swap(result, fh.frame);
        *output_frame = (rs2_frame*)result;
        return true;
    }

    return false;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, queue, output_frame)

int rs2_try_wait_for_frame(rs2_frame_queue* queue, unsigned int timeout_ms, rs2_frame** output_frame, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(queue);
    VALIDATE_NOT_NULL(output_frame);
    librealsense::frame_holder fh;
    if (!queue->queue.dequeue(&fh, timeout_ms))
    {
        return false;
    }

    frame_interface* result = nullptr;
    std::swap(result, fh.frame);
    *output_frame = (rs2_frame*)result;
    return true;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, queue, output_frame)

void rs2_enqueue_frame(rs2_frame* frame, void* queue) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame);
    VALIDATE_NOT_NULL(queue);
    auto q = reinterpret_cast<rs2_frame_queue*>(queue);
    librealsense::frame_holder fh;
    fh.frame = (frame_interface*)frame;
    q->queue.enqueue(std::move(fh));
}
NOEXCEPT_RETURN(, frame, queue)

void rs2_flush_queue(rs2_frame_queue* queue, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(queue);
    queue->queue.clear();
}
HANDLE_EXCEPTIONS_AND_RETURN(, queue)

int rs2_frame_queue_size(rs2_frame_queue* queue, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(queue);
    return int(queue->queue.size());
}
HANDLE_EXCEPTIONS_AND_RETURN(0, queue)

void rs2_get_extrinsics(const rs2_stream_profile* from,
    const rs2_stream_profile* to,
    rs2_extrinsics* extrin, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(from);
    VALIDATE_NOT_NULL(to);
    VALIDATE_NOT_NULL(extrin);

    if (!environment::get_instance().get_extrinsics_graph().try_fetch_extrinsics(*from->profile, *to->profile, extrin))
    {
        throw not_implemented_exception("Requested extrinsics are not available!");
    }
}
HANDLE_EXCEPTIONS_AND_RETURN(, from, to, extrin)

void rs2_register_extrinsics(const rs2_stream_profile* from,
    const rs2_stream_profile* to,
    rs2_extrinsics extrin, rs2_error** error)BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(from);
    VALIDATE_NOT_NULL(to);

    environment::get_instance().get_extrinsics_graph().register_extrinsics(*from->profile, *to->profile, extrin);
}
HANDLE_EXCEPTIONS_AND_RETURN(, from, to, extrin)

void rs2_override_extrinsics( const rs2_sensor* sensor, const rs2_extrinsics* extrinsics, rs2_error** error ) BEGIN_API_CALL
{
    throw not_implemented_exception( "deprecated" );
}
HANDLE_EXCEPTIONS_AND_RETURN( , sensor, extrinsics )

void rs2_reset_sensor_calibration( rs2_sensor const * sensor, rs2_error** error ) BEGIN_API_CALL
{
    throw not_implemented_exception( "deprecated" );
}
HANDLE_EXCEPTIONS_AND_RETURN( , sensor )


void rs2_hardware_reset(const rs2_device* device, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    device->device->hardware_reset();
}
HANDLE_EXCEPTIONS_AND_RETURN(, device)

int rs2_is_in_recovery_mode(const rs2_device* device, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(device->device);
    return device->device->is_in_recovery_mode();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, device)

// Verify  and provide API version encoded as integer value
int rs2_get_api_version(rs2_error** error) BEGIN_API_CALL
{
    // Each component type is within [0-99] range
    VALIDATE_RANGE(RS2_API_MAJOR_VERSION, 0, 99);
    VALIDATE_RANGE(RS2_API_MINOR_VERSION, 0, 99);
    VALIDATE_RANGE(RS2_API_PATCH_VERSION, 0, 99);
    return RS2_API_VERSION;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, RS2_API_MAJOR_VERSION, RS2_API_MINOR_VERSION, RS2_API_PATCH_VERSION)

rs2_context* rs2_create_recording_context(int api_version, const char* filename, const char* section, rs2_recording_mode mode, rs2_error** error) BEGIN_API_CALL
{
    throw not_implemented_exception( "deprecated" );
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, api_version, filename, section, mode)

rs2_context* rs2_create_mock_context_versioned(int api_version, const char* filename, const char* section, const char* min_api_version, rs2_error** error) BEGIN_API_CALL
{
    throw not_implemented_exception( "deprecated" );
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, api_version, filename, section)

rs2_context* rs2_create_mock_context(int api_version, const char* filename, const char* section, rs2_error** error) BEGIN_API_CALL
{
    throw not_implemented_exception( "deprecated" );
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, api_version, filename, section)

void rs2_set_region_of_interest(const rs2_sensor* sensor, int min_x, int min_y, int max_x, int max_y, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);

    VALIDATE_LE(min_x, max_x);
    VALIDATE_LE(min_y, max_y);
    VALIDATE_LE(0, min_x);
    VALIDATE_LE(0, min_y);

    auto roi = VALIDATE_INTERFACE(sensor->sensor, librealsense::roi_sensor_interface);
    roi->get_roi_method().set({ min_x, min_y, max_x, max_y });
}
HANDLE_EXCEPTIONS_AND_RETURN(, sensor, min_x, min_y, max_x, max_y)

void rs2_get_region_of_interest(const rs2_sensor* sensor, int* min_x, int* min_y, int* max_x, int* max_y, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    VALIDATE_NOT_NULL(min_x);
    VALIDATE_NOT_NULL(min_y);
    VALIDATE_NOT_NULL(max_x);
    VALIDATE_NOT_NULL(max_y);

    auto roi = VALIDATE_INTERFACE(sensor->sensor, librealsense::roi_sensor_interface);
    auto rect = roi->get_roi_method().get();

    *min_x = rect.min_x;
    *min_y = rect.min_y;
    *max_x = rect.max_x;
    *max_y = rect.max_y;
}
HANDLE_EXCEPTIONS_AND_RETURN(, sensor, min_x, min_y, max_x, max_y)

void rs2_free_error(rs2_error* error) { if (error) delete error; }
const char* rs2_get_failed_function(const rs2_error* error) { return error ? error->function.c_str() : nullptr; }
const char* rs2_get_failed_args(const rs2_error* error) { return error ? error->args.c_str() : nullptr; }
const char* rs2_get_error_message(const rs2_error* error) { return error ? error->message.c_str() : nullptr; }
rs2_exception_type rs2_get_librealsense_exception_type(const rs2_error* error) { return error ? error->exception_type : RS2_EXCEPTION_TYPE_UNKNOWN; }

void rs2_log_to_console(rs2_log_severity min_severity, rs2_error** error) BEGIN_API_CALL
{
    librealsense::log_to_console(min_severity);
}
HANDLE_EXCEPTIONS_AND_RETURN(, min_severity)

void rs2_log_to_file(rs2_log_severity min_severity, const char* file_path, rs2_error** error) BEGIN_API_CALL
{
    librealsense::log_to_file(min_severity, file_path);
}
HANDLE_EXCEPTIONS_AND_RETURN(, min_severity, file_path)

void rs2_log_to_callback_cpp( rs2_log_severity min_severity, rs2_log_callback * callback, rs2_error** error ) BEGIN_API_CALL
{
    // Take ownership of the callback ASAP or else memory leaks could result if we throw! (the caller usually does a
    // 'new' when calling us)
    VALIDATE_NOT_NULL( callback );
    rs2_log_callback_sptr callback_ptr{ callback,
                                        []( rs2_log_callback * p )
                                        {
                                            p->release();
                                        } };

    librealsense::log_to_callback( min_severity, callback_ptr );
}
HANDLE_EXCEPTIONS_AND_RETURN( , min_severity, callback )

void rs2_reset_logger( rs2_error** error) BEGIN_API_CALL
{
    librealsense::reset_logger();
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN_VOID()

void rs2_enable_rolling_log_file( unsigned max_size, rs2_error ** error ) BEGIN_API_CALL
{
    librealsense::enable_rolling_log_file( max_size );
}
HANDLE_EXCEPTIONS_AND_RETURN(, max_size)

// librealsense wrapper around a C function
class on_log_callback : public rs2_log_callback
{
    rs2_log_callback_ptr _on_log;
    void* _user_arg;

public:
    on_log_callback( rs2_log_callback_ptr on_log, void * user_arg ) : _on_log( on_log ), _user_arg( user_arg ) {}

    void on_log( rs2_log_severity severity, rs2_log_message const& msg ) noexcept override
    {
        if( _on_log )
        {
            try
            {
                _on_log( severity, &msg, _user_arg );
            }
            catch( ... )
            {
                std::cerr << "Received an execption from log callback!" << std::endl;
            }
        }
    }
    void release() override
    {
        // Shouldn't get called...
        throw std::runtime_error( "on_log_callback::release() ?!?!?!" );
        delete this;
    }
};

void rs2_log_to_callback( rs2_log_severity min_severity, rs2_log_callback_ptr on_log, void * arg, rs2_error** error ) BEGIN_API_CALL
{
    // Wrap the C function with a callback interface that will get deleted when done
    librealsense::log_to_callback( min_severity,
                                   rs2_log_callback_sptr{ new on_log_callback( on_log, arg ) }
    );
}
HANDLE_EXCEPTIONS_AND_RETURN( , min_severity, on_log, arg )


unsigned rs2_get_log_message_line_number( rs2_log_message const* msg, rs2_error** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( msg );
    log_message const& wrapper = *(log_message const*) (msg);
    return wrapper.get_log_message_line_number();
}
HANDLE_EXCEPTIONS_AND_RETURN( 0, msg )

const char* rs2_get_log_message_filename( rs2_log_message const* msg, rs2_error** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( msg );
    log_message const& wrapper = *(log_message const*) (msg);
    return wrapper.get_log_message_filename();
}
HANDLE_EXCEPTIONS_AND_RETURN( nullptr, msg )

const char* rs2_get_raw_log_message( rs2_log_message const* msg, rs2_error** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( msg );
    log_message const & wrapper = *( log_message const * )( msg );
    return wrapper.get_raw_log_message();
}
HANDLE_EXCEPTIONS_AND_RETURN( nullptr, msg )

const char* rs2_get_full_log_message( rs2_log_message const* msg, rs2_error** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( msg );
    log_message & wrapper = *( log_message * )( msg );
    return wrapper.get_full_log_message();
}
HANDLE_EXCEPTIONS_AND_RETURN( nullptr, msg )


int rs2_is_sensor_extendable_to(const rs2_sensor* sensor, rs2_extension extension_type, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    VALIDATE_ENUM(extension_type);
    switch (extension_type)
    {
    case RS2_EXTENSION_DEBUG                   : return VALIDATE_INTERFACE_NO_THROW(sensor->sensor, librealsense::debug_interface)        != nullptr;
    case RS2_EXTENSION_INFO                    : return VALIDATE_INTERFACE_NO_THROW(sensor->sensor, librealsense::info_interface)         != nullptr;
    case RS2_EXTENSION_OPTIONS                 : return VALIDATE_INTERFACE_NO_THROW(sensor->sensor, librealsense::options_interface)      != nullptr;
    case RS2_EXTENSION_VIDEO                   : return VALIDATE_INTERFACE_NO_THROW(sensor->sensor, librealsense::video_sensor_interface) != nullptr;
    case RS2_EXTENSION_ROI                     : return VALIDATE_INTERFACE_NO_THROW(sensor->sensor, librealsense::roi_sensor_interface)   != nullptr;
    case RS2_EXTENSION_DEPTH_SENSOR            : return VALIDATE_INTERFACE_NO_THROW(sensor->sensor, librealsense::depth_sensor)           != nullptr;
    case RS2_EXTENSION_DEPTH_STEREO_SENSOR     : return VALIDATE_INTERFACE_NO_THROW(sensor->sensor, librealsense::depth_stereo_sensor)    != nullptr;
    case RS2_EXTENSION_SOFTWARE_SENSOR         : return VALIDATE_INTERFACE_NO_THROW(sensor->sensor, librealsense::software_sensor)        != nullptr;
    case RS2_EXTENSION_POSE_SENSOR             : return VALIDATE_INTERFACE_NO_THROW(sensor->sensor, librealsense::pose_sensor_interface)  != nullptr;
    case RS2_EXTENSION_WHEEL_ODOMETER          : return VALIDATE_INTERFACE_NO_THROW(sensor->sensor, librealsense::wheel_odometry_interface)!= nullptr;
    case RS2_EXTENSION_TM2_SENSOR              : return false;
    case RS2_EXTENSION_COLOR_SENSOR            : return VALIDATE_INTERFACE_NO_THROW(sensor->sensor, librealsense::color_sensor)           != nullptr;
    case RS2_EXTENSION_MOTION_SENSOR           : return VALIDATE_INTERFACE_NO_THROW(sensor->sensor, librealsense::motion_sensor)          != nullptr;
    case RS2_EXTENSION_FISHEYE_SENSOR          : return VALIDATE_INTERFACE_NO_THROW(sensor->sensor, librealsense::fisheye_sensor)         != nullptr;
    case RS2_EXTENSION_CALIBRATED_SENSOR       : return false;
    case RS2_EXTENSION_MAX_USABLE_RANGE_SENSOR : return VALIDATE_INTERFACE_NO_THROW(sensor->sensor, librealsense::max_usable_range_sensor)!= nullptr;
    case RS2_EXTENSION_DEBUG_STREAM_SENSOR     : return VALIDATE_INTERFACE_NO_THROW(sensor->sensor, librealsense::debug_stream_sensor )   != nullptr;
    case RS2_EXTENSION_SAFETY_SENSOR           : return VALIDATE_INTERFACE_NO_THROW(sensor->sensor, librealsense::safety_sensor)          != nullptr;
    case RS2_EXTENSION_DEPTH_MAPPING_SENSOR    : return VALIDATE_INTERFACE_NO_THROW(sensor->sensor, librealsense::depth_mapping_sensor)   != nullptr;

    default:
        return false;
    }
}
HANDLE_EXCEPTIONS_AND_RETURN(0, sensor, extension_type)

int rs2_is_device_extendable_to(const rs2_device* dev, rs2_extension extension, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_ENUM(extension);
    switch (extension)
    {
        case RS2_EXTENSION_DEBUG                 : return VALIDATE_INTERFACE_NO_THROW(dev->device, librealsense::debug_interface)               != nullptr;
        case RS2_EXTENSION_INFO                  : return VALIDATE_INTERFACE_NO_THROW(dev->device, librealsense::info_interface)                != nullptr;
        case RS2_EXTENSION_OPTIONS               : return VALIDATE_INTERFACE_NO_THROW(dev->device, librealsense::options_interface)             != nullptr;
        case RS2_EXTENSION_VIDEO                 : return VALIDATE_INTERFACE_NO_THROW(dev->device, librealsense::video_sensor_interface)        != nullptr;
        case RS2_EXTENSION_ROI                   : return VALIDATE_INTERFACE_NO_THROW(dev->device, librealsense::roi_sensor_interface)          != nullptr;
        case RS2_EXTENSION_DEPTH_SENSOR          : return VALIDATE_INTERFACE_NO_THROW(dev->device, librealsense::depth_sensor)                  != nullptr;
        case RS2_EXTENSION_DEPTH_STEREO_SENSOR   : return VALIDATE_INTERFACE_NO_THROW(dev->device, librealsense::depth_stereo_sensor)           != nullptr;
        case RS2_EXTENSION_COLOR_SENSOR          : return VALIDATE_INTERFACE_NO_THROW(dev->device, librealsense::color_sensor)                  != nullptr;
        case RS2_EXTENSION_MOTION_SENSOR         : return VALIDATE_INTERFACE_NO_THROW(dev->device, librealsense::motion_sensor)                 != nullptr;
        case RS2_EXTENSION_FISHEYE_SENSOR        : return VALIDATE_INTERFACE_NO_THROW(dev->device, librealsense::fisheye_sensor)                != nullptr;
        case RS2_EXTENSION_SAFETY_SENSOR         : return VALIDATE_INTERFACE_NO_THROW(dev->device, librealsense::safety_sensor)                 != nullptr;
        case RS2_EXTENSION_ADVANCED_MODE         : return VALIDATE_INTERFACE_NO_THROW(dev->device, librealsense::ds_advanced_mode_interface)    != nullptr;
        case RS2_EXTENSION_DEPTH_MAPPING_SENSOR  : return VALIDATE_INTERFACE_NO_THROW(dev->device, librealsense::depth_mapping_sensor)          != nullptr;
        case RS2_EXTENSION_RECORD                : return VALIDATE_INTERFACE_NO_THROW(dev->device, librealsense::record_device)                 != nullptr;
        case RS2_EXTENSION_PLAYBACK              : return VALIDATE_INTERFACE_NO_THROW(dev->device, librealsense::playback_device)               != nullptr;
        case RS2_EXTENSION_TM2                   : return false;
        case RS2_EXTENSION_UPDATABLE             : return VALIDATE_INTERFACE_NO_THROW(dev->device, librealsense::updatable)                     != nullptr;
        case RS2_EXTENSION_UPDATE_DEVICE         : return VALIDATE_INTERFACE_NO_THROW(dev->device, librealsense::update_device_interface)       != nullptr;
        case RS2_EXTENSION_GLOBAL_TIMER          : return VALIDATE_INTERFACE_NO_THROW(dev->device, librealsense::global_time_interface)         != nullptr;
        case RS2_EXTENSION_AUTO_CALIBRATED_DEVICE: return VALIDATE_INTERFACE_NO_THROW(dev->device, librealsense::auto_calibrated_interface)     != nullptr;
        case RS2_EXTENSION_DEVICE_CALIBRATION    : return VALIDATE_INTERFACE_NO_THROW(dev->device, librealsense::device_calibration)            != nullptr;
        case RS2_EXTENSION_SERIALIZABLE          : return VALIDATE_INTERFACE_NO_THROW(dev->device, librealsense::serializable_interface)        != nullptr;
        case RS2_EXTENSION_FW_LOGGER             : return VALIDATE_INTERFACE_NO_THROW(dev->device, librealsense::firmware_logger_extensions)    != nullptr;
        case RS2_EXTENSION_CALIBRATION_CHANGE_DEVICE: return VALIDATE_INTERFACE_NO_THROW(dev->device, librealsense::calibration_change_device)  != nullptr;
        case RS2_EXTENSION_ETH_CONFIG            : return VALIDATE_INTERFACE_NO_THROW(dev->device, librealsense::eth_config_device )            != nullptr;

        default:
            return false;
    }
}
HANDLE_EXCEPTIONS_AND_RETURN(0, dev, extension)


int rs2_is_frame_extendable_to(const rs2_frame* f, rs2_extension extension_type, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(f);
    VALIDATE_ENUM(extension_type);
    switch (extension_type)
    {
    case RS2_EXTENSION_VIDEO_FRAME              : return VALIDATE_INTERFACE_NO_THROW((frame_interface*)f, librealsense::video_frame)     != nullptr;
    case RS2_EXTENSION_COMPOSITE_FRAME          : return VALIDATE_INTERFACE_NO_THROW((frame_interface*)f, librealsense::composite_frame) != nullptr;
    case RS2_EXTENSION_POINTS                   : return VALIDATE_INTERFACE_NO_THROW((frame_interface*)f, librealsense::points)          != nullptr;
    case RS2_EXTENSION_DEPTH_FRAME              : return VALIDATE_INTERFACE_NO_THROW((frame_interface*)f, librealsense::depth_frame)     != nullptr;
    case RS2_EXTENSION_DISPARITY_FRAME          : return VALIDATE_INTERFACE_NO_THROW((frame_interface*)f, librealsense::disparity_frame) != nullptr;
    case RS2_EXTENSION_MOTION_FRAME             : return VALIDATE_INTERFACE_NO_THROW((frame_interface*)f, librealsense::motion_frame)    != nullptr;
    case RS2_EXTENSION_POSE_FRAME               : return VALIDATE_INTERFACE_NO_THROW((frame_interface*)f, librealsense::pose_frame)      != nullptr;
    case RS2_EXTENSION_LABELED_POINTS         : return VALIDATE_INTERFACE_NO_THROW((frame_interface*)f, librealsense::labeled_points) != nullptr;

    default:
        return false;
    }
}
HANDLE_EXCEPTIONS_AND_RETURN(0, f, extension_type)

int rs2_is_processing_block_extendable_to(const rs2_processing_block* f, rs2_extension extension_type, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(f);
    VALIDATE_ENUM(extension_type);
    switch (extension_type)
    {
    case RS2_EXTENSION_DECIMATION_FILTER: return VALIDATE_INTERFACE_NO_THROW((processing_block_interface*)(f->block.get()), librealsense::decimation_filter) != nullptr;
    case RS2_EXTENSION_ROTATION_FILTER: return VALIDATE_INTERFACE_NO_THROW((processing_block_interface*)(f->block.get()), librealsense::rotation_filter) != nullptr;
    case RS2_EXTENSION_THRESHOLD_FILTER: return VALIDATE_INTERFACE_NO_THROW((processing_block_interface*)(f->block.get()), librealsense::threshold) != nullptr;
    case RS2_EXTENSION_DISPARITY_FILTER: return VALIDATE_INTERFACE_NO_THROW((processing_block_interface*)(f->block.get()), librealsense::disparity_transform) != nullptr;
    case RS2_EXTENSION_SPATIAL_FILTER: return VALIDATE_INTERFACE_NO_THROW((processing_block_interface*)(f->block.get()), librealsense::spatial_filter) != nullptr;
    case RS2_EXTENSION_TEMPORAL_FILTER: return VALIDATE_INTERFACE_NO_THROW((processing_block_interface*)(f->block.get()), librealsense::temporal_filter) != nullptr;
    case RS2_EXTENSION_HOLE_FILLING_FILTER: return VALIDATE_INTERFACE_NO_THROW((processing_block_interface*)(f->block.get()), librealsense::hole_filling_filter) != nullptr;
    case RS2_EXTENSION_ZERO_ORDER_FILTER: throw not_implemented_exception( "deprecated" );
    case RS2_EXTENSION_DEPTH_HUFFMAN_DECODER: throw not_implemented_exception( "deprecated" );
    case RS2_EXTENSION_HDR_MERGE: return VALIDATE_INTERFACE_NO_THROW((processing_block_interface*)(f->block.get()), librealsense::hdr_merge) != nullptr;
    case RS2_EXTENSION_SEQUENCE_ID_FILTER: return VALIDATE_INTERFACE_NO_THROW((processing_block_interface*)(f->block.get()), librealsense::sequence_id_filter) != nullptr;
  
    default:
        return false;
    }
}
HANDLE_EXCEPTIONS_AND_RETURN(0, f, extension_type)

int rs2_is_embedded_filter_extendable_to(const rs2_embedded_filter* embedded_filter, rs2_extension extension_type, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(embedded_filter);
    VALIDATE_ENUM(extension_type);
    switch (extension_type)
    {
    case RS2_EXTENSION_DECIMATION_EMBEDDED_FILTER: return VALIDATE_INTERFACE_NO_THROW((embedded_filter_interface*)(embedded_filter->_embedded_filter.get()), librealsense::decimation_embedded_filter) != nullptr;
    case RS2_EXTENSION_TEMPORAL_EMBEDDED_FILTER: return VALIDATE_INTERFACE_NO_THROW((embedded_filter_interface*)(embedded_filter->_embedded_filter.get()), librealsense::temporal_embedded_filter) != nullptr;
    default:
        return false;
    }
}
HANDLE_EXCEPTIONS_AND_RETURN(0, embedded_filter, extension_type)


int rs2_stream_profile_is(const rs2_stream_profile* profile, rs2_extension extension_type, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(profile);
    VALIDATE_ENUM(extension_type);
    switch (extension_type)
    {
    case RS2_EXTENSION_VIDEO_PROFILE    : return VALIDATE_INTERFACE_NO_THROW(profile->profile, librealsense::video_stream_profile_interface)  != nullptr;
    case RS2_EXTENSION_MOTION_PROFILE   : return VALIDATE_INTERFACE_NO_THROW(profile->profile, librealsense::motion_stream_profile_interface) != nullptr;
    case RS2_EXTENSION_POSE_PROFILE     : return VALIDATE_INTERFACE_NO_THROW(profile->profile, librealsense::pose_stream_profile_interface)   != nullptr;
    default:
        return false;
    }
}
HANDLE_EXCEPTIONS_AND_RETURN(0, profile, extension_type)

rs2_device* rs2_context_add_device(rs2_context* ctx, const char* file, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(ctx);
    VALIDATE_NOT_NULL(file);

    auto dev_info = std::make_shared< playback_device_info >( ctx->ctx, file );
    ctx->ctx->add_device( dev_info );
    return new rs2_device{ dev_info->create_device() };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, ctx, file)

void rs2_context_add_software_device(rs2_context* ctx, rs2_device* dev, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(ctx);
    VALIDATE_NOT_NULL(dev);
    VALIDATE_INTERFACE(dev->device, librealsense::software_device);
    
    auto dev_info = std::make_shared< software_device_info >( ctx->ctx );
    dev_info->set_device( std::dynamic_pointer_cast< software_device >( dev->device ) );
    ctx->ctx->add_device( dev_info );
}
HANDLE_EXCEPTIONS_AND_RETURN(, ctx, dev)

void rs2_context_remove_device(rs2_context* ctx, const char* file, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(ctx);
    VALIDATE_NOT_NULL(file);
    // The context uses the address from the device-info to maintain the list of devices. I.e., we need a device-info
    // that uses a device-info that is_same_as() the one created above, in rs2_context_add_device:
    auto dev_info = std::make_shared< playback_device_info >( ctx->ctx, file );
    ctx->ctx->remove_device( dev_info );
}
HANDLE_EXCEPTIONS_AND_RETURN(, ctx, file)

void rs2_context_unload_tracking_module(rs2_context* ctx, rs2_error** error) BEGIN_API_CALL
{
}
HANDLE_EXCEPTIONS_AND_RETURN(, ctx)

const char* rs2_playback_device_get_file_path(const rs2_device* device, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    auto playback = VALIDATE_INTERFACE(device->device, librealsense::playback_device);
    return playback->get_file_name().c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device)

unsigned long long int rs2_playback_get_duration(const rs2_device* device, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    auto playback = VALIDATE_INTERFACE(device->device, librealsense::playback_device);
    return playback->get_duration();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, device)

void rs2_playback_seek(const rs2_device* device, long long int time, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    VALIDATE_LE(0, time);
    auto playback = VALIDATE_INTERFACE(device->device, librealsense::playback_device);
    playback->seek_to_time(std::chrono::nanoseconds(time));
}
HANDLE_EXCEPTIONS_AND_RETURN(, device)

unsigned long long int rs2_playback_get_position(const rs2_device* device, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    auto playback = VALIDATE_INTERFACE(device->device, librealsense::playback_device);
    return playback->get_position();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, device)

void rs2_playback_device_resume(const rs2_device* device, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    auto playback = VALIDATE_INTERFACE(device->device, librealsense::playback_device);
    playback->resume();
}
HANDLE_EXCEPTIONS_AND_RETURN(, device)

void rs2_playback_device_pause(const rs2_device* device, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    auto playback = VALIDATE_INTERFACE(device->device, librealsense::playback_device);
    return playback->pause();
}
HANDLE_EXCEPTIONS_AND_RETURN(, device)

void rs2_playback_device_set_real_time(const rs2_device* device, int real_time, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    auto playback = VALIDATE_INTERFACE(device->device, librealsense::playback_device);
    playback->set_real_time(real_time == 0 ? false : true);
}
HANDLE_EXCEPTIONS_AND_RETURN(, device)

int rs2_playback_device_is_real_time(const rs2_device* device, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    auto playback = VALIDATE_INTERFACE(device->device, librealsense::playback_device);
    return playback->is_real_time() ? 1 : 0;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, device)

void rs2_playback_device_set_status_changed_callback(const rs2_device* device, rs2_playback_status_changed_callback* callback, rs2_error** error) BEGIN_API_CALL
{
    // Take ownership of the callback ASAP or else memory leaks could result if we throw! (the caller usually does a
    // 'new' when calling us)
    VALIDATE_NOT_NULL( callback );
    auto cb = std::shared_ptr< rs2_playback_status_changed_callback >( callback,
                                                                       []( rs2_playback_status_changed_callback * p ) {
                                                                           if( p )
                                                                               p->release();
                                                                       } );

    VALIDATE_NOT_NULL(device);
    auto playback = VALIDATE_INTERFACE(device->device, librealsense::playback_device);
    // Maintain a single status-changed callback that'll get replaced with each subsequent call, and removed when this
    // rs2_device is destroyed:
    device->playback_status_changed = playback->playback_status_changed.subscribe(
        [cb]( rs2_playback_status status ) { cb->on_playback_status_changed( status ); } );
}
HANDLE_EXCEPTIONS_AND_RETURN(, device, callback)


rs2_playback_status rs2_playback_device_get_current_status(const rs2_device* device, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    auto playback = VALIDATE_INTERFACE(device->device, librealsense::playback_device);
    return playback->get_current_status();
}
HANDLE_EXCEPTIONS_AND_RETURN(RS2_PLAYBACK_STATUS_UNKNOWN, device)

void rs2_playback_device_set_playback_speed(const rs2_device* device, float speed, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    auto playback = VALIDATE_INTERFACE(device->device, librealsense::playback_device);
    playback->set_frame_rate(speed);
}
HANDLE_EXCEPTIONS_AND_RETURN(, device)

void rs2_playback_device_stop(const rs2_device* device, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    auto playback = VALIDATE_INTERFACE(device->device, librealsense::playback_device);
    return playback->stop();
}
HANDLE_EXCEPTIONS_AND_RETURN(, device)

rs2_device* rs2_create_record_device(const rs2_device* device, const char* file, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(device->device);
    VALIDATE_NOT_NULL(file);

    return rs2_create_record_device_ex(device, file, device->device->compress_while_record(), error);
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device, device->device, file)

rs2_device* rs2_create_record_device_ex(const rs2_device* device, const char* file, int compression_enabled, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(file);

    return new rs2_device({
        std::make_shared<record_device>(device->device, std::make_shared<ros2_writer>(file, compression_enabled != 0))
        });
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device, file)

void rs2_record_device_pause(const rs2_device* device, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    auto record_device = VALIDATE_INTERFACE(device->device, librealsense::record_device);
    record_device->pause_recording();
}
HANDLE_EXCEPTIONS_AND_RETURN(, device)

void rs2_record_device_resume(const rs2_device* device, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    auto record_device = VALIDATE_INTERFACE(device->device, librealsense::record_device);
    record_device->resume_recording();
}
HANDLE_EXCEPTIONS_AND_RETURN(, device)

const char* rs2_record_device_filename(const rs2_device* device, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    auto record_device = VALIDATE_INTERFACE(device->device, librealsense::record_device);
    return record_device->get_filename().c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device)


rs2_frame* rs2_allocate_synthetic_video_frame(rs2_source* source, const rs2_stream_profile* new_stream, rs2_frame* original,
    int new_bpp, int new_width, int new_height, int new_stride, rs2_extension frame_type, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(source);
    VALIDATE_NOT_NULL(original);
    VALIDATE_NOT_NULL(new_stream);

    auto recovered_profile = std::dynamic_pointer_cast<stream_profile_interface>(new_stream->profile->shared_from_this());

    return (rs2_frame*)source->source->allocate_video_frame(recovered_profile,
        (frame_interface*)original, new_bpp, new_width, new_height, new_stride, frame_type);
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, source, new_stream, original, new_bpp, new_width, new_height, new_stride, frame_type)

rs2_frame* rs2_allocate_synthetic_motion_frame(rs2_source* source, const rs2_stream_profile* new_stream, rs2_frame* original,
    rs2_extension frame_type, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(source);
    VALIDATE_NOT_NULL(original);
    VALIDATE_NOT_NULL(new_stream);

    auto recovered_profile = std::dynamic_pointer_cast<stream_profile_interface>(new_stream->profile->shared_from_this());

    return (rs2_frame*)source->source->allocate_motion_frame(recovered_profile,
        (frame_interface*)original, frame_type);
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, source, new_stream, original, frame_type)

rs2_frame* rs2_allocate_points(rs2_source* source, const rs2_stream_profile* new_stream, rs2_frame* original, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(source);
    VALIDATE_NOT_NULL(original);
    VALIDATE_NOT_NULL(new_stream);

    auto recovered_profile = std::dynamic_pointer_cast<stream_profile_interface>(new_stream->profile->shared_from_this());

    return (rs2_frame*)source->source->allocate_points(recovered_profile,
        (frame_interface*)original);
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, source, new_stream, original)

void rs2_synthetic_frame_ready(rs2_source* source, rs2_frame* frame, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame);

    librealsense::frame_holder holder((frame_interface*)frame);
    VALIDATE_NOT_NULL(source);

    source->source->frame_ready(std::move(holder));
}
HANDLE_EXCEPTIONS_AND_RETURN(, source, frame)

rs2_pipeline* rs2_create_pipeline(rs2_context* ctx, rs2_error ** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(ctx);

    auto pipe = std::make_shared<pipeline::pipeline>(ctx->ctx);

    return new rs2_pipeline{ pipe };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, ctx)

void rs2_pipeline_stop(rs2_pipeline* pipe, rs2_error ** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(pipe);

    pipe->pipeline->stop();
}
HANDLE_EXCEPTIONS_AND_RETURN(, pipe)

void rs2_pipeline_set_device( rs2_pipeline * pipe, rs2_device * device, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( pipe );
    VALIDATE_NOT_NULL( device );

    pipe->pipeline->set_device( device->device );
}
HANDLE_EXCEPTIONS_AND_RETURN(, pipe, device ) 

rs2_frame* rs2_pipeline_wait_for_frames(rs2_pipeline* pipe, unsigned int timeout_ms, rs2_error ** error)BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(pipe);

    auto f = pipe->pipeline->wait_for_frames(timeout_ms);
    auto frame = f.frame;
    f.frame = nullptr;
    return (rs2_frame*)(frame);
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, pipe)

int rs2_pipeline_poll_for_frames(rs2_pipeline * pipe, rs2_frame** output_frame, rs2_error ** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(pipe);
    VALIDATE_NOT_NULL(output_frame);

    librealsense::frame_holder fh;
    if (pipe->pipeline->poll_for_frames(&fh))
    {
        frame_interface* result = nullptr;
        std::swap(result, fh.frame);
        *output_frame = (rs2_frame*)result;
        return true;
    }
    return false;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, pipe, output_frame)

int rs2_pipeline_try_wait_for_frames(rs2_pipeline* pipe, rs2_frame** output_frame, unsigned int timeout_ms, rs2_error ** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(pipe);
    VALIDATE_NOT_NULL(output_frame);

    librealsense::frame_holder fh;
    if (pipe->pipeline->try_wait_for_frames(&fh, timeout_ms))
    {
        frame_interface* result = nullptr;
        std::swap(result, fh.frame);
        *output_frame = (rs2_frame*)result;
        return true;
    }
    return false;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, pipe, output_frame)

void rs2_delete_pipeline(rs2_pipeline* pipe) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(pipe);

    delete pipe;
}
NOEXCEPT_RETURN(, pipe)

rs2_pipeline_profile* rs2_pipeline_start(rs2_pipeline* pipe, rs2_error ** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(pipe);
    return new rs2_pipeline_profile{ pipe->pipeline->start(std::make_shared<pipeline::config>()) };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, pipe)

rs2_pipeline_profile* rs2_pipeline_start_with_config(rs2_pipeline* pipe, rs2_config* config, rs2_error ** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(pipe);
    VALIDATE_NOT_NULL(config);
    return new rs2_pipeline_profile{ pipe->pipeline->start(config->config) };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, pipe, config)

rs2_pipeline_profile* rs2_pipeline_start_with_callback(rs2_pipeline* pipe, rs2_frame_callback_ptr on_frame, void* user, rs2_error ** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(pipe);
    auto callback = make_user_frame_callback( on_frame, user );
    return new rs2_pipeline_profile{ pipe->pipeline->start(std::make_shared<pipeline::config>(), std::move(callback)) };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, pipe, on_frame, user)

rs2_pipeline_profile* rs2_pipeline_start_with_config_and_callback(rs2_pipeline* pipe, rs2_config* config, rs2_frame_callback_ptr on_frame, void* user, rs2_error ** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(pipe);
    VALIDATE_NOT_NULL(config);
    auto callback = make_user_frame_callback( on_frame, user );
    return new rs2_pipeline_profile{ pipe->pipeline->start(config->config, callback) };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, pipe, config, on_frame, user)

rs2_pipeline_profile* rs2_pipeline_start_with_callback_cpp(rs2_pipeline* pipe, rs2_frame_callback* callback, rs2_error ** error) BEGIN_API_CALL
{
    // Take ownership of the callback ASAP or else memory leaks could result if we throw! (the caller usually does a
    // 'new' when calling us)
    VALIDATE_NOT_NULL( callback );
    rs2_frame_callback_sptr callback_ptr{ callback,
                                          []( rs2_frame_callback * p )
                                          {
                                              p->release();
                                          } };

    VALIDATE_NOT_NULL(pipe);
    return new rs2_pipeline_profile{ pipe->pipeline->start( std::make_shared< pipeline::config >(), callback_ptr ) };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, pipe, callback)

rs2_pipeline_profile* rs2_pipeline_start_with_config_and_callback_cpp(rs2_pipeline* pipe, rs2_config* config, rs2_frame_callback* callback, rs2_error ** error) BEGIN_API_CALL
{
    // Take ownership of the callback ASAP or else memory leaks could result if we throw! (the caller usually does a
    // 'new' when calling us)
    VALIDATE_NOT_NULL( callback );
    rs2_frame_callback_sptr callback_ptr{ callback,
                                          []( rs2_frame_callback * p )
                                          {
                                              p->release();
                                          } };

    VALIDATE_NOT_NULL(pipe);
    VALIDATE_NOT_NULL(config);
    return new rs2_pipeline_profile{ pipe->pipeline->start( config->config, callback_ptr ) };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, pipe, config, callback)

rs2_pipeline_profile* rs2_pipeline_get_active_profile(rs2_pipeline* pipe, rs2_error ** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(pipe);

    return new rs2_pipeline_profile{ pipe->pipeline->get_active_profile() };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, pipe)

rs2_device* rs2_pipeline_profile_get_device(rs2_pipeline_profile* profile, rs2_error ** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(profile);

    auto dev = profile->profile->get_device();
    return new rs2_device{ dev };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, profile)

rs2_stream_profile_list* rs2_pipeline_profile_get_streams(rs2_pipeline_profile* profile, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(profile);
    return new rs2_stream_profile_list{ profile->profile->get_active_streams() };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, profile)

void rs2_delete_pipeline_profile(rs2_pipeline_profile* profile) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(profile);

    delete profile;
}
NOEXCEPT_RETURN(, profile)

//config
rs2_config* rs2_create_config(rs2_error** error) BEGIN_API_CALL
{
    return new rs2_config{ std::make_shared<pipeline::config>() };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, 0)

void rs2_delete_config(rs2_config* config) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(config);

    delete config;
}
NOEXCEPT_RETURN(, config)

void rs2_config_enable_stream(rs2_config* config,
    rs2_stream stream,
    int index,
    int width,
    int height,
    rs2_format format,
    int framerate,
    rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(config);
    config->config->enable_stream(stream, index, width, height, format, framerate);
}
HANDLE_EXCEPTIONS_AND_RETURN(, config, stream, index, width, height, format, framerate)

void rs2_config_enable_all_stream(rs2_config* config, rs2_error ** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(config);
    config->config->enable_all_stream();
}
HANDLE_EXCEPTIONS_AND_RETURN(, config)

void rs2_config_enable_device(rs2_config* config, const char* serial, rs2_error ** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(config);
    VALIDATE_NOT_NULL(serial);

    config->config->enable_device(serial);

}
HANDLE_EXCEPTIONS_AND_RETURN(, config, serial)

void rs2_config_enable_device_from_file_repeat_option(rs2_config* config, const char* file, int repeat_playback, rs2_error ** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(config);
    VALIDATE_NOT_NULL(file);

    config->config->enable_device_from_file(file, repeat_playback != 0);
}
HANDLE_EXCEPTIONS_AND_RETURN(, config, file)

void rs2_config_enable_device_from_file(rs2_config* config, const char* file, rs2_error ** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(config);
    VALIDATE_NOT_NULL(file);

    config->config->enable_device_from_file(file, true);
}
HANDLE_EXCEPTIONS_AND_RETURN(, config, file)

void rs2_config_enable_record_to_file(rs2_config* config, const char* file, rs2_error ** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(config);
    VALIDATE_NOT_NULL(file);

    config->config->enable_record_to_file(file);
}
HANDLE_EXCEPTIONS_AND_RETURN(, config, file)

void rs2_config_disable_stream(rs2_config* config, rs2_stream stream, rs2_error ** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(config);
    config->config->disable_stream(stream);
}
HANDLE_EXCEPTIONS_AND_RETURN(, config, stream)

void rs2_config_disable_indexed_stream(rs2_config* config, rs2_stream stream, int index, rs2_error ** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(config);
    config->config->disable_stream(stream, index);
}
HANDLE_EXCEPTIONS_AND_RETURN(, config, stream, index)

void rs2_config_disable_all_streams(rs2_config* config, rs2_error ** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(config);
    config->config->disable_all_streams();
}
HANDLE_EXCEPTIONS_AND_RETURN(, config)

rs2_pipeline_profile* rs2_config_resolve(rs2_config* config, rs2_pipeline* pipe, rs2_error ** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(config);
    VALIDATE_NOT_NULL(pipe);
    return new rs2_pipeline_profile{ config->config->resolve(pipe->pipeline) };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, config, pipe)

int rs2_config_can_resolve(rs2_config* config, rs2_pipeline* pipe, rs2_error ** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(config);
    VALIDATE_NOT_NULL(pipe);
    return config->config->can_resolve(pipe->pipeline) ? 1 : 0;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, config, pipe)

rs2_processing_block* rs2_create_processing_block(rs2_frame_processor_callback* proc, rs2_error** error) BEGIN_API_CALL
{
    // Take ownership of the callback ASAP or else memory leaks could result if we throw! (the caller usually does a
    // 'new' when calling us)
    rs2_frame_processor_callback_sptr callback_ptr{ proc, []( rs2_frame_processor_callback * p ) {
                                                  p->release();
                                              } };
    auto block = std::make_shared<librealsense::processing_block>("Custom processing block");
    block->set_processing_callback( callback_ptr );

    return new rs2_processing_block{ block };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, proc)


class internal_frame_processor_fptr_callback : public rs2_frame_processor_callback
{
    rs2_frame_processor_callback_ptr fptr;
    void * user;

public:
    internal_frame_processor_fptr_callback( rs2_frame_processor_callback_ptr on_frame, void * user )
        : fptr( on_frame )
        , user( user )
    {
    }

    void on_frame( rs2_frame * frame, rs2_source * source ) override
    {
        if( fptr )
        {
            try
            {
                fptr( frame, source, user );
            }
            catch( ... )
            {
                LOG_ERROR( "Received an exception from frame callback!" );
            }
        }
    }

    void release() override { delete this; }
};


rs2_processing_block* rs2_create_processing_block_fptr(rs2_frame_processor_callback_ptr proc, void * context, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(proc);

    auto block = std::make_shared<librealsense::processing_block>("Custom processing block");

    block->set_processing_callback({
        new internal_frame_processor_fptr_callback(proc, context),
        [](rs2_frame_processor_callback* p) { } });

    return new rs2_processing_block{ block };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, proc, context)

int rs2_processing_block_register_simple_option(rs2_processing_block* block, rs2_option option_id, float min, float max, float step, float def, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(block);
    VALIDATE_LE(min, max);
    VALIDATE_RANGE(def, min, max);
    VALIDATE_LE(0, step);

    if (block->options->supports_option(option_id)) return false;
    std::shared_ptr<option> opt = std::make_shared<float_option>(option_range{ min, max, step, def });
    // TODO: am I supposed to use the extensions API here?
    auto options = dynamic_cast<options_container*>(block->options);
    if (!options)
        throw std::runtime_error("Options are not container options");
    options->register_option(option_id, opt);
    return true;
}
HANDLE_EXCEPTIONS_AND_RETURN(false, block, option_id, min, max, step, def)

rs2_processing_block* rs2_create_sync_processing_block(rs2_error** error) BEGIN_API_CALL
{
    auto block = std::make_shared<librealsense::syncer_process_unit>();

    return new rs2_processing_block{ block };
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

void rs2_start_processing(rs2_processing_block* block, rs2_frame_callback* on_frame, rs2_error** error) BEGIN_API_CALL
{
    // Take ownership of the callback ASAP or else memory leaks could result if we throw! (the caller usually does a
    // 'new' when calling us)
    rs2_frame_callback_sptr callback_ptr{ on_frame,
                                          []( rs2_frame_callback * p )
                                          {
                                              p->release();
                                          } };

    VALIDATE_NOT_NULL(block);

    block->block->set_output_callback( callback_ptr );
}
HANDLE_EXCEPTIONS_AND_RETURN(, block, on_frame)

void rs2_start_processing_fptr(rs2_processing_block* block, rs2_frame_callback_ptr on_frame, void* user, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(block);
    VALIDATE_NOT_NULL(on_frame);

    auto callback = make_user_frame_callback( on_frame, user );
    block->block->set_output_callback( std::move( callback ) );
}
HANDLE_EXCEPTIONS_AND_RETURN(, block, on_frame, user)

void rs2_start_processing_queue(rs2_processing_block* block, rs2_frame_queue* queue, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(block);
    VALIDATE_NOT_NULL(queue);
    auto callback = make_user_frame_callback( rs2_enqueue_frame, queue );
    block->block->set_output_callback(std::move(callback));
}
HANDLE_EXCEPTIONS_AND_RETURN(, block, queue)

void rs2_process_frame(rs2_processing_block* block, rs2_frame* frame, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(block);
    VALIDATE_NOT_NULL(frame);

    block->block->invoke(frame_holder((frame_interface*)frame));
}
HANDLE_EXCEPTIONS_AND_RETURN(, block, frame)

void rs2_delete_processing_block(rs2_processing_block* block) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(block);

    delete block;
}
NOEXCEPT_RETURN(, block)

rs2_frame* rs2_extract_frame(rs2_frame* composite, int index, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(composite);

    auto cf = VALIDATE_INTERFACE((frame_interface*)composite, librealsense::composite_frame);

    VALIDATE_RANGE(index, 0, (int)cf->get_embedded_frames_count() - 1);
    auto res = cf->get_frame(index);
    res->acquire();
    return (rs2_frame*)res;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, composite)

rs2_frame* rs2_allocate_composite_frame(rs2_source* source, rs2_frame** frames, int count, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(source)
    VALIDATE_NOT_NULL(frames)
    VALIDATE_RANGE(count, 1, 128);

    std::vector<frame_holder> holders(count);
    for (int i = 0; i < count; i++)
    {
        holders[i] = std::move(frame_holder((frame_interface*)frames[i]));
    }
    auto res = source->source->allocate_composite_frame(std::move(holders));

    return (rs2_frame*)res;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, frames, count)

int rs2_embedded_frames_count(rs2_frame* composite, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(composite)

    auto cf = VALIDATE_INTERFACE((frame_interface*)composite, librealsense::composite_frame);

    return static_cast<int>(cf->get_embedded_frames_count());
}
HANDLE_EXCEPTIONS_AND_RETURN(0, composite)

rs2_vertex* rs2_get_frame_vertices(const rs2_frame* frame, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame);
    auto points = VALIDATE_INTERFACE((frame_interface*)frame, librealsense::points);
    return (rs2_vertex*)points->get_vertices();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, frame)

void rs2_export_to_ply(const rs2_frame* frame, const char* fname, rs2_frame* texture, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame);
    VALIDATE_NOT_NULL(fname);
    auto points = VALIDATE_INTERFACE((frame_interface*)frame, librealsense::points);
    points->export_to_ply(fname, (frame_interface*)texture);
}
HANDLE_EXCEPTIONS_AND_RETURN(, frame, fname)

rs2_pixel* rs2_get_frame_texture_coordinates(const rs2_frame* frame, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame);
    auto points = VALIDATE_INTERFACE((frame_interface*)frame, librealsense::points);
    return (rs2_pixel*)points->get_texture_coordinates();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, frame)

int rs2_get_frame_points_count(const rs2_frame* frame, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame);
    auto points = VALIDATE_INTERFACE((frame_interface*)frame, librealsense::points);
    return static_cast<int>(points->get_vertex_count());
}
HANDLE_EXCEPTIONS_AND_RETURN(0, frame)

rs2_vertex* rs2_get_frame_labeled_vertices(const rs2_frame* frame, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame);
    auto labeled_points = VALIDATE_INTERFACE((frame_interface*)frame, librealsense::labeled_points);
    return (rs2_vertex*)labeled_points->get_vertices();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, frame)

int rs2_get_frame_labeled_points_count(const rs2_frame* frame, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame);
    auto labeled_points = VALIDATE_INTERFACE((frame_interface*)frame, librealsense::labeled_points);
    return static_cast<int>(labeled_points->get_vertex_count());
}
HANDLE_EXCEPTIONS_AND_RETURN(0, frame)

void* rs2_get_frame_labels(const rs2_frame* frame, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame);
    auto labeled_points = VALIDATE_INTERFACE((frame_interface*)frame, librealsense::labeled_points);
    return (void*)labeled_points->get_labels();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, frame)

unsigned int rs2_get_frame_labeled_points_width(const rs2_frame* frame_ref, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame_ref);
    auto labeled_points = VALIDATE_INTERFACE(((frame_interface*)frame_ref), librealsense::labeled_points);
    return labeled_points->get_width();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, frame_ref)

unsigned int rs2_get_frame_labeled_points_height(const rs2_frame* frame_ref, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame_ref);
    auto labeled_points = VALIDATE_INTERFACE(((frame_interface*)frame_ref), librealsense::labeled_points);
    return labeled_points->get_height();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, frame_ref)

unsigned int rs2_get_frame_labeled_points_bits_per_pixel(const rs2_frame* frame_ref, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame_ref);
    auto labeled_points = VALIDATE_INTERFACE(((frame_interface*)frame_ref), librealsense::labeled_points);
    return static_cast<unsigned int>(labeled_points->get_bpp());
}
HANDLE_EXCEPTIONS_AND_RETURN(0, frame_ref)

rs2_processing_block* rs2_create_pointcloud(rs2_error** error) BEGIN_API_CALL
{
    return new rs2_processing_block { pointcloud::create() };
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

rs2_processing_block* rs2_create_yuy_decoder(rs2_error** error) BEGIN_API_CALL
{
    return new rs2_processing_block { std::make_shared<yuy2_converter>(RS2_FORMAT_RGB8) };
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

rs2_processing_block* rs2_create_m420_decoder(rs2_error** error) BEGIN_API_CALL
{
    return new rs2_processing_block{ std::make_shared<m420_converter>(RS2_FORMAT_RGB8) };
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

rs2_processing_block* rs2_create_y411_decoder(rs2_error** error) BEGIN_API_CALL
{
    return new rs2_processing_block{ std::make_shared<y411_converter>(RS2_FORMAT_RGB8) };
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

rs2_processing_block* rs2_create_threshold(rs2_error** error) BEGIN_API_CALL
{
    return new rs2_processing_block { std::make_shared<threshold>() };
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

rs2_processing_block* rs2_create_units_transform(rs2_error** error) BEGIN_API_CALL
{
    return new rs2_processing_block { std::make_shared<units_transform>() };
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

rs2_processing_block* rs2_create_align(rs2_stream align_to, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_ENUM(align_to);

    auto block = align::create_align(align_to);

    return new rs2_processing_block{ block };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, align_to)

rs2_processing_block* rs2_create_colorizer(rs2_error** error) BEGIN_API_CALL
{
    auto block = std::make_shared<librealsense::colorizer>();

    auto res = new rs2_processing_block{ block };

    return res;
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)


rs2_processing_block* rs2_create_decimation_filter_block(rs2_error** error) BEGIN_API_CALL
{
    auto block = std::make_shared<librealsense::decimation_filter>();

    return new rs2_processing_block{ block };
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

rs2_processing_block* rs2_create_rotation_filter_block( rs2_streams_list streams_to_rotate, rs2_error ** error ) BEGIN_API_CALL
{
    auto block = std::make_shared< librealsense::rotation_filter >( streams_to_rotate.list );

    return new rs2_processing_block{ block };
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN( nullptr )

rs2_processing_block* rs2_create_temporal_filter_block(rs2_error** error) BEGIN_API_CALL
{
    auto block = std::make_shared<librealsense::temporal_filter>();

    return new rs2_processing_block{ block };
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

rs2_processing_block* rs2_create_spatial_filter_block(rs2_error** error) BEGIN_API_CALL
{
    auto block = std::make_shared<librealsense::spatial_filter>();

    return new rs2_processing_block{ block };
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

rs2_processing_block* rs2_create_disparity_transform_block(unsigned char transform_to_disparity, rs2_error** error) BEGIN_API_CALL
{
    auto block = std::make_shared<librealsense::disparity_transform>(transform_to_disparity > 0);

    return new rs2_processing_block{ block };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, transform_to_disparity)

rs2_processing_block* rs2_create_hole_filling_filter_block(rs2_error** error) BEGIN_API_CALL
{
    auto block = std::make_shared<librealsense::hole_filling_filter>();

    return new rs2_processing_block{ block };
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

rs2_processing_block* rs2_create_rates_printer_block(rs2_error** error) BEGIN_API_CALL
{
    auto block = std::make_shared<librealsense::rates_printer>();

    return new rs2_processing_block{ block };
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

rs2_processing_block* rs2_create_zero_order_invalidation_block(rs2_error** error) BEGIN_API_CALL
{
    throw not_implemented_exception( "deprecated" );
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

rs2_processing_block* rs2_create_huffman_depth_decompress_block(rs2_error** error) BEGIN_API_CALL
{
    throw not_implemented_exception( "deprecated" );
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

rs2_processing_block* rs2_create_hdr_merge_processing_block(rs2_error** error) BEGIN_API_CALL
{
    auto block = std::make_shared<librealsense::hdr_merge>();

    return new rs2_processing_block{ block };
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

rs2_processing_block* rs2_create_sequence_id_filter(rs2_error** error) BEGIN_API_CALL
{
    auto block = std::make_shared<librealsense::sequence_id_filter>();

    return new rs2_processing_block{ block };
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

float rs2_get_depth_scale(rs2_sensor* sensor, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    auto ds = VALIDATE_INTERFACE(sensor->sensor, librealsense::depth_sensor);
    return ds->get_depth_scale();
}
HANDLE_EXCEPTIONS_AND_RETURN(0.f, sensor)

float rs2_get_max_usable_depth_range(rs2_sensor const * sensor, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);

    auto murs = VALIDATE_INTERFACE(sensor->sensor, librealsense::max_usable_range_sensor);
    return murs->get_max_usable_depth_range();
}

HANDLE_EXCEPTIONS_AND_RETURN(0.f, sensor)

float rs2_get_stereo_baseline(rs2_sensor* sensor, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    auto ds = VALIDATE_INTERFACE(sensor->sensor, librealsense::depth_stereo_sensor);
    return ds->get_stereo_baseline_mm();
}
HANDLE_EXCEPTIONS_AND_RETURN(0.f, sensor)

rs2_device* rs2_create_device_from_sensor(const rs2_sensor* sensor, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    VALIDATE_NOT_NULL(sensor->parent);
    return new rs2_device{ sensor->parent };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, sensor)

float rs2_depth_frame_get_distance(const rs2_frame* frame_ref, int x, int y, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame_ref);
    auto df = VALIDATE_INTERFACE(((frame_interface*)frame_ref), librealsense::depth_frame);
    VALIDATE_RANGE(x, 0, df->get_width() - 1);
    VALIDATE_RANGE(y, 0, df->get_height() - 1);
    return df->get_distance(x, y);
}
HANDLE_EXCEPTIONS_AND_RETURN(0, frame_ref, x, y)

float rs2_depth_frame_get_units( const rs2_frame* frame_ref, rs2_error** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( frame_ref );
    auto df = VALIDATE_INTERFACE((( frame_interface * ) frame_ref ), librealsense::depth_frame );
    return df->get_units();
}
HANDLE_EXCEPTIONS_AND_RETURN( 0, frame_ref )

float rs2_depth_stereo_frame_get_baseline(const rs2_frame* frame_ref, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame_ref);
    auto df = VALIDATE_INTERFACE(((frame_interface*)frame_ref), librealsense::disparity_frame);
    return df->get_stereo_baseline();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, frame_ref)

void rs2_pose_frame_get_pose_data(const rs2_frame* frame, rs2_pose* pose, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame);
    VALIDATE_NOT_NULL(pose);

    auto pf = VALIDATE_INTERFACE((frame_interface*)frame, librealsense::pose_frame);

    const float3 t = pf->get_translation();
    pose->translation = { t.x, t.y, t.z };

    const float3 v = pf->get_velocity();
    pose->velocity = { v.x, v.y, v.z };

    const float3 a = pf->get_acceleration();
    pose->acceleration = { a.x, a.y, a.z };

    const float4 r = pf->get_rotation();
    pose->rotation = { r.x, r.y, r.z, r.w };

    const float3 av = pf->get_angular_velocity();
    pose->angular_velocity = { av.x, av.y, av.z };

    const float3 aa = pf->get_angular_acceleration();
    pose->angular_acceleration = { aa.x, aa.y, aa.z };

    pose->tracker_confidence = pf->get_tracker_confidence();
    pose->mapper_confidence = pf->get_mapper_confidence();
}
HANDLE_EXCEPTIONS_AND_RETURN(, frame, pose)

void rs2_extract_target_dimensions(const rs2_frame* frame_ref, rs2_calib_target_type calib_type, float* target_dims, unsigned int target_dims_size, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(frame_ref);
    VALIDATE_NOT_NULL(target_dims_size);

    auto vf = VALIDATE_INTERFACE(((frame_interface*)frame_ref), librealsense::video_frame);
    int width = vf->get_width();
    int height = vf->get_height();
    auto fmt = vf->get_stream()->get_format();

    std::shared_ptr<target_calculator_interface> target_calculator;
    if (calib_type == RS2_CALIB_TARGET_RECT_GAUSSIAN_DOT_VERTICES)
        target_calculator = std::make_shared<rect_gaussian_dots_target_calculator>(width, height, 0, 0, width, height);
    else if (calib_type == RS2_CALIB_TARGET_ROI_RECT_GAUSSIAN_DOT_VERTICES)
        target_calculator = std::make_shared<rect_gaussian_dots_target_calculator>(width, height, _roi_ws, _roi_hs, _roi_we - _roi_ws, _roi_he - _roi_hs);
    else if (calib_type == RS2_CALIB_TARGET_POS_GAUSSIAN_DOT_VERTICES)
        target_calculator = std::make_shared<rect_gaussian_dots_target_calculator>(width, height, _roi_ws, _roi_hs, _roi_we - _roi_ws, _roi_he - _roi_hs);
    else
        throw std::runtime_error("unsupported calibration target type");
    
    if (RS2_FORMAT_Y8 == fmt)
    {
        if (!target_calculator->calculate(vf->get_frame_data(), target_dims, target_dims_size))
            throw std::runtime_error("Failed to find the four rectangle side sizes on the frame");
    }
    else if (RS2_FORMAT_RGB8 == fmt)
    {
        int size = width * height;
        std::vector<uint8_t> buf(size);
        uint8_t* p = buf.data();
        const uint8_t* q = vf->get_frame_data();
        float tmp = 0;
        for (int i = 0; i < size; ++i)
        {
            tmp = static_cast<float>(*q++);
            tmp += static_cast<float>(*q++);
            tmp += static_cast<float>(*q++);
            *p++ = static_cast<uint8_t>(tmp / 3 + 0.5f);
        }
        if (!target_calculator->calculate(buf.data(), target_dims, target_dims_size))
            throw std::runtime_error("Failed to find the four rectangle side sizes on the frame");
    }
    else
        throw std::runtime_error( rsutils::string::from()
                                  << "Unsupported video frame format " << rs2_format_to_string( fmt ) );
}
HANDLE_EXCEPTIONS_AND_RETURN(, frame_ref, calib_type, target_dims, target_dims_size)

rs2_time_t rs2_get_time(rs2_error** error) BEGIN_API_CALL
{
    return time_service::get_time();
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(0)

rs2_device* rs2_create_software_device(rs2_error** error) BEGIN_API_CALL
{
    // We're not given a context...
    auto ctx = context::make( json::object( { { "dds", false } } ) );
    auto dev_info = std::make_shared< software_device_info >( ctx );
    auto dev = std::make_shared< software_device >( dev_info );
    dev_info->set_device( dev );
    return new rs2_device{ dev };
}
NOARGS_HANDLE_EXCEPTIONS_AND_RETURN(0)

void rs2_software_device_create_matcher(rs2_device* dev, rs2_matchers m, rs2_error** error)BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    auto df = VALIDATE_INTERFACE(dev->device, librealsense::software_device);
    df->set_matcher_type(m);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, m)

void rs2_software_device_register_info(rs2_device* dev, rs2_camera_info info, const char * val, rs2_error** error)BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    auto df = VALIDATE_INTERFACE(dev->device, librealsense::software_device);
    df->register_info(info, val);
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, info, val)

void rs2_software_device_update_info(rs2_device* dev, rs2_camera_info info, const char * val, rs2_error** error)BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    auto df = VALIDATE_INTERFACE(dev->device, librealsense::software_device);
    if (df->supports_info(info))
    {
        df->update_info(info, val);
    }
    else
        throw librealsense::invalid_value_exception( rsutils::string::from()
                                                     << "info " << rs2_camera_info_to_string( info )
                                                     << " not supported by the device!" );
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, info, val)

rs2_sensor* rs2_software_device_add_sensor(rs2_device* dev, const char* sensor_name, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    auto df = VALIDATE_INTERFACE(dev->device, librealsense::software_device);

    return new rs2_sensor(
        dev->device,
        &df->add_software_sensor(sensor_name));
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, dev, sensor_name)

void rs2_software_sensor_on_video_frame(rs2_sensor* sensor, rs2_software_video_frame frame, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    auto bs = VALIDATE_INTERFACE(sensor->sensor, librealsense::software_sensor);
    return bs->on_video_frame(frame);
}
HANDLE_EXCEPTIONS_AND_RETURN(, sensor, frame.pixels)

void rs2_software_sensor_on_motion_frame(rs2_sensor* sensor, rs2_software_motion_frame frame, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    auto bs = VALIDATE_INTERFACE(sensor->sensor, librealsense::software_sensor);
    return bs->on_motion_frame(frame);
}
HANDLE_EXCEPTIONS_AND_RETURN(, sensor, frame.data)

void rs2_software_sensor_on_pose_frame(rs2_sensor* sensor, rs2_software_pose_frame frame, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    auto bs = VALIDATE_INTERFACE(sensor->sensor, librealsense::software_sensor);
    return bs->on_pose_frame(frame);
}
HANDLE_EXCEPTIONS_AND_RETURN(, sensor, frame.data)

void rs2_software_sensor_on_notification(rs2_sensor* sensor, rs2_software_notification notif, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    auto bs = VALIDATE_INTERFACE(sensor->sensor, librealsense::software_sensor);
    return bs->on_notification(notif);
}
HANDLE_EXCEPTIONS_AND_RETURN(, sensor, notif.category, notif.type, notif.severity, notif.description, notif.serialized_data)

void rs2_software_sensor_set_metadata(rs2_sensor* sensor, rs2_frame_metadata_value key, rs2_metadata_type value, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    auto bs = VALIDATE_INTERFACE(sensor->sensor, librealsense::software_sensor);
    return bs->set_metadata(key, value);
}
HANDLE_EXCEPTIONS_AND_RETURN(, sensor, key, value)

rs2_stream_profile* rs2_software_sensor_add_video_stream(rs2_sensor* sensor, rs2_video_stream video_stream, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    auto bs = VALIDATE_INTERFACE(sensor->sensor, librealsense::software_sensor);
    return bs->add_video_stream(video_stream)->get_c_wrapper();
}
HANDLE_EXCEPTIONS_AND_RETURN(0,sensor, video_stream.type, video_stream.index, video_stream.fmt, video_stream.width, video_stream.height, video_stream.uid)

rs2_stream_profile* rs2_software_sensor_add_video_stream_ex(rs2_sensor* sensor, rs2_video_stream video_stream, int is_default, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    auto bs = VALIDATE_INTERFACE(sensor->sensor, librealsense::software_sensor);
    return bs->add_video_stream(video_stream, is_default != 0 )->get_c_wrapper();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, sensor, video_stream.type, video_stream.index, video_stream.fmt, video_stream.width, video_stream.height, video_stream.uid, is_default)

rs2_stream_profile* rs2_software_sensor_add_motion_stream(rs2_sensor* sensor, rs2_motion_stream motion_stream, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    auto bs = VALIDATE_INTERFACE(sensor->sensor, librealsense::software_sensor);
    return bs->add_motion_stream(motion_stream)->get_c_wrapper();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, sensor, motion_stream.type, motion_stream.index, motion_stream.fmt, motion_stream.uid)

rs2_stream_profile* rs2_software_sensor_add_motion_stream_ex(rs2_sensor* sensor, rs2_motion_stream motion_stream, int is_default, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    auto bs = VALIDATE_INTERFACE(sensor->sensor, librealsense::software_sensor);
    return bs->add_motion_stream(motion_stream, is_default != 0)->get_c_wrapper();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, sensor, motion_stream.type, motion_stream.index, motion_stream.fmt, motion_stream.uid, is_default)

rs2_stream_profile* rs2_software_sensor_add_pose_stream(rs2_sensor* sensor, rs2_pose_stream pose_stream, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    auto bs = VALIDATE_INTERFACE(sensor->sensor, librealsense::software_sensor);
    return bs->add_pose_stream(pose_stream)->get_c_wrapper();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, sensor, pose_stream.type, pose_stream.index, pose_stream.fmt, pose_stream.uid)

rs2_stream_profile* rs2_software_sensor_add_pose_stream_ex(rs2_sensor* sensor, rs2_pose_stream pose_stream, int is_default, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    auto bs = VALIDATE_INTERFACE(sensor->sensor, librealsense::software_sensor);
    return bs->add_pose_stream(pose_stream, is_default != 0)->get_c_wrapper();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, sensor, pose_stream.type, pose_stream.index, pose_stream.fmt, pose_stream.uid, is_default)

void rs2_software_sensor_add_read_only_option(rs2_sensor* sensor, rs2_option option, float val, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    auto bs = VALIDATE_INTERFACE(sensor->sensor, librealsense::software_sensor);
    return bs->add_read_only_option(option, val);
}
HANDLE_EXCEPTIONS_AND_RETURN(, sensor, option, val)

void rs2_software_sensor_update_read_only_option(rs2_sensor* sensor, rs2_option option, float val, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    auto bs = VALIDATE_INTERFACE(sensor->sensor, librealsense::software_sensor);
    return bs->update_read_only_option(option, val);
}
HANDLE_EXCEPTIONS_AND_RETURN(, sensor, option, val)

void rs2_software_sensor_add_option(rs2_sensor* sensor, rs2_option option, float min, float max, float step, float def, int is_writable, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_LE(min, max);
    VALIDATE_RANGE(def, min, max);
    VALIDATE_LE(0, step);
    VALIDATE_NOT_NULL(sensor);
    auto bs = VALIDATE_INTERFACE(sensor->sensor, librealsense::software_sensor);
    return bs->add_option(option, option_range{ min, max, step, def }, bool(is_writable != 0));
}
HANDLE_EXCEPTIONS_AND_RETURN(, sensor, option, min, max, step, def, is_writable)

void rs2_software_sensor_detach(rs2_sensor* sensor, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    auto bs = VALIDATE_INTERFACE(sensor->sensor, librealsense::software_sensor);
    sensor->parent.reset();
}
HANDLE_EXCEPTIONS_AND_RETURN(, sensor)

void rs2_log(rs2_log_severity severity, const char * message, rs2_error ** error) BEGIN_API_CALL
{
    VALIDATE_ENUM(severity);
    VALIDATE_NOT_NULL(message);
    switch (severity)
    {
    case RS2_LOG_SEVERITY_DEBUG:
        LOG_DEBUG_STR(message);
        break;
    case RS2_LOG_SEVERITY_INFO:
        LOG_INFO_STR(message);
        break;
    case RS2_LOG_SEVERITY_WARN:
        LOG_WARNING_STR(message);
        break;
    case RS2_LOG_SEVERITY_ERROR:
        LOG_ERROR_STR(message);
        break;
    case RS2_LOG_SEVERITY_FATAL:
        LOG_FATAL_STR(message);
        break;
    case RS2_LOG_SEVERITY_NONE:
        break;
    default:
        LOG_INFO_STR(message);
    }
}
HANDLE_EXCEPTIONS_AND_RETURN(, severity, message)

void rs2_loopback_enable(const rs2_device* device, const char* from_file, rs2_error** error) BEGIN_API_CALL
{
    throw not_implemented_exception( "deprecated" );
}
HANDLE_EXCEPTIONS_AND_RETURN(, device, from_file)

void rs2_loopback_disable(const rs2_device* device, rs2_error** error) BEGIN_API_CALL
{
    throw not_implemented_exception( "deprecated" );
}
HANDLE_EXCEPTIONS_AND_RETURN(, device)

int rs2_loopback_is_enabled(const rs2_device* device, rs2_error** error) BEGIN_API_CALL
{
    throw not_implemented_exception( "deprecated" );
}
HANDLE_EXCEPTIONS_AND_RETURN(0, device)

void rs2_connect_tm2_controller(const rs2_device* device, const unsigned char* mac, rs2_error** error) BEGIN_API_CALL
{
    throw not_implemented_exception( "deprecated" );
}
HANDLE_EXCEPTIONS_AND_RETURN(, device)

void rs2_disconnect_tm2_controller(const rs2_device* device, int id, rs2_error** error) BEGIN_API_CALL
{
    throw not_implemented_exception( "deprecated" );
}
HANDLE_EXCEPTIONS_AND_RETURN(, device)

void rs2_set_intrinsics(const rs2_sensor* sensor, const rs2_stream_profile* profile, const rs2_intrinsics* intrinsics, rs2_error** error) BEGIN_API_CALL
{
    throw not_implemented_exception( "deprecated" );
}
HANDLE_EXCEPTIONS_AND_RETURN(, sensor, profile, intrinsics)

void rs2_override_intrinsics( const rs2_sensor* sensor, const rs2_intrinsics* intrinsics, rs2_error** error ) BEGIN_API_CALL
{
    throw not_implemented_exception( "deprecated" );
}
HANDLE_EXCEPTIONS_AND_RETURN( , sensor, intrinsics )

void rs2_set_extrinsics(const rs2_sensor* from_sensor, const rs2_stream_profile* from_profile, rs2_sensor* to_sensor, const rs2_stream_profile* to_profile, const rs2_extrinsics* extrinsics, rs2_error** error) BEGIN_API_CALL
{
    throw not_implemented_exception( "deprecated" );
}
HANDLE_EXCEPTIONS_AND_RETURN(, from_sensor, from_profile, to_sensor, to_profile, extrinsics)

void rs2_set_motion_device_intrinsics(const rs2_sensor* sensor, const rs2_stream_profile* profile, const rs2_motion_device_intrinsic* intrinsics, rs2_error** error) BEGIN_API_CALL
{
    throw not_implemented_exception( "deprecated" );
}
HANDLE_EXCEPTIONS_AND_RETURN(, sensor, profile, intrinsics)

void rs2_reset_to_factory_calibration(const rs2_device* device, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    auto auto_calib = std::dynamic_pointer_cast<auto_calibrated_interface>(device->device);
    if (!auto_calib)
        throw std::runtime_error("this device does not support reset to factory calibration");
    auto_calib->reset_to_factory_calibration();
}
HANDLE_EXCEPTIONS_AND_RETURN(, device)

void rs2_write_calibration(const rs2_device* device, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    auto auto_calib = std::dynamic_pointer_cast<auto_calibrated_interface>(device->device);
    if (!auto_calib)
        throw std::runtime_error("this device does not support auto calibration");
    auto_calib->write_calibration();
}
HANDLE_EXCEPTIONS_AND_RETURN(, device)

rs2_processing_block_list* rs2_get_recommended_processing_blocks(rs2_sensor* sensor, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    return new rs2_processing_block_list{ sensor->sensor->get_recommended_processing_blocks() };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, sensor)

rs2_processing_block* rs2_get_processing_block(const rs2_processing_block_list* list, int index, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(list);
    VALIDATE_RANGE(index, 0, (int)list->list.size() - 1);

    return new rs2_processing_block(list->list[index]);
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

int rs2_get_recommended_processing_blocks_count(const rs2_processing_block_list* list, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(list);
    return static_cast<int>(list->list.size());
}
HANDLE_EXCEPTIONS_AND_RETURN(0, list)

void rs2_delete_recommended_processing_blocks(rs2_processing_block_list* list) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(list);
    delete list;
}
NOEXCEPT_RETURN(, list)

const char* rs2_get_processing_block_info(const rs2_processing_block* block, rs2_camera_info info, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(block);
    VALIDATE_ENUM(info);
    if (block->block->supports_info(info))
    {
        return block->block->get_info(info).c_str();
    }
    throw librealsense::invalid_value_exception( rsutils::string::from() << "Info " << rs2_camera_info_to_string( info )
                                                                         << " not supported by processing block!" );
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, block, info)

int rs2_supports_processing_block_info(const rs2_processing_block* block, rs2_camera_info info, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(block);
    VALIDATE_ENUM(info);
    return block->block->supports_info(info);
}
HANDLE_EXCEPTIONS_AND_RETURN(false, block, info)

int rs2_import_localization_map(const rs2_sensor* sensor, const unsigned char* lmap_blob, unsigned int blob_size, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    VALIDATE_NOT_NULL(lmap_blob);
    VALIDATE_RANGE(blob_size,1, std::numeric_limits<uint32_t>::max());      // Set by FW

    auto pose_snr = VALIDATE_INTERFACE(sensor->sensor, librealsense::pose_sensor_interface);

    std::vector<uint8_t> buffer_to_send(lmap_blob, lmap_blob + blob_size);
    return (int)pose_snr->import_relocalization_map(buffer_to_send);
}
HANDLE_EXCEPTIONS_AND_RETURN(0, sensor, lmap_blob, blob_size)

const rs2_raw_data_buffer* rs2_export_localization_map(const rs2_sensor* sensor, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);

    auto pose_snr = VALIDATE_INTERFACE(sensor->sensor, librealsense::pose_sensor_interface);
    std::vector<uint8_t> recv_buffer;
    if (pose_snr->export_relocalization_map(recv_buffer))
        return new rs2_raw_data_buffer{ recv_buffer };
    return (rs2_raw_data_buffer*)nullptr;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, sensor)

int rs2_set_static_node(const rs2_sensor* sensor, const char* guid, const rs2_vector pos, const rs2_quaternion orient, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    VALIDATE_NOT_NULL(guid);
    auto pose_snr = VALIDATE_INTERFACE(sensor->sensor, librealsense::pose_sensor_interface);
    std::string s_guid(guid);
    VALIDATE_RANGE(s_guid.size(), 1, 127);      // T2xx spec

    return int(pose_snr->set_static_node(s_guid, { pos.x, pos.y, pos.z }, { orient.x, orient.y, orient.z, orient.w }));
}
HANDLE_EXCEPTIONS_AND_RETURN(0, sensor, guid, pos, orient)

int rs2_get_static_node(const rs2_sensor* sensor, const char* guid, rs2_vector *pos, rs2_quaternion *orient, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    VALIDATE_NOT_NULL(guid);
    VALIDATE_NOT_NULL(pos);
    VALIDATE_NOT_NULL(orient);
    auto pose_snr = VALIDATE_INTERFACE(sensor->sensor, librealsense::pose_sensor_interface);
    std::string s_guid(guid);
    VALIDATE_RANGE(s_guid.size(), 1, 127);      // T2xx spec

    float3 t_pos{};
    float4 t_or {};
    int ret = 0;
    if ((ret = pose_snr->get_static_node(s_guid, t_pos, t_or)))
    {
        pos->x = t_pos.x;
        pos->y = t_pos.y;
        pos->z = t_pos.z;
        orient->x = t_or.x;
        orient->y = t_or.y;
        orient->z = t_or.z;
        orient->w = t_or.w;
    }
    return ret;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, sensor, guid, pos, orient)

int rs2_remove_static_node(const rs2_sensor* sensor, const char* guid, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    VALIDATE_NOT_NULL(guid);
    auto pose_snr = VALIDATE_INTERFACE(sensor->sensor, librealsense::pose_sensor_interface);
    std::string s_guid(guid);
    VALIDATE_RANGE(s_guid.size(), 1, 127);      // T2xx spec

    return int(pose_snr->remove_static_node(s_guid));
}
HANDLE_EXCEPTIONS_AND_RETURN(0, sensor, guid)

int rs2_load_wheel_odometry_config(const rs2_sensor* sensor, const unsigned char* odometry_blob, unsigned int blob_size, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    VALIDATE_NOT_NULL(odometry_blob);
    VALIDATE_RANGE(blob_size, 1, std::numeric_limits<uint32_t>::max());

    auto wo_snr = VALIDATE_INTERFACE(sensor->sensor, librealsense::wheel_odometry_interface);

    std::vector<uint8_t> buffer_to_send(odometry_blob, odometry_blob + blob_size);
    int ret = wo_snr->load_wheel_odometery_config(buffer_to_send);
    if (!ret)
        throw librealsense::wrong_api_call_sequence_exception(
            rsutils::string::from() << "Load wheel odometry config failed, file size " << blob_size );
    return ret;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, sensor, odometry_blob, blob_size)

int rs2_send_wheel_odometry(const rs2_sensor* sensor, char wo_sensor_id, unsigned int frame_num,
                            const rs2_vector translational_velocity, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    auto wo_snr = VALIDATE_INTERFACE(sensor->sensor, librealsense::wheel_odometry_interface);

    return wo_snr->send_wheel_odometry(wo_sensor_id, frame_num, { translational_velocity.x, translational_velocity.y, translational_velocity.z });
}
HANDLE_EXCEPTIONS_AND_RETURN(0, sensor, wo_sensor_id, frame_num, translational_velocity)


class update_progress_callback : public rs2_update_progress_callback
{
    rs2_update_progress_callback_ptr _nptr;
    void * _client_data;

public:
    update_progress_callback( rs2_update_progress_callback_ptr on_update_progress, void * client_data = NULL )
        : _nptr( on_update_progress )
        , _client_data( client_data )
    {
    }

    void on_update_progress( const float progress )
    {
        if( _nptr )
        {
            try
            {
                _nptr( progress, _client_data );
            }
            catch( ... )
            {
                LOG_ERROR( "Received an exception from firmware update progress callback!" );
            }
        }
    }

    void release() { delete this; }
};


void rs2_update_firmware_cpp(const rs2_device* device, const void* fw_image, int fw_image_size, rs2_update_progress_callback* callback, rs2_error** error) BEGIN_API_CALL
{
    // Take ownership of the callback ASAP or else memory leaks could result if we throw! (the caller usually does a
    // 'new' when calling us)
    rs2_update_progress_callback_sptr callback_ptr;
    if( callback )
        callback_ptr.reset( callback, []( rs2_update_progress_callback * p ) { p->release(); } );

    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(device->device);
    VALIDATE_NOT_NULL(fw_image);

    auto fwu = VALIDATE_INTERFACE(device->device, librealsense::update_device_interface);
    fwu->update( fw_image, fw_image_size, callback_ptr );
}
HANDLE_EXCEPTIONS_AND_RETURN(, device, fw_image)

void rs2_update_firmware(const rs2_device* device, const void* fw_image, int fw_image_size, rs2_update_progress_callback_ptr callback, void* client_data, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(device->device);
    VALIDATE_NOT_NULL(fw_image);

    auto fwu = VALIDATE_INTERFACE(device->device, librealsense::update_device_interface);

    if(callback == NULL)
        fwu->update(fw_image, fw_image_size, nullptr);
    else
    {
        rs2_update_progress_callback_sptr cb(new update_progress_callback(callback, client_data),
            [](update_progress_callback* p) { delete p; });
        fwu->update(fw_image, fw_image_size, std::move(cb));
    }
}
HANDLE_EXCEPTIONS_AND_RETURN(, device, fw_image)

const rs2_raw_data_buffer* rs2_create_flash_backup_cpp(const rs2_device* device, rs2_update_progress_callback* callback, rs2_error** error) BEGIN_API_CALL
{
    // Take ownership of the callback ASAP or else memory leaks could result if we throw! (the caller usually does a
    // 'new' when calling us)
    rs2_update_progress_callback_sptr callback_ptr;
    if( callback )
        callback_ptr.reset( callback, []( rs2_update_progress_callback * p ) { p->release(); } );

    VALIDATE_NOT_NULL(device);

    auto fwud = std::dynamic_pointer_cast<updatable>(device->device);
    if (!fwud)
        throw std::runtime_error("This device does not support update protocol!");

    std::vector<uint8_t> res = fwud->backup_flash( callback_ptr );

    return new rs2_raw_data_buffer{ res };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device)

const rs2_raw_data_buffer* rs2_create_flash_backup(const rs2_device* device, rs2_update_progress_callback_ptr callback, void* client_data, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);

    auto fwud = std::dynamic_pointer_cast<updatable>(device->device);
    if (!fwud)
        throw std::runtime_error("This device does not support update protocol!");

    std::vector<uint8_t> res;

    if (callback == NULL)
        res = fwud->backup_flash(nullptr);
    else
    {
        rs2_update_progress_callback_sptr cb(new update_progress_callback(callback, client_data),
            [](update_progress_callback* p) { delete p; });
        res = fwud->backup_flash(std::move(cb));
    }

    return new rs2_raw_data_buffer{ res };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device)

void rs2_update_firmware_unsigned_cpp( const rs2_device * device,
                                       const void * image,
                                       int image_size,
                                       rs2_update_progress_callback * callback,
                                       int update_mode,
                                       rs2_error ** error ) BEGIN_API_CALL
{
    // Take ownership of the callback ASAP or else memory leaks could result if we throw! (the caller usually does a
    // 'new' when calling us)
    rs2_update_progress_callback_sptr callback_ptr;
    if( callback )
        callback_ptr.reset( callback, []( rs2_update_progress_callback * p ) { p->release(); } );

    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(device->device);
    VALIDATE_NOT_NULL(image);

    auto fwud = std::dynamic_pointer_cast<updatable>(device->device);
    if (!fwud)
        throw std::runtime_error("This device does not support update protocol!");

    std::vector<uint8_t> buffer((uint8_t*)image, (uint8_t*)image + image_size);

    fwud->update_flash( buffer, callback_ptr, update_mode );
}
HANDLE_EXCEPTIONS_AND_RETURN(, image, device)

void rs2_update_firmware_unsigned(const rs2_device* device, const void* image, int image_size, rs2_update_progress_callback_ptr callback, void* client_data, int update_mode,  rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(image);

    auto fwud = std::dynamic_pointer_cast<updatable>(device->device);
    if (!fwud)
        throw std::runtime_error("This device does not support update protocol!");

    std::vector<uint8_t> buffer((uint8_t*)image, (uint8_t*)image + image_size);

    if (callback == NULL)
        fwud->update_flash(buffer, nullptr, update_mode);
    else
    {
        rs2_update_progress_callback_sptr cb(new update_progress_callback(callback, client_data),
            [](update_progress_callback* p) { delete p; });
        fwud->update_flash(buffer, std::move(cb), update_mode);
    }
}
HANDLE_EXCEPTIONS_AND_RETURN(, image, device)

int rs2_check_firmware_compatibility(const rs2_device* device, const void* fw_image, int fw_image_size, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(fw_image);

    auto fwud = std::dynamic_pointer_cast< firmware_check_interface >( device->device );
    if( ! fwud )
        throw std::runtime_error("This device does not support update protocol!");

    std::vector<uint8_t> buffer((uint8_t*)fw_image, (uint8_t*)fw_image + fw_image_size);

    bool res = fwud->check_fw_compatibility(buffer);

    return res ? 1 : 0;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, fw_image, device)

void rs2_enter_update_state(const rs2_device* device, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);

    auto fwud = std::dynamic_pointer_cast<updatable>(device->device);
    if (!fwud)
        throw std::runtime_error("this device does not support fw update");
    fwud->enter_update_state();
}
HANDLE_EXCEPTIONS_AND_RETURN(, device)

const rs2_raw_data_buffer * rs2_run_on_chip_calibration_cpp( rs2_device * device,
                                                             const void * json_content,
                                                             int content_size,
                                                             float * health,
                                                             rs2_update_progress_callback * progress_callback,
                                                             int timeout_ms,
                                                             rs2_error ** error ) BEGIN_API_CALL
{
    // Take ownership of the callback ASAP or else memory leaks could result if we throw! (the caller usually does a
    // 'new' when calling us)
    rs2_update_progress_callback_sptr callback_ptr;
    if( progress_callback )
        callback_ptr.reset( progress_callback, []( rs2_update_progress_callback * p ) { p->release(); } );

    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(health);

    if (content_size > 0)
        VALIDATE_NOT_NULL(json_content);

    auto auto_calib = VALIDATE_INTERFACE(device->device, librealsense::auto_calibrated_interface);

    std::vector<uint8_t> buffer;

    std::string json((char*)json_content, (char*)json_content + content_size);
    buffer = auto_calib->run_on_chip_calibration( timeout_ms, json, health, callback_ptr );

    return new rs2_raw_data_buffer { buffer };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device)

const rs2_raw_data_buffer* rs2_run_on_chip_calibration(rs2_device* device, const void* json_content, int content_size, float* health, rs2_update_progress_callback_ptr progress_callback, void* user, int timeout_ms, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(health);

    if (content_size > 0)
        VALIDATE_NOT_NULL(json_content);

    auto auto_calib = VALIDATE_INTERFACE(device->device, librealsense::auto_calibrated_interface);

    std::vector<uint8_t> buffer;

    std::string json((char*)json_content, (char*)json_content + content_size);

    if (progress_callback == nullptr)
        buffer = auto_calib->run_on_chip_calibration(timeout_ms, json, health, nullptr);
    else
    {
        rs2_update_progress_callback_sptr cb(new update_progress_callback(progress_callback, user),
            [](update_progress_callback* p) { delete p; });

        buffer = auto_calib->run_on_chip_calibration(timeout_ms, json, health, cb);
    }

    return new rs2_raw_data_buffer{ buffer };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device)

const rs2_raw_data_buffer* rs2_run_tare_calibration_cpp(rs2_device* device, float ground_truth_mm, const void* json_content, int content_size, float* health, rs2_update_progress_callback* progress_callback, int timeout_ms, rs2_error** error) BEGIN_API_CALL
{
    // Take ownership of the callback ASAP or else memory leaks could result if we throw! (the caller usually does a
    // 'new' when calling us)
    rs2_update_progress_callback_sptr callback_ptr;
    if( progress_callback )
        callback_ptr.reset( progress_callback, []( rs2_update_progress_callback * p ) { p->release(); } );

    VALIDATE_NOT_NULL(device);

    if(content_size > 0)
        VALIDATE_NOT_NULL(json_content);

    auto auto_calib = VALIDATE_INTERFACE(device->device, librealsense::auto_calibrated_interface);

    std::string json((char*)json_content, (char*)json_content + content_size);
    std::vector< uint8_t > buffer = auto_calib->run_tare_calibration( timeout_ms, ground_truth_mm, json, health, callback_ptr );

    return new rs2_raw_data_buffer{ buffer };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device)

const rs2_raw_data_buffer* rs2_run_tare_calibration(rs2_device* device, float ground_truth_mm, const void* json_content, int content_size, float * health, rs2_update_progress_callback_ptr progress_callback, void* user, int timeout_ms, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);

    if (content_size > 0)
        VALIDATE_NOT_NULL(json_content);

    auto auto_calib = VALIDATE_INTERFACE(device->device, librealsense::auto_calibrated_interface);

    std::vector<uint8_t> buffer;
    std::string json((char*)json_content, (char*)json_content + content_size);

    if (progress_callback == nullptr)
        buffer = auto_calib->run_tare_calibration(timeout_ms, ground_truth_mm, json, health, nullptr);
    else
    {
        rs2_update_progress_callback_sptr cb(new update_progress_callback(progress_callback, user),
            [](update_progress_callback* p) { delete p; });

        buffer = auto_calib->run_tare_calibration(timeout_ms, ground_truth_mm, json, health, cb);
    }
    return new rs2_raw_data_buffer{ buffer };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device)

const rs2_raw_data_buffer* rs2_process_calibration_frame(rs2_device* device, const rs2_frame* f, float* const health, rs2_update_progress_callback* progress_callback, int timeout_ms, rs2_error** error)  BEGIN_API_CALL
{
    // Take ownership of the callback ASAP or else memory leaks could result if we throw! (the caller usually does a
    // 'new' when calling us)
    rs2_update_progress_callback_sptr callback_ptr;
    if (progress_callback)
        callback_ptr.reset(progress_callback, [](rs2_update_progress_callback* p) { p->release(); });
    VALIDATE_NOT_NULL(device);

    auto auto_calib = VALIDATE_INTERFACE(device->device, librealsense::auto_calibrated_interface);

    std::vector<uint8_t> buffer;
    buffer = auto_calib->process_calibration_frame(timeout_ms, f, health, callback_ptr);

    return new rs2_raw_data_buffer{ buffer };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device)

const rs2_raw_data_buffer* rs2_get_calibration_table(const rs2_device* device, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
   
    auto auto_calib = VALIDATE_INTERFACE(device->device, librealsense::auto_calibrated_interface);
    auto buffer = auto_calib->get_calibration_table();
    return new rs2_raw_data_buffer{ buffer };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device)

void rs2_set_calibration_table(const rs2_device* device, const void* calibration, int calibration_size, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(calibration);
    VALIDATE_GT(calibration_size, 0);

    auto auto_calib = VALIDATE_INTERFACE(device->device, librealsense::auto_calibrated_interface);

    std::vector<uint8_t> buffer((uint8_t*)calibration, (uint8_t*)calibration + calibration_size);
    auto_calib->set_calibration_table(buffer);
}
HANDLE_EXCEPTIONS_AND_RETURN(,calibration, device)

rs2_raw_data_buffer* rs2_serialize_json(rs2_device* dev, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    auto serializable = VALIDATE_INTERFACE(dev->device, librealsense::serializable_interface);
    return new rs2_raw_data_buffer{ serializable->serialize_json() };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, dev)

void rs2_load_json(rs2_device* dev, const void* json_content, unsigned content_size, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(json_content);
    auto serializable = VALIDATE_INTERFACE(dev->device, librealsense::serializable_interface);
    serializable->load_json(std::string(static_cast<const char*>(json_content), content_size));
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev, json_content, content_size)

void rs2_start_collecting_fw_logs( rs2_device * dev, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( dev );
    auto fw_logger = VALIDATE_INTERFACE( dev->device, librealsense::firmware_logger_extensions );

    fw_logger->start();
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev )

void rs2_stop_collecting_fw_logs( rs2_device * dev, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( dev );
    auto fw_logger = VALIDATE_INTERFACE( dev->device, librealsense::firmware_logger_extensions );

    fw_logger->stop();
}
HANDLE_EXCEPTIONS_AND_RETURN(, dev )

rs2_firmware_log_message* rs2_create_fw_log_message(rs2_device* dev, rs2_error** error)BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    auto fw_logger = VALIDATE_INTERFACE(dev->device, librealsense::firmware_logger_extensions);
    
    return new rs2_firmware_log_message{ std::make_shared <librealsense::fw_logs::fw_logs_binary_data>() };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, dev)

int rs2_get_fw_log(rs2_device* dev, rs2_firmware_log_message* fw_log_msg, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(fw_log_msg);
    auto fw_logger = VALIDATE_INTERFACE(dev->device, librealsense::firmware_logger_extensions);

    fw_logs::fw_logs_binary_data binary_data;
    bool result = fw_logger->get_fw_log(binary_data);
    if (result)
    {
        *(fw_log_msg->firmware_log_binary_data).get() = binary_data;
    }
    return result? 1 : 0;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, dev, fw_log_msg)

int rs2_get_flash_log(rs2_device* dev, rs2_firmware_log_message* fw_log_msg, rs2_error** error)BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(fw_log_msg);
    auto fw_logger = VALIDATE_INTERFACE(dev->device, librealsense::firmware_logger_extensions);

    fw_logs::fw_logs_binary_data binary_data;
    bool result = fw_logger->get_flash_log(binary_data);
    if (result)
    {
        *(fw_log_msg->firmware_log_binary_data).get() = binary_data;
    }
    return result ? 1 : 0;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, dev, fw_log_msg)
void rs2_delete_fw_log_message(rs2_firmware_log_message* msg) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(msg);
    delete msg;
}
NOEXCEPT_RETURN(, msg)

const unsigned char* rs2_fw_log_message_data(rs2_firmware_log_message* msg, rs2_error** error)BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(msg);
    return msg->firmware_log_binary_data->logs_buffer.data();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, msg)

int rs2_fw_log_message_size(rs2_firmware_log_message* msg, rs2_error** error)BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(msg);
    return (int)msg->firmware_log_binary_data->logs_buffer.size();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, msg)

rs2_log_severity rs2_fw_log_message_severity(const rs2_firmware_log_message* msg, rs2_error** error) BEGIN_API_CALL
{
    return msg->firmware_log_binary_data->get_severity();
}
HANDLE_EXCEPTIONS_AND_RETURN(RS2_LOG_SEVERITY_NONE, msg)

unsigned int rs2_fw_log_message_timestamp(rs2_firmware_log_message* msg, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(msg);
    return msg->firmware_log_binary_data->get_timestamp();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, msg)

int rs2_init_fw_log_parser(rs2_device* dev, const char* xml_content,rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(xml_content);
    
    auto fw_logger = VALIDATE_INTERFACE(dev->device, librealsense::firmware_logger_extensions);

    return (fw_logger->init_parser(xml_content)) ? 1 : 0;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, xml_content)

rs2_firmware_log_parsed_message* rs2_create_fw_log_parsed_message(rs2_device* dev, rs2_error** error)BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);

    auto fw_logger = VALIDATE_INTERFACE(dev->device, librealsense::firmware_logger_extensions);

    return new rs2_firmware_log_parsed_message{ std::make_shared <librealsense::fw_logs::fw_log_data>() };
}
HANDLE_EXCEPTIONS_AND_RETURN(0, dev)

int rs2_parse_firmware_log(rs2_device* dev, rs2_firmware_log_message* fw_log_msg, rs2_firmware_log_parsed_message* parsed_msg, rs2_error** error)BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);
    VALIDATE_NOT_NULL(fw_log_msg);
    VALIDATE_NOT_NULL(parsed_msg);

    auto fw_logger = VALIDATE_INTERFACE(dev->device, librealsense::firmware_logger_extensions);

    bool parsing_result = fw_logger->parse_log(fw_log_msg->firmware_log_binary_data.get(),
        parsed_msg->firmware_log_parsed.get());

    return parsing_result ? 1 : 0;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, dev, fw_log_msg)

unsigned int rs2_get_number_of_fw_logs(rs2_device* dev, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(dev);

    auto fw_logger = VALIDATE_INTERFACE(dev->device, librealsense::firmware_logger_extensions);
    return fw_logger->get_number_of_fw_logs();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, dev)

void rs2_delete_fw_log_parsed_message(rs2_firmware_log_parsed_message* fw_log_parsed_msg) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(fw_log_parsed_msg);
    delete fw_log_parsed_msg;
}
NOEXCEPT_RETURN(, fw_log_parsed_msg)

const char* rs2_get_fw_log_parsed_message(rs2_firmware_log_parsed_message* fw_log_parsed_msg, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(fw_log_parsed_msg);
    return fw_log_parsed_msg->firmware_log_parsed->message.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, fw_log_parsed_msg)

const char* rs2_get_fw_log_parsed_file_name(rs2_firmware_log_parsed_message* fw_log_parsed_msg, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(fw_log_parsed_msg);
    return fw_log_parsed_msg->firmware_log_parsed->file_name.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, fw_log_parsed_msg)

const char* rs2_get_fw_log_parsed_thread_name(rs2_firmware_log_parsed_message* fw_log_parsed_msg, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(fw_log_parsed_msg);
    return fw_log_parsed_msg->firmware_log_parsed->source_name.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, fw_log_parsed_msg)

const char* rs2_get_fw_log_parsed_module_name(rs2_firmware_log_parsed_message* fw_log_parsed_msg, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(fw_log_parsed_msg);
    return fw_log_parsed_msg->firmware_log_parsed->module_name.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, fw_log_parsed_msg)


rs2_log_severity rs2_get_fw_log_parsed_severity(rs2_firmware_log_parsed_message* fw_log_parsed_msg, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(fw_log_parsed_msg);
    return fw_log_parsed_msg->firmware_log_parsed->severity;
}
HANDLE_EXCEPTIONS_AND_RETURN(RS2_LOG_SEVERITY_NONE, fw_log_parsed_msg)

unsigned int rs2_get_fw_log_parsed_line(rs2_firmware_log_parsed_message* fw_log_parsed_msg, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(fw_log_parsed_msg);
    return fw_log_parsed_msg->firmware_log_parsed->line;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, fw_log_parsed_msg)

unsigned int rs2_get_fw_log_parsed_timestamp(rs2_firmware_log_parsed_message* fw_log_parsed_msg, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(fw_log_parsed_msg);
    return fw_log_parsed_msg->firmware_log_parsed->timestamp;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, fw_log_parsed_msg)

unsigned int rs2_get_fw_log_parsed_sequence_id(rs2_firmware_log_parsed_message* fw_log_parsed_msg, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(fw_log_parsed_msg);
    return fw_log_parsed_msg->firmware_log_parsed->sequence;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, fw_log_parsed_msg)

rs2_terminal_parser* rs2_create_terminal_parser(const char* xml_content, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(xml_content);
    return new rs2_terminal_parser{ std::make_shared<librealsense::terminal_parser>(std::string(xml_content)) };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, xml_content)

void rs2_delete_terminal_parser(rs2_terminal_parser* terminal_parser) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(terminal_parser);
    delete terminal_parser;
}
NOEXCEPT_RETURN(, terminal_parser)

rs2_raw_data_buffer* rs2_terminal_parse_command(rs2_terminal_parser* terminal_parser,
    const char* command, unsigned int size_of_command, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(terminal_parser);
    VALIDATE_NOT_NULL(command);
    VALIDATE_LE(size_of_command, 1000);//bufer shall be less than 1000 bytes or similar

    std::string command_string;
    command_string.insert(command_string.begin(), command, command + size_of_command);

    auto result = terminal_parser->terminal_parser->parse_command(command_string);
    return new rs2_raw_data_buffer{ result };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, terminal_parser, command)

rs2_raw_data_buffer* rs2_terminal_parse_response(rs2_terminal_parser* terminal_parser,
    const char* command, unsigned int size_of_command,
    const void* response, unsigned int size_of_response, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(terminal_parser);
    VALIDATE_NOT_NULL(command);
    VALIDATE_NOT_NULL(response);
    VALIDATE_LE(size_of_command, 1000); //bufer shall be less than 1000 bytes or similar

    // some commands may return a longer length as a response
    //VALIDATE_LE(size_of_response, 5000);//bufer shall be less than 5000 bytes or similar


    std::string command_string;
    command_string.insert(command_string.begin(), command, command + size_of_command);

    std::vector<uint8_t> response_vec;
    response_vec.insert(response_vec.begin(), (uint8_t*)response, (uint8_t*)response + size_of_response);

    auto result = terminal_parser->terminal_parser->parse_response(command_string, response_vec);
    return new rs2_raw_data_buffer{ result };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, terminal_parser, command, response)

void rs2_project_point_to_pixel(float pixel[2], const struct rs2_intrinsics* intrin, const float point[3]) BEGIN_API_CALL
{
    float x = point[0] / point[2], y = point[1] / point[2];

    if ((intrin->model == RS2_DISTORTION_MODIFIED_BROWN_CONRADY) ||
        (intrin->model == RS2_DISTORTION_INVERSE_BROWN_CONRADY))
    {

        float r2 = x * x + y * y;
        float f = 1 + intrin->coeffs[0] * r2 + intrin->coeffs[1] * r2 * r2 + intrin->coeffs[4] * r2 * r2 * r2;
        x *= f;
        y *= f;
        float dx = x + 2 * intrin->coeffs[2] * x * y + intrin->coeffs[3] * (r2 + 2 * x * x);
        float dy = y + 2 * intrin->coeffs[3] * x * y + intrin->coeffs[2] * (r2 + 2 * y * y);
        x = dx;
        y = dy;
    }

    if (intrin->model == RS2_DISTORTION_BROWN_CONRADY)
    {
        float r2 = x * x + y * y;
        float f = 1 + intrin->coeffs[0] * r2 + intrin->coeffs[1] * r2 * r2 + intrin->coeffs[4] * r2 * r2 * r2;

        float xf = x * f;
        float yf = y * f;

        float dx = xf + 2 * intrin->coeffs[2] * x * y + intrin->coeffs[3] * (r2 + 2 * x * x);
        float dy = yf + 2 * intrin->coeffs[3] * x * y + intrin->coeffs[2] * (r2 + 2 * y * y);

        x = dx;
        y = dy;
    }

    if (intrin->model == RS2_DISTORTION_FTHETA)
    {
        float r = sqrtf(x * x + y * y);
        if (r < FLT_EPSILON)
        {
            r = FLT_EPSILON;
        }
        float rd = (float)(1.0f / intrin->coeffs[0] * atan(2 * r * tan(intrin->coeffs[0] / 2.0f)));
        x *= rd / r;
        y *= rd / r;
    }
    if (intrin->model == RS2_DISTORTION_KANNALA_BRANDT4)
    {
        float r = sqrtf(x * x + y * y);
        if (r < FLT_EPSILON)
        {
            r = FLT_EPSILON;
        }
        float theta = atan(r);
        float theta2 = theta * theta;
        float series = 1 + theta2 * (intrin->coeffs[0] + theta2 * (intrin->coeffs[1] + theta2 * (intrin->coeffs[2] + theta2 * intrin->coeffs[3])));
        float rd = theta * series;
        x *= rd / r;
        y *= rd / r;
    }

    pixel[0] = x * intrin->fx + intrin->ppx;
    pixel[1] = y * intrin->fy + intrin->ppy;
}
NOEXCEPT_RETURN(, pixel)
/* Helper inner function (not part of the API) */
inline bool is_intrinsics_distortion_zero(const struct rs2_intrinsics* intrin)
{
    return (std::fabs(intrin->coeffs[0]) < FLT_EPSILON && std::fabs(intrin->coeffs[1]) < FLT_EPSILON &&
            std::fabs(intrin->coeffs[2]) < FLT_EPSILON && std::fabs(intrin->coeffs[3]) < FLT_EPSILON &&
            std::fabs(intrin->coeffs[4]) < FLT_EPSILON);
}

void rs2_deproject_pixel_to_point(float point[3], const struct rs2_intrinsics* intrin, const float pixel[2], float depth) BEGIN_API_CALL
{
    assert(intrin->model != RS2_DISTORTION_MODIFIED_BROWN_CONRADY); // Cannot deproject from a forward-distorted image
 
    float x = (pixel[0] - intrin->ppx) / intrin->fx;
    float y = (pixel[1] - intrin->ppy) / intrin->fy;

    float xo = x;
    float yo = y;
    

    if (!is_intrinsics_distortion_zero(intrin))
    {
        if (intrin->model == RS2_DISTORTION_INVERSE_BROWN_CONRADY)
        {
            // need to loop until convergence
            // 10 iterations determined empirically
            for (int i = 0; i < 10; i++)
            {
                float r2 = x * x + y * y;
                float icdist = (float)1 / (float)(1 + ((intrin->coeffs[4] * r2 + intrin->coeffs[1]) * r2 + intrin->coeffs[0]) * r2);
                float xq = x / icdist;
                float yq = y / icdist;
                float delta_x = 2 * intrin->coeffs[2] * xq * yq + intrin->coeffs[3] * (r2 + 2 * xq * xq);
                float delta_y = 2 * intrin->coeffs[3] * xq * yq + intrin->coeffs[2] * (r2 + 2 * yq * yq);
                x = (xo - delta_x) * icdist;
                y = (yo - delta_y) * icdist;
            }
        }
        if (intrin->model == RS2_DISTORTION_BROWN_CONRADY)
        {
            // need to loop until convergence
            // 10 iterations determined empirically
            for (int i = 0; i < 10; i++)
            {
                float r2 = x * x + y * y;
                float icdist = (float)1 / (float)(1 + ((intrin->coeffs[4] * r2 + intrin->coeffs[1]) * r2 + intrin->coeffs[0]) * r2);
                float delta_x = 2 * intrin->coeffs[2] * x * y + intrin->coeffs[3] * (r2 + 2 * x * x);
                float delta_y = 2 * intrin->coeffs[3] * x * y + intrin->coeffs[2] * (r2 + 2 * y * y);
                x = (xo - delta_x) * icdist;
                y = (yo - delta_y) * icdist;
            }

        }
    }
    if (intrin->model == RS2_DISTORTION_KANNALA_BRANDT4)
    {
        float rd = sqrtf(x * x + y * y);
        if (rd < FLT_EPSILON)
        {
            rd = FLT_EPSILON;
        }

        float theta = rd;
        float theta2 = rd * rd;
        for (int i = 0; i < 4; i++)
        {
            float f = theta * (1 + theta2 * (intrin->coeffs[0] + theta2 * (intrin->coeffs[1] + theta2 * (intrin->coeffs[2] + theta2 * intrin->coeffs[3])))) - rd;
            if (std::fabs(f) < FLT_EPSILON)
            {
                break;
            }
            float df = 1 + theta2 * (3 * intrin->coeffs[0] + theta2 * (5 * intrin->coeffs[1] + theta2 * (7 * intrin->coeffs[2] + 9 * theta2 * intrin->coeffs[3])));
            theta -= f / df;
            theta2 = theta * theta;
        }
        float r = tan(theta);
        x *= r / rd;
        y *= r / rd;
    }
    if (intrin->model == RS2_DISTORTION_FTHETA)
    {
        float rd = sqrtf(x * x + y * y);
        if (rd < FLT_EPSILON)
        {
            rd = FLT_EPSILON;
        }
        float r = (std::fabs(intrin->coeffs[0]) < FLT_EPSILON) ? 0.0f : (float)(tan(intrin->coeffs[0] * rd) / atan(2 * tan(intrin->coeffs[0] / 2.0f)));
        x *= r / rd;
        y *= r / rd;
    }

    point[0] = depth * x;
    point[1] = depth * y;
    point[2] = depth;
}
NOEXCEPT_RETURN(, point)

void rs2_transform_point_to_point(float to_point[3], const struct rs2_extrinsics* extrin, const float from_point[3]) BEGIN_API_CALL
{
    to_point[0] = extrin->rotation[0] * from_point[0] + extrin->rotation[3] * from_point[1] + extrin->rotation[6] * from_point[2] + extrin->translation[0];
    to_point[1] = extrin->rotation[1] * from_point[0] + extrin->rotation[4] * from_point[1] + extrin->rotation[7] * from_point[2] + extrin->translation[1];
    to_point[2] = extrin->rotation[2] * from_point[0] + extrin->rotation[5] * from_point[1] + extrin->rotation[8] * from_point[2] + extrin->translation[2];
}
NOEXCEPT_RETURN(, to_point)

void rs2_fov(const struct rs2_intrinsics* intrin, float to_fov[2]) BEGIN_API_CALL
{
    to_fov[0] = (atan2f(intrin->ppx + 0.5f, intrin->fx) + atan2f(intrin->width - (intrin->ppx + 0.5f), intrin->fx)) * 57.2957795f;
    to_fov[1] = (atan2f(intrin->ppy + 0.5f, intrin->fy) + atan2f(intrin->height - (intrin->ppy + 0.5f), intrin->fy)) * 57.2957795f;
}
NOEXCEPT_RETURN(, to_fov)

/* Helper inner function (not part of the API) */
void next_pixel_in_line(float curr[2], const float start[2], const float end[2])
{
    float line_slope = (end[1] - start[1]) / (end[0] - start[0]);
    if (fabs(end[0] - curr[0]) > fabs(end[1] - curr[1]))
    {
        curr[0] = end[0] > curr[0] ? curr[0] + 1 : curr[0] - 1;
        curr[1] = end[1] - line_slope * (end[0] - curr[0]);
    }
    else
    {
        curr[1] = end[1] > curr[1] ? curr[1] + 1 : curr[1] - 1;
        curr[0] = end[0] - ((end[1] - curr[1]) / line_slope);
    }
}

/* Helper inner function (not part of the API) */
bool is_pixel_in_line(const float curr[2], const float start[2], const float end[2])
{
    return ((end[0] >= start[0] && end[0] >= curr[0] && curr[0] >= start[0]) || (end[0] <= start[0] && end[0] <= curr[0] && curr[0] <= start[0])) &&
        ((end[1] >= start[1] && end[1] >= curr[1] && curr[1] >= start[1]) || (end[1] <= start[1] && end[1] <= curr[1] && curr[1] <= start[1]));
}

/* Helper inner function (not part of the API) */
void adjust_2D_point_to_boundary(float p[2], int width, int height)
{
    if (p[0] < 0) p[0] = 0;
    if (p[0] >= width) p[0] = (float)width - 1;
    if (p[1] < 0) p[1] = 0;
    if (p[1] >= height) p[1] = (float)height - 1;
}


void rs2_project_color_pixel_to_depth_pixel(float to_pixel[2],
    const uint16_t* data, float depth_scale,
    float depth_min, float depth_max,
    const struct rs2_intrinsics* depth_intrin,
    const struct rs2_intrinsics* color_intrin,
    const struct rs2_extrinsics* color_to_depth,
    const struct rs2_extrinsics* depth_to_color,
    const float from_pixel[2]) BEGIN_API_CALL
{
    //Find line start pixel
    float start_pixel[2] = { 0 }, min_point[3] = { 0 }, min_transformed_point[3] = { 0 };
    rs2_deproject_pixel_to_point(min_point, color_intrin, from_pixel, depth_min);
    rs2_transform_point_to_point(min_transformed_point, color_to_depth, min_point);
    rs2_project_point_to_pixel(start_pixel, depth_intrin, min_transformed_point);
    adjust_2D_point_to_boundary(start_pixel, depth_intrin->width, depth_intrin->height);

    //Find line end depth pixel
    float end_pixel[2] = { 0 }, max_point[3] = { 0 }, max_transformed_point[3] = { 0 };
    rs2_deproject_pixel_to_point(max_point, color_intrin, from_pixel, depth_max);
    rs2_transform_point_to_point(max_transformed_point, color_to_depth, max_point);
    rs2_project_point_to_pixel(end_pixel, depth_intrin, max_transformed_point);
    adjust_2D_point_to_boundary(end_pixel, depth_intrin->width, depth_intrin->height);

    //search along line for the depth pixel that it's projected pixel is the closest to the input pixel
    float min_dist = -1;
    for (float p[2] = { start_pixel[0], start_pixel[1] }; is_pixel_in_line(p, start_pixel, end_pixel); next_pixel_in_line(p, start_pixel, end_pixel))
    {
        float depth = depth_scale * data[(int)p[1] * depth_intrin->width + (int)p[0]];
        if (depth == 0) {
            if (p[0] == end_pixel[0] && p[1] == end_pixel[1]) {
                break;
            } else {
                continue;
            }
        }

        float projected_pixel[2] = { 0 }, point[3] = { 0 }, transformed_point[3] = { 0 };
        rs2_deproject_pixel_to_point(point, depth_intrin, p, depth);
        rs2_transform_point_to_point(transformed_point, depth_to_color, point);
        rs2_project_point_to_pixel(projected_pixel, color_intrin, transformed_point);

        float new_dist = (float)(pow((projected_pixel[1] - from_pixel[1]), 2) + pow((projected_pixel[0] - from_pixel[0]), 2));
        if (new_dist < min_dist || min_dist < 0)
        {
            min_dist = new_dist;
            to_pixel[0] = p[0];
            to_pixel[1] = p[1];
        }

        // this is needed to avoid floating point precision issues when p is equal to end_pixel
        // in this case the next_pixel_in_line can jump back and forth and cause an infinite loop
        // example: start=(0,0), end=(30,2)
        if (p[0] == end_pixel[0] && p[1] == end_pixel[1]) {
            break;
        }
    }
}
NOEXCEPT_RETURN(, to_pixel)

const rs2_raw_data_buffer* rs2_run_focal_length_calibration_cpp(rs2_device* device, rs2_frame_queue* left, rs2_frame_queue* right, float target_w, float target_h, 
    int adjust_both_sides, float* ratio, float* angle, rs2_update_progress_callback * progress_callback, rs2_error** error) BEGIN_API_CALL
{
    // Take ownership of the callback ASAP or else memory leaks could result if we throw! (the caller usually does a
    // 'new' when calling us)
    rs2_update_progress_callback_sptr callback_ptr;
    if( progress_callback )
        callback_ptr.reset( progress_callback, []( rs2_update_progress_callback * p ) { p->release(); } );

    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(left);
    VALIDATE_NOT_NULL(right);
    VALIDATE_NOT_NULL(ratio);
    VALIDATE_NOT_NULL(angle);
    VALIDATE_GT(rs2_frame_queue_size(left, error), 0);
    VALIDATE_GT(rs2_frame_queue_size(right, error), 0);
    VALIDATE_GT(target_w, 0.f);
    VALIDATE_GT(target_h, 0.f);
    VALIDATE_RANGE(adjust_both_sides, 0, 1);

    auto auto_calib = VALIDATE_INTERFACE(device->device, librealsense::auto_calibrated_interface);
    std::vector< uint8_t > buffer =
        auto_calib->run_focal_length_calibration( left, right, target_w, target_h, adjust_both_sides, ratio, angle, callback_ptr );

    return new rs2_raw_data_buffer{ buffer };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device, left, right, ratio, angle)

const rs2_raw_data_buffer* rs2_run_focal_length_calibration(rs2_device* device, rs2_frame_queue* left, rs2_frame_queue* right, float target_w, float target_h,
    int adjust_both_sides, float* ratio, float* angle, rs2_update_progress_callback_ptr callback, void* client_data, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(left);
    VALIDATE_NOT_NULL(right);
    VALIDATE_NOT_NULL(ratio);
    VALIDATE_NOT_NULL(angle);
    VALIDATE_GT(rs2_frame_queue_size(left, error), 0);
    VALIDATE_GT(rs2_frame_queue_size(right, error), 0);
    VALIDATE_GT(target_w, 0.f);
    VALIDATE_GT(target_h, 0.f);
    VALIDATE_RANGE(adjust_both_sides, 0, 1);

    auto auto_calib = VALIDATE_INTERFACE(device->device, librealsense::auto_calibrated_interface);
    std::vector<uint8_t> buffer;
    if (callback == nullptr)
        buffer = auto_calib->run_focal_length_calibration(left, right, target_w, target_h, adjust_both_sides, ratio, angle, nullptr);
    else
    {
        rs2_update_progress_callback_sptr cb(new update_progress_callback(callback, client_data), [](update_progress_callback* p) { delete p; });
        buffer = auto_calib->run_focal_length_calibration(left, right, target_w, target_h, adjust_both_sides, ratio, angle, cb);
    }

    return new rs2_raw_data_buffer{ buffer };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device, left, right, ratio, angle)

const rs2_raw_data_buffer* rs2_run_uv_map_calibration_cpp(rs2_device* device, rs2_frame_queue* left, rs2_frame_queue* color, rs2_frame_queue* depth, int py_px_only,
    float* health, int health_size, rs2_update_progress_callback* progress_callback, rs2_error** error) BEGIN_API_CALL
{
    // Take ownership of the callback ASAP or else memory leaks could result if we throw! (the caller usually does a
    // 'new' when calling us)
    rs2_update_progress_callback_sptr callback_ptr;
    if( progress_callback )
        callback_ptr.reset( progress_callback, []( rs2_update_progress_callback * p ) { p->release(); } );

    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(left);
    VALIDATE_NOT_NULL(color);
    VALIDATE_NOT_NULL(depth);
    VALIDATE_NOT_NULL(health);
    VALIDATE_GT(health_size, 0);
    VALIDATE_GT(rs2_frame_queue_size(left, error), 0);
    VALIDATE_GT(rs2_frame_queue_size(color, error), 0);
    VALIDATE_GT(rs2_frame_queue_size(depth, error), 0);
    VALIDATE_RANGE(py_px_only, 0, 1);

    auto auto_calib = VALIDATE_INTERFACE(device->device, librealsense::auto_calibrated_interface);
    std::vector< uint8_t > buffer = auto_calib->run_uv_map_calibration( left, color, depth, py_px_only, health, health_size, callback_ptr );

    return new rs2_raw_data_buffer{ buffer };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device, left, color, depth, health)

const rs2_raw_data_buffer* rs2_run_uv_map_calibration(rs2_device* device, rs2_frame_queue* left, rs2_frame_queue* color, rs2_frame_queue* depth, int py_px_only,
    float* health, int health_size, rs2_update_progress_callback_ptr callback, void* client_data, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(left);
    VALIDATE_NOT_NULL(color);
    VALIDATE_NOT_NULL(depth);
    VALIDATE_NOT_NULL(health);
    VALIDATE_GT(health_size, 0);
    VALIDATE_GT(rs2_frame_queue_size(left, error), 0);
    VALIDATE_GT(rs2_frame_queue_size(color, error), 0);
    VALIDATE_GT(rs2_frame_queue_size(depth, error), 0);
    VALIDATE_RANGE(py_px_only, 0, 1);

    auto auto_calib = VALIDATE_INTERFACE(device->device, librealsense::auto_calibrated_interface);
    std::vector<uint8_t> buffer;
    if (callback == nullptr)
        buffer = auto_calib->run_uv_map_calibration(left, color, depth, py_px_only, health, health_size, nullptr);
    else
    {
        rs2_update_progress_callback_sptr cb(new update_progress_callback(callback, client_data), [](update_progress_callback* p) { delete p; });
        buffer = auto_calib->run_uv_map_calibration(left, color, depth, py_px_only, health, health_size, cb);
    }

    return new rs2_raw_data_buffer{ buffer };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device, left, color, depth, health)


float rs2_calculate_target_z_cpp(rs2_device* device, rs2_frame_queue* queue1, rs2_frame_queue* queue2, rs2_frame_queue* queue3,
    float target_width, float target_height, rs2_update_progress_callback* progress_callback, rs2_error** error) BEGIN_API_CALL
{
    // Take ownership of the callback ASAP or else memory leaks could result if we throw! (the caller usually does a
    // 'new' when calling us)
    rs2_update_progress_callback_sptr callback_ptr;
    if( progress_callback )
        callback_ptr.reset( progress_callback, []( rs2_update_progress_callback * p ) { p->release(); } );

    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(queue1);
    VALIDATE_NOT_NULL(queue2);
    VALIDATE_NOT_NULL(queue3);
    VALIDATE_GT(target_width, 0.f);
    VALIDATE_GT(target_height, 0.f);
    VALIDATE_GT(rs2_frame_queue_size(queue1, error), 0); // Queues 2-3 are optional, can be empty

    auto auto_calib = VALIDATE_INTERFACE(device->device, librealsense::auto_calibrated_interface);
    return auto_calib->calculate_target_z(queue1, queue2, queue3, target_width, target_height, callback_ptr );
}
HANDLE_EXCEPTIONS_AND_RETURN(-1.f, device, queue1, queue2, queue3, target_width, target_height)

float rs2_calculate_target_z(rs2_device* device, rs2_frame_queue* queue1, rs2_frame_queue* queue2, rs2_frame_queue* queue3,
    float target_width, float target_height, rs2_update_progress_callback_ptr progress_callback, void* client_data, rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(queue1);
    VALIDATE_NOT_NULL(queue2);
    VALIDATE_NOT_NULL(queue3);
    VALIDATE_GT(target_width, 0.f);
    VALIDATE_GT(target_height, 0.f);
    VALIDATE_GT(rs2_frame_queue_size(queue1, error), 0); // Queues 2-3 are optional, can be empty


    auto auto_calib = VALIDATE_INTERFACE(device->device, librealsense::auto_calibrated_interface);

    if (progress_callback == NULL)
        return auto_calib->calculate_target_z(queue1, queue2, queue3, target_width, target_height, nullptr);
    else
    {
        rs2_update_progress_callback_sptr cb(new update_progress_callback(progress_callback, client_data), [](update_progress_callback* p) { delete p; });
        return auto_calib->calculate_target_z(queue1, queue2, queue3, target_width, target_height, cb);
    }
}
HANDLE_EXCEPTIONS_AND_RETURN(-1.f, device, queue1, queue2, queue3, target_width, target_height)

const rs2_raw_data_buffer* rs2_get_calibration_config(
    rs2_device* device,
    rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    auto auto_calib = VALIDATE_INTERFACE(device->device, librealsense::auto_calibrated_interface);
    auto ret_str = auto_calib->get_calibration_config();
    std::vector<uint8_t> vec(ret_str.begin(), ret_str.end());
    return new rs2_raw_data_buffer{ std::move(vec) };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device)

void rs2_set_calibration_config(
    rs2_device* device,
    const char* calibration_config_json_str,
    rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(calibration_config_json_str);
    auto auto_calib = VALIDATE_INTERFACE(device->device, librealsense::auto_calibrated_interface);
    auto_calib->set_calibration_config(calibration_config_json_str);
}
HANDLE_EXCEPTIONS_AND_RETURN(, device, calibration_config_json_str)

const rs2_raw_data_buffer* rs2_get_safety_preset(
    const rs2_sensor* sensor,
    int index,
    rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    VALIDATE_RANGE(index, 0, 63);
    auto safety_sensor = VALIDATE_INTERFACE(sensor->sensor, librealsense::safety_sensor);
    auto ret_str = safety_sensor->get_safety_preset(index);
    std::vector<uint8_t> vec(ret_str.begin(), ret_str.end());
    return new rs2_raw_data_buffer{ std::move(vec) };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, sensor, index)

void rs2_set_safety_preset(
    const rs2_sensor* sensor,
    int index,
    const char* sp_json_str,
    rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    VALIDATE_RANGE(index, 0, 63);
    VALIDATE_NOT_NULL(sp_json_str);
    auto safety_sensor = VALIDATE_INTERFACE(sensor->sensor, librealsense::safety_sensor);
    safety_sensor->set_safety_preset(index, sp_json_str);
}
HANDLE_EXCEPTIONS_AND_RETURN(, sensor, index, sp_json_str)

const rs2_raw_data_buffer* rs2_get_safety_interface_config(
    const rs2_sensor* sensor,
    rs2_calib_location loc,
    rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    VALIDATE_RANGE(loc, RS2_CALIB_LOCATION_FIRST, RS2_CALIB_LOCATION_COUNT);
    auto safety_sensor = VALIDATE_INTERFACE(sensor->sensor, librealsense::safety_sensor);
    auto ret_str = safety_sensor->get_safety_interface_config(loc);
    std::vector<uint8_t> vec(ret_str.begin(), ret_str.end());
    return new rs2_raw_data_buffer{ std::move(vec) };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, sensor, loc)

void rs2_set_safety_interface_config(
    const rs2_sensor* sensor,
    const char* sic_json_str,
    rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    VALIDATE_NOT_NULL(sic_json_str);
    auto safety_sensor = VALIDATE_INTERFACE(sensor->sensor, librealsense::safety_sensor);
    safety_sensor->set_safety_interface_config(sic_json_str);
}
HANDLE_EXCEPTIONS_AND_RETURN(, sensor, sic_json_str)

const rs2_raw_data_buffer* rs2_get_application_config(
    rs2_sensor const* sensor,
    rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    auto safety_sensor = VALIDATE_INTERFACE(sensor->sensor, librealsense::safety_sensor);
    auto ret_str = safety_sensor->get_application_config();
    std::vector<uint8_t> vec(ret_str.begin(), ret_str.end());
    return new rs2_raw_data_buffer{ std::move(vec) };
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, sensor)

void rs2_set_application_config(
    rs2_sensor const* sensor,
    const char* application_config_json_str,
    rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(sensor);
    VALIDATE_NOT_NULL(application_config_json_str);
    auto safety_sensor = VALIDATE_INTERFACE(sensor->sensor, librealsense::safety_sensor);
    safety_sensor->set_application_config(application_config_json_str);
}
HANDLE_EXCEPTIONS_AND_RETURN(, sensor, application_config_json_str)

void rs2_hw_monitor_get_opcode_string(int opcode, char* buffer, size_t buffer_size,
    rs2_device* device,
    rs2_error** error) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL(device);
    auto device_interface = VALIDATE_INTERFACE(device->device, librealsense::debug_interface);
    strncpy(buffer, device_interface->get_opcode_string(opcode).c_str(), buffer_size);
}
HANDLE_EXCEPTIONS_AND_RETURN(, device)

int rs2_supports_eth_config( const rs2_device * device, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( device );
    auto eth_config = VALIDATE_INTERFACE_NO_THROW( device->device, librealsense::eth_config_device );
    if( eth_config )
        return eth_config->supports_ethernet_configuration();

    return false;
}
HANDLE_EXCEPTIONS_AND_RETURN( false, device )

uint32_t rs2_get_link_speed( const rs2_device * device, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( device );
    auto eth_config = VALIDATE_INTERFACE( device->device, librealsense::eth_config_device );
    return eth_config->get_ethernet_link_speed();
}
HANDLE_EXCEPTIONS_AND_RETURN( 0, device )

rs2_eth_link_priority rs2_get_link_priority( const rs2_device * device, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( device );
    auto eth_config = VALIDATE_INTERFACE( device->device, librealsense::eth_config_device );
    return static_cast< rs2_eth_link_priority >( eth_config->get_link_priority() );
}
HANDLE_EXCEPTIONS_AND_RETURN( RS2_LINK_PRIORITY_COUNT, device )

void rs2_set_link_priority( const rs2_device * device, rs2_eth_link_priority priority, rs2_error ** error ) BEGIN_API_CALL
{

    VALIDATE_NOT_NULL( device );
    auto eth_config = VALIDATE_INTERFACE( device->device, librealsense::eth_config_device );
    return eth_config->set_link_priority( static_cast< rsutils::type::link_priority >( priority ) );
}
HANDLE_EXCEPTIONS_AND_RETURN( , device )

unsigned int rs2_get_link_timeout( const rs2_device * device, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( device );
    auto eth_config = VALIDATE_INTERFACE( device->device, librealsense::eth_config_device );
    return eth_config->get_link_timeout();
}
HANDLE_EXCEPTIONS_AND_RETURN( 0, device )

void rs2_set_link_timeout( const rs2_device * device, unsigned int timeout, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( device );
    auto eth_config = VALIDATE_INTERFACE( device->device, librealsense::eth_config_device );
    return eth_config->set_link_timeout( timeout );
}
HANDLE_EXCEPTIONS_AND_RETURN( , device )

unsigned int rs2_get_dds_domain( const rs2_device * device, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( device );
    auto eth_config = VALIDATE_INTERFACE( device->device, librealsense::eth_config_device );
    return static_cast< rs2_eth_link_priority >( eth_config->get_dds_domain_id() );
}
HANDLE_EXCEPTIONS_AND_RETURN( 0, device )

void rs2_set_dds_domain( const rs2_device * device, unsigned int domain, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( device );
    auto eth_config = VALIDATE_INTERFACE( device->device, librealsense::eth_config_device );
    return eth_config->set_dds_domain_id( domain );
}
HANDLE_EXCEPTIONS_AND_RETURN(, device )

void rs2_get_ip_address( const rs2_device * device, rs2_ip_address configured, rs2_ip_address actual, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( device );
    auto eth_config = VALIDATE_INTERFACE( device->device, librealsense::eth_config_device );
    rsutils::type::ip_address tmp_configured, tmp_actual;
    eth_config->get_ip_address( tmp_configured, tmp_actual );
    tmp_configured.get_components( configured );
    tmp_actual.get_components( actual );
}
HANDLE_EXCEPTIONS_AND_RETURN( , device )

void rs2_set_ip_address( const rs2_device * device, const rs2_ip_address ip, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( device );
    auto eth_config = VALIDATE_INTERFACE( device->device, librealsense::eth_config_device );
    rsutils::type::ip_address tmp( ip );
    eth_config->set_ip_address( tmp );
}
HANDLE_EXCEPTIONS_AND_RETURN(, device )

void rs2_get_netmask( const rs2_device * device, rs2_ip_address configured, rs2_ip_address actual, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( device );
    auto eth_config = VALIDATE_INTERFACE( device->device, librealsense::eth_config_device );
    rsutils::type::ip_address tmp_configured, tmp_actual;
    eth_config->get_netmask( tmp_configured, tmp_actual );
    tmp_configured.get_components( configured );
    tmp_actual.get_components( actual );
}
HANDLE_EXCEPTIONS_AND_RETURN( , device )

void rs2_set_netmask( const rs2_device * device, const rs2_ip_address netmask, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( device );
    auto eth_config = VALIDATE_INTERFACE( device->device, librealsense::eth_config_device );
    rsutils::type::ip_address tmp( netmask );
    eth_config->set_netmask( tmp );
}
HANDLE_EXCEPTIONS_AND_RETURN( , device )

void rs2_get_gateway( const rs2_device * device, rs2_ip_address configured, rs2_ip_address actual, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( device );
    auto eth_config = VALIDATE_INTERFACE( device->device, librealsense::eth_config_device );
    rsutils::type::ip_address tmp_configured, tmp_actual;
    eth_config->get_gateway( tmp_configured, tmp_actual );
    tmp_configured.get_components( configured );
    tmp_actual.get_components( actual );
}
HANDLE_EXCEPTIONS_AND_RETURN( , device )

void rs2_set_gateway( const rs2_device * device, const rs2_ip_address gateway, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( device );
    auto eth_config = VALIDATE_INTERFACE( device->device, librealsense::eth_config_device );
    rsutils::type::ip_address tmp( gateway );
    eth_config->set_gateway( gateway);
}
HANDLE_EXCEPTIONS_AND_RETURN( , device )

void rs2_get_dhcp_config( const rs2_device * device, int * enabled, unsigned int * timeout, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( device );
    auto eth_config = VALIDATE_INTERFACE( device->device, librealsense::eth_config_device );
    bool tmp_enabled;
    uint8_t tmp_timeout;
    eth_config->get_dhcp_config( tmp_enabled, tmp_timeout );
    *enabled = tmp_enabled ? 1 : 0;
    *timeout = tmp_timeout;
}
HANDLE_EXCEPTIONS_AND_RETURN( , device )

void rs2_set_dhcp_config( const rs2_device * device, int enabled, unsigned int timeout, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( device );
    auto eth_config = VALIDATE_INTERFACE( device->device, librealsense::eth_config_device );
    VALIDATE_LE( timeout, 255 );
    eth_config->set_dhcp_config( enabled != 0, static_cast< uint8_t >( timeout ) );
}
HANDLE_EXCEPTIONS_AND_RETURN( , device )

unsigned int rs2_get_mtu( const rs2_device * device, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( device );
    auto eth_config = VALIDATE_INTERFACE( device->device, librealsense::eth_config_device );
    return eth_config->get_mtu();
}
HANDLE_EXCEPTIONS_AND_RETURN( 9000, device )

void rs2_set_mtu( const rs2_device * device, unsigned int mtu, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( device );
    auto eth_config = VALIDATE_INTERFACE( device->device, librealsense::eth_config_device );
    return eth_config->set_mtu( mtu );
}
HANDLE_EXCEPTIONS_AND_RETURN( , device )

unsigned int rs2_get_transmission_delay( const rs2_device * device, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( device );
    auto eth_config = VALIDATE_INTERFACE( device->device, librealsense::eth_config_device );
    return eth_config->get_transmission_delay();
}
HANDLE_EXCEPTIONS_AND_RETURN( 0, device )

void rs2_set_transmission_delay( const rs2_device * device, unsigned int delay, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( device );
    auto eth_config = VALIDATE_INTERFACE( device->device, librealsense::eth_config_device );
    return eth_config->set_transmission_delay( delay );
}
HANDLE_EXCEPTIONS_AND_RETURN( , device )

void rs2_restore_default_eth_config( const rs2_device * device, rs2_error ** error ) BEGIN_API_CALL
{
    VALIDATE_NOT_NULL( device );
    auto eth_config = VALIDATE_INTERFACE( device->device, librealsense::eth_config_device );
    return eth_config->restore_defaults();
}
HANDLE_EXCEPTIONS_AND_RETURN(, device )
