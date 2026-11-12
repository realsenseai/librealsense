// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#pragma once

#include <map>
#include <utility>

#include "resolver.h"

namespace librealsense
{
    namespace pipeline
    {
        class profile;
        class pipeline;

        class config
        {
        public:
            config();
            void enable_stream(rs2_stream stream, int index, uint32_t width, uint32_t height, rs2_format format, uint32_t framerate);
            void enable_all_stream();
            void enable_device(const std::string& serial);
            void enable_device_from_file(const std::string& file, bool repeat_playback);
            void enable_record_to_file(const std::string& file);
            void disable_stream(rs2_stream stream, int index = -1);
            void disable_all_streams();
            std::shared_ptr<profile> resolve(std::shared_ptr<pipeline> pipe, const std::chrono::milliseconds& timeout = std::chrono::milliseconds(0));
            bool can_resolve(std::shared_ptr<pipeline> pipe);
            bool get_repeat_playback();
            void enable_intent(bool enabled) { std::lock_guard<std::mutex> lock(_mtx); _enable_intent = enabled; }
            bool intent_enabled() const { std::lock_guard<std::mutex> lock(_mtx); return _enable_intent; }

            //Non top level API
            std::shared_ptr<profile> get_cached_resolved_profile();

            config(const config& other)
            {
                std::lock_guard< std::mutex > lock( other._mtx ); // So other configuration won't change while reading (streams enabled/disabled)
                _device_request = other._device_request;
                _stream_requests = other._stream_requests;
                _enable_all_streams = other._enable_all_streams;
                _resolved_profile = nullptr;
                _playback_loop = other._playback_loop;
                _enable_intent = other._enable_intent;
            }
        private:
            struct device_request
            {
                std::string serial;
                std::string filename;
                std::string record_output;
            };
            std::shared_ptr<device_interface> get_or_add_playback_device(std::shared_ptr<context> ctx, const std::string& file);
            std::shared_ptr<device_interface> resolve_device_requests(std::shared_ptr<pipeline> pipe, const std::chrono::milliseconds& timeout);
            stream_profiles get_default_configuration(std::shared_ptr<device_interface> dev);
            std::shared_ptr<profile> resolve(std::shared_ptr<device_interface> dev);
            util::config filter_stream_requests(const stream_profiles& profiles) const;

            device_request _device_request;
            std::map<std::pair<rs2_stream, int>, stream_profile> _stream_requests;
            mutable std::mutex _mtx;
            bool _enable_all_streams = false;
            bool _enable_intent = false;
            std::shared_ptr<profile> _resolved_profile;
            bool _playback_loop = false;
            std::vector<std::pair<rs2_stream, int>> _streams_to_disable;
        };
    }
}
