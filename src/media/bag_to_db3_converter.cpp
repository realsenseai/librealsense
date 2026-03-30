// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "bag_to_db3_converter.h"
#include "core/serialization.h"
#include <stdexcept>
#include <rsutils/easylogging/easyloggingpp.h>

#ifdef BUILD_ROSBAG2

#include "ros_factory.h"
#include "ros_common.h"
#include "ros2/ros2_writer.h"
#include <rsutils/string/from.h>

namespace librealsense
{
    using namespace device_serializer;

    static std::vector<stream_identifier> collect_stream_identifiers(const device_snapshot& device_desc)
    {
        std::vector<stream_identifier> all_streams;
        for (auto&& sensor_snap : device_desc.get_sensors_snapshots())
        {
            for (auto&& profile : sensor_snap.get_stream_profiles())
            {
                all_streams.push_back({ get_device_index(), sensor_snap.get_sensor_index(),
                    profile->get_stream_type(), static_cast<uint32_t>(profile->get_stream_index()) });
            }
        }
        return all_streams;
    }

    static uint64_t write_frames(std::shared_ptr<reader> reader, std::shared_ptr<writer> writer, std::function<void(float)> progress_callback)
    {
        uint64_t frame_count = 0;
        auto duration_ns = reader->query_duration().count();
        while (true)
        {
            auto data = reader->read_next_data();

            if (data->is<serialized_end_of_file>())
                break;

            if (auto frame = data->as<serialized_frame>())
            {
                if (progress_callback && duration_ns > 0)
                {
                    auto ts = frame->get_timestamp().count();
                    progress_callback(std::min(1.0f, static_cast<float>(ts) / duration_ns));
                }
                writer->write_frame(frame->stream_id, frame->get_timestamp(), std::move(frame->frame));
                ++frame_count;
            }
            else if (auto notif = data->as<serialized_notification>())
            {
                writer->write_notification(notif->sensor_id, notif->get_timestamp(), notif->notif);
            }
        }
        if (progress_callback)
            progress_callback(1.0f);
        return frame_count;
    }

    void convert_bag_to_db3(const std::string& input_bag, const std::string& output_db3, std::shared_ptr<context> ctx, std::function<void(float)> progress_callback)
    {
        if (is_db3_file(input_bag))
            throw invalid_value_exception(rsutils::string::from() << "Input file '" << input_bag << "' is already a .db3 file");

        LOG_INFO("Converting " << input_bag << " to " << output_db3);

        auto reader = create_reader_for_file(input_bag, ctx);
        std::shared_ptr<writer> writer = std::make_shared<ros2_writer>(output_db3, false);

        auto device_desc = reader->query_device_description(nanoseconds(0));
        writer->write_device_description(device_desc);

        auto all_streams = collect_stream_identifiers(device_desc);
        reader->enable_stream(all_streams);

        for (auto&& extrinsic_entry : device_desc.get_extrinsics_map())
        {
            auto& stream_id = extrinsic_entry.first;
            auto& reference_id = extrinsic_entry.second.first;
            auto& ext = extrinsic_entry.second.second;
            writer->write_extrinsics(stream_id, reference_id, ext);
        }
        auto frame_count = write_frames(reader, writer, progress_callback);

        LOG_INFO("Conversion complete: " << frame_count << " frames written to " << writer->get_file_name());
    }
}

#else // !BUILD_ROSBAG2

namespace librealsense
{
    void convert_bag_to_db3(const std::string&, const std::string&, std::shared_ptr<context>, std::function<void(float)>)
    {
        LOG_WARNING("bag-to-db3 conversion not available (BUILD_ROSBAG2 is off)");
        throw std::runtime_error("bag-to-db3 conversion requires BUILD_ROSBAG2");
    }
}

#endif
