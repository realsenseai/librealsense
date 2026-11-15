// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#pragma once
#include <atomic>
#include "../../core/roi.h"
#include "../../core/extension.h"
#include "../../core/serialization.h"
#include <src/core/device-interface.h>
#include "../../archive.h"
#include "../../sensor.h"
#include "playback_sensor.h"

#include <rsutils/lazy.h>
#include <rsutils/signal.h>


namespace librealsense
{
    class playback_device : public device_interface,
        public extendable_interface,
        public info_container
    {
    public:
        playback_device( std::shared_ptr< const device_info > const &,
                         std::shared_ptr< device_serializer::reader > const & serializer );
        virtual ~playback_device();

        std::shared_ptr<context> get_context() const override;

        sensor_interface& get_sensor(size_t i) override;
        size_t get_sensors_count() const override;
        const sensor_interface& get_sensor(size_t i) const override;
        void hardware_reset() override;
        bool extend_to(rs2_extension extension_type, void** ext) override;
        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override;

        void set_frame_rate(double rate);
        void seek_to_time(std::chrono::nanoseconds time);
        rs2_playback_status get_current_status() const;
        uint64_t get_duration() const;
        void pause();
        void resume();
        void stop();
        void set_real_time(bool real_time);
        bool is_real_time() const;
        const std::string& get_file_name() const;
        uint64_t get_position() const;
        rsutils::public_signal< playback_device, rs2_playback_status > playback_status_changed;
        std::shared_ptr< const device_info > get_device_info() const override;
        std::pair<uint32_t, rs2_extrinsics> get_extrinsics(const stream_interface& stream) const override;
        static bool try_extend_snapshot(std::shared_ptr<extension_snapshot>& e, rs2_extension extension_type, void** ext);
        bool is_valid() const override;
        bool is_in_recovery_mode() const override;

        std::vector<tagged_profile> get_profiles_tags() const override { return std::vector<tagged_profile>(); };//no hard-coded default streams for playback
        void tag_profiles(stream_profiles profiles) const override
        {
            for(auto profile : profiles)
                profile->tag_profile(profile_tag::PROFILE_TAG_DEFAULT | profile_tag::PROFILE_TAG_SUPERSET);
        }

        bool compress_while_record() const override { return true; }
        bool contradicts(const stream_profile_interface* a, const std::vector<stream_profile>& others) const override { return false; }

    private:
        void update_time_base(device_serializer::nanoseconds base_timestamp);
        device_serializer::nanoseconds calc_sleep_time(device_serializer::nanoseconds  timestamp);
        void start();
        void stop_internal();
        void try_looping();
        template <typename T> void do_loop(T op);
        std::map<uint32_t, std::shared_ptr<playback_sensor>> create_playback_sensors(const device_serializer::device_snapshot& device_description);
        std::shared_ptr<stream_profile_interface> get_stream(const std::map<unsigned, std::shared_ptr<playback_sensor>>& sensors_map, device_serializer::stream_identifier stream_id);
        rs2_extrinsics calc_extrinsic(const rs2_extrinsics& from, const rs2_extrinsics& to);
        void catch_up();
        void register_device_info(const device_serializer::device_snapshot& device_description);
        void register_extrinsics(const device_serializer::device_snapshot& device_description);
        void update_extensions(const device_serializer::device_snapshot& device_description);
        bool prefetch_done();

    private:
        rsutils::lazy< std::shared_ptr< dispatcher > > m_read_thread;
        std::shared_ptr< const device_info > m_device_info;
        std::shared_ptr<device_serializer::reader> m_reader;
        device_serializer::device_snapshot m_device_description;
        std::atomic_bool m_is_started;
        std::atomic_bool m_is_paused;
        std::chrono::high_resolution_clock::time_point m_base_sys_time; // !< System time when reading began (first frame was read)
        device_serializer::nanoseconds m_base_timestamp; // !< Timestamp of the first frame that has a real timestamp (different than 0)
        std::map<uint32_t, std::shared_ptr<playback_sensor>> m_sensors;
        std::map<uint32_t, std::shared_ptr<playback_sensor>> m_active_sensors;
        std::atomic<double> m_sample_rate;
        std::atomic_bool m_real_time;
        std::atomic< device_serializer::nanoseconds > m_prev_timestamp;
        std::vector< std::shared_ptr< rsutils::lazy< rs2_extrinsics > > > m_extrinsics_fetchers;
        std::map<int, std::pair<uint32_t, rs2_extrinsics>> m_extrinsics_map;
        device_serializer::nanoseconds m_last_published_timestamp;
        std::mutex m_last_published_timestamp_mutex;
        std::mutex _active_sensors_mutex;
    };

    MAP_EXTENSION(RS2_EXTENSION_PLAYBACK, playback_device);
}
