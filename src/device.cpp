// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#include "device.h"

#include "environment.h"
#include "core/video.h"
#include "core/motion.h"
#include "core/frame-holder.h"
#include "sync.h"
#include "context.h"  // rs2_device_info
#include "core/sensor-interface.h"

#include <rsutils/string/from.h>
#include <rsutils/json.h>

using namespace librealsense;


stream_interface *
librealsense::find_profile( rs2_stream stream, int index, std::vector< stream_interface * > const & profiles )
{
    auto prof = std::find_if(profiles.begin(), profiles.end(), [&](stream_interface* profile)
    {
        return profile->get_stream_type() == stream && ( index == -1 || profile->get_stream_index() == index );
    });

    if (prof != profiles.end())
        return *prof;
    else
        return nullptr;
}


device::device( std::shared_ptr< const device_info > const & dev_info,
                bool device_changed_notifications )
    : _dev_info( dev_info )
    , _is_alive( std::make_shared< std::atomic< bool > >( true ) )
    , _profiles_tags( [this]() { return get_profiles_tags(); } )
    , _format_conversion(
          [this]
          {
              auto context = get_context();
              if( ! context )
                  return format_conversion::full;
              std::string const format_conversion( "format-conversion", 17 );
              std::string const full( "full", 4 );
              auto const value = context->get_settings().nested( format_conversion ).default_value( full );
              if( value == full )
                  return format_conversion::full;
              if( value == "basic" )
                  return format_conversion::basic;
              if( value == "raw" )
                  return format_conversion::raw;
              throw invalid_value_exception( "invalid " + format_conversion + " value '" + value + "'" );
          } )
{
    if( device_changed_notifications )
    {
        std::weak_ptr< std::atomic< bool > > weak_alive = _is_alive;
        std::weak_ptr< const device_info > weak_dev_info = _dev_info;
        _device_change_subscription = get_context()->on_device_changes(
            [weak_alive, weak_dev_info]( std::vector< std::shared_ptr< device_info > > const & removed,
                                         std::vector< std::shared_ptr< device_info > > const & added )
            {
                // The callback can be called from one thread while the object is being destroyed by another.
                // Check if members can still be accessed.
                auto alive = weak_alive.lock();
                if( ! alive || ! *alive )
                    return;
                auto this_dev_info = weak_dev_info.lock();
                if( ! this_dev_info )
                    return;

                // Update is_valid variable when device is invalid
                for( auto & dev_info : removed )
                {
                    if( dev_info->is_same_as( this_dev_info ) )
                    {
                        *alive = false;
                        return;
                    }
                }
            } );
    }
}

device::~device()
{
    _sensors.clear();
}

int device::add_sensor(const std::shared_ptr<sensor_interface>& sensor_base)
{
    _sensors.push_back(sensor_base);
    return (int)_sensors.size() - 1;
}

int device::assign_sensor(const std::shared_ptr<sensor_interface>& sensor_base, uint8_t idx)
{
    try
    {
        _sensors[idx] = sensor_base;
        return (int)_sensors.size() - 1;
    }
    catch (std::out_of_range)
    {
        throw invalid_value_exception( rsutils::string::from()
                                       << "Cannot assign sensor - invalid subdevice value" << idx );
    }
}

size_t device::get_sensors_count() const
{
    return static_cast<unsigned int>(_sensors.size());
}

sensor_interface& device::get_sensor(size_t subdevice)
{
    try
    {
        return *(_sensors.at(subdevice));
    }
    catch (std::out_of_range)
    {
        throw invalid_value_exception("invalid subdevice value");
    }
}

size_t device::find_sensor_idx(const sensor_interface& s) const
{
    int idx = 0;
    for (auto&& sensor : _sensors)
    {
        if (&s == sensor.get()) return idx;
        idx++;
    }
    throw std::runtime_error("Sensor not found!");
}

const sensor_interface& device::get_sensor(size_t subdevice) const
{
    try
    {
        return *(_sensors.at(subdevice));
    }
    catch (std::out_of_range)
    {
        throw invalid_value_exception("invalid subdevice value");
    }
}

void device::hardware_reset()
{
    throw not_implemented_exception( rsutils::string::from()
                                     << __FUNCTION__ << " is not implemented for this device!" );
}

std::shared_ptr<matcher> device::create_matcher(const frame_holder& frame) const
{

    return std::make_shared<identity_matcher>( frame.frame->get_stream()->get_unique_id(), frame.frame->get_stream()->get_stream_type());
}

std::pair<uint32_t, rs2_extrinsics> device::get_extrinsics(const stream_interface& stream) const
{
    auto stream_index = stream.get_unique_id();
    auto pair = _extrinsics.at(stream_index);
    auto pin_stream = pair.second;
    rs2_extrinsics ext{};
    if (environment::get_instance().get_extrinsics_graph().try_fetch_extrinsics(*pin_stream, stream, &ext) == false)
    {
        throw std::runtime_error( rsutils::string::from()
                                  << "Failed to fetch extrinsics between pin stream (" << pin_stream->get_unique_id()
                                  << ") to given stream (" << stream.get_unique_id() << ")" );
    }
    return std::make_pair(pair.first, ext);
}

