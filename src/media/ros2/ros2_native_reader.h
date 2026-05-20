// License: Apache 2.0 See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once
#include "ros2_reader_base.h"


namespace librealsense
{
    using namespace device_serializer;

    class info_container;
    class options_container;

    // Reader for `.db3` files produced by `ros2 bag record`. For librealsense's own
    // ros2_writer output, see `ros2_reader`. The factory in `ros_factory.cpp` dispatches.
    class ros2_native_reader : public ros2_reader_base
    {
    public:
        ros2_native_reader(const std::string& file, const std::shared_ptr<context> ctx);

        std::shared_ptr<serialized_data> read_next_data() override;
        void reset() override;

    private:
        device_snapshot read_device_description(const nanoseconds& time, bool reset = false) override;
        device_snapshot read_native_device_description();
        std::shared_ptr<serialized_frame> create_frame(const std::shared_ptr<rosbag2_storage::SerializedBagMessage>& msg);
        void setup_frame(frame_interface* frame_ptr, const stream_identifier& sid) const override;
        std::vector<std::string> get_stream_topics() const override;
        std::vector<std::string> get_option_topics() const override;
        std::vector<std::string> get_notification_topics() const override;

        static rs2_stream native_stream_type_from_topic(const std::string& topic);
        static rs2_format native_format_from_image_encoding(const std::string& encoding, rs2_stream stream_type);

        std::map<std::string, stream_identifier>                              _topic_to_stream_id;
        std::map<stream_identifier, std::shared_ptr<stream_profile_interface>> _profile_by_stream;
        std::map<stream_identifier, uint64_t>                                  _native_frame_counters;
    };
}
