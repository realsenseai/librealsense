// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#include "bag_to_db3_converter.h"
#include "reader_factory.h"
#include "ros2/ros2_writer.h"


namespace librealsense
{
    using namespace device_serializer;

    void convert_bag_to_db3(const std::string& input_bag, const std::string& output_db3, std::shared_ptr<context> ctx)
    {
        LOG_INFO("Converting " << input_bag << " to " << output_db3 << ".db3");

        auto reader = create_reader_for_file(input_bag, ctx);
        std::shared_ptr<writer> writer = std::make_shared<ros2_writer>(output_db3, false);

        // 1. Write device description (info, options, stream profiles, processing blocks)
        auto device_desc = reader->query_device_description(nanoseconds(0));
        writer->write_device_description(device_desc);

        // 2. Write extrinsics and collect stream identifiers
        std::vector<stream_identifier> all_streams;
        for (auto&& sensor_snap : device_desc.get_sensors_snapshots())
        {
            for (auto&& profile : sensor_snap.get_stream_profiles())
            {
                all_streams.push_back({ 0, sensor_snap.get_sensor_index(),
                    profile->get_stream_type(), static_cast<uint32_t>(profile->get_stream_index()) });
            }
        }

        for (auto&& extrinsic_entry : device_desc.get_extrinsics_map())
        {
            auto& stream_id = extrinsic_entry.first;
            auto& reference_id = extrinsic_entry.second.first;
            auto& ext = extrinsic_entry.second.second;
            writer->write_extrinsics(stream_id, reference_id, ext);
        }

        // 3. Enable all streams so the reader delivers frames
        reader->enable_stream(all_streams);

        // 4. Read all data and write frames/notifications
        uint64_t frame_count = 0;
        while (true)
        {
            auto data = reader->read_next_data();

            if (data->is<serialized_end_of_file>())
                break;

            if (auto frame = data->as<serialized_frame>())
            {
                writer->write_frame(frame->stream_id, frame->get_timestamp(), std::move(frame->frame));
                ++frame_count;
            }
            else if (auto notif = data->as<serialized_notification>())
            {
                writer->write_notification(notif->sensor_id, notif->get_timestamp(), notif->notif);
            }
            // serialized_option: individual option changes are already captured in device_description
        }

        LOG_INFO("Conversion complete: " << frame_count << " frames written to " << writer->get_file_name());
    }
}