void device::register_stream_to_extrinsic_group(const stream_interface& stream, uint32_t group_index)
{
    auto iter = std::find_if(_extrinsics.begin(),
                           _extrinsics.end(),
                           [group_index](const std::pair<int, std::pair<uint32_t, std::shared_ptr<const stream_interface>>>& p) { return p.second.first == group_index; });
    if (iter == _extrinsics.end())
    {
        //First stream to register for this group
        _extrinsics[stream.get_unique_id()] = std::make_pair(group_index, stream.shared_from_this());
    }
    else
    {
        //iter->second holds the group_id and the key stream
        _extrinsics[stream.get_unique_id()] = iter->second;
    }
}

std::vector< rs2_format > device::map_supported_color_formats( rs2_format source_format, bool should_map_source_format )
{
    // Mapping from source color format to all of the compatible target color formats.

    std::vector<rs2_format> target_formats = { RS2_FORMAT_RGB8, RS2_FORMAT_RGBA8, RS2_FORMAT_BGR8, RS2_FORMAT_BGRA8 };
    switch (source_format)
    {
    case RS2_FORMAT_M420:
        target_formats.push_back(RS2_FORMAT_M420);
        target_formats.push_back(RS2_FORMAT_Y16);
        target_formats.push_back(RS2_FORMAT_Y8);
        break;
    case RS2_FORMAT_YUYV:
        break;
    case RS2_FORMAT_UYVY:
        break;
    default:
        LOG_ERROR("Format is not supported for mapping");
    }

    if( should_map_source_format )
        target_formats.push_back( source_format );

    return target_formats;
}

format_conversion device::get_format_conversion() const
{
    return *_format_conversion;
}

void device::tag_profiles(stream_profiles profiles) const
{
    for (auto profile : profiles)
    {
        for (auto tag : *_profiles_tags)
        {
            if (auto vp = dynamic_cast<video_stream_profile_interface*>(profile.get()))
            {
                if ((tag.stream == RS2_STREAM_ANY || vp->get_stream_type() == tag.stream) &&
                    (tag.format == RS2_FORMAT_ANY || vp->get_format() == tag.format) &&
                    (tag.width == -1 || vp->get_width() == tag.width) &&
                    (tag.height == -1 || vp->get_height() == tag.height) &&
                    (tag.fps == -1 || vp->get_framerate() == tag.fps) &&
                    (tag.stream_index == -1 || vp->get_stream_index() == tag.stream_index))
                    profile->tag_profile(tag.tag);
            }
            else
            if (auto mp = dynamic_cast<motion_stream_profile_interface*>(profile.get()))
            {
                if ((tag.stream == RS2_STREAM_ANY || mp->get_stream_type() == tag.stream) &&
                    (tag.format == RS2_FORMAT_ANY || mp->get_format() == tag.format) &&
                    (tag.fps == -1 || mp->get_framerate() == tag.fps) &&
                    (tag.stream_index == -1 || mp->get_stream_index() == tag.stream_index))
                    profile->tag_profile(tag.tag);
            }
        }
    }
}

bool device::contradicts(const stream_profile_interface* a, const std::vector<stream_profile>& others) const
{
    if (auto vid_a = dynamic_cast<const video_stream_profile_interface*>(a))
    {
        for (auto request : others)
        {
            if (a->get_framerate() != 0 && request.fps != 0 && (a->get_framerate() != request.fps))
                return true;
            if (vid_a->get_width() != 0 && request.width != 0 && (vid_a->get_width() != request.width))
                return true;
            if (vid_a->get_height() != 0 && request.height != 0 && (vid_a->get_height() != request.height))
                return true;
        }
    }
    return false;
}

void device::stop_activity() const
{
    for (auto& sensor : _sensors)
    {
        auto snr_name = (sensor->supports_info(RS2_CAMERA_INFO_NAME)) ? sensor->get_info(RS2_CAMERA_INFO_NAME) : "";

        // Disable asynchronous services
        for (auto& opt : sensor->get_supported_options())
        {
            if (val_in_range(opt, { RS2_OPTION_GLOBAL_TIME_ENABLED, RS2_OPTION_ERROR_POLLING_ENABLED }))
            {
                try
                {
                    // enumerated options use zero or positive values
                    if (sensor->get_option(opt).query() > 0.f)
                        sensor->get_option(opt).set(false);
                }
                catch (...)
                {
                    LOG_ERROR("Failed to toggle off " << opt << " [" << snr_name << "]");
                }
            }
        }

        // Stop UVC/HID streaming
        try
        {
            if (sensor->is_streaming())
            {
                sensor->stop();
                sensor->close();
            }
        }
        catch (const wrong_api_call_sequence_exception& exc)
        {
            LOG_WARNING("Out of order stop/close invocation for " << snr_name << ": " << exc.what());
        }
        catch (...)
        {
            auto snr_name = (sensor->supports_info(RS2_CAMERA_INFO_NAME)) ? sensor->get_info(RS2_CAMERA_INFO_NAME) : "";
            LOG_ERROR("Failed to deactivate " << snr_name);
        }
    }
}
