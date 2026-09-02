// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#pragma once
#include "uvc-types.h"
#include "uvc-device.h"

#include "stdio.h"
#include "stdlib.h"
#include <cstring>
#include <string>
#include <chrono>
#include <thread>

typedef void(uvc_frame_callback_t)(struct librealsense::platform::frame_object *frame, void *user_ptr);

namespace librealsense
{
    namespace platform
    {
        struct uvc_streamer_context
        {
            stream_profile profile;
            frame_callback user_cb;
            std::shared_ptr<uvc_stream_ctrl_t> control;
            rs_usb_device usb_device;
            rs_usb_messenger messenger;
            uint8_t request_count;
        };

        class uvc_streamer
        {
        public:
            uvc_streamer(uvc_streamer_context context);
            virtual ~uvc_streamer();

            const uvc_streamer_context get_context() { return _context; }

            bool running() { return _running; }
            void start();
            void stop();
            void enable_user_callbacks() { _publish_frames = true; }
            void disable_user_callbacks() { _publish_frames = false; }
            bool wait_for_first_frame(uint32_t timeout_ms);

        private:
            std::mutex _running_mutex;
            std::condition_variable _stopped_cv;
            bool _running = false;
            bool _frame_arrived = false;
            bool _publish_frames = true;

            int64_t _watchdog_timeout;
            uvc_streamer_context _context;

            dispatcher _action_dispatcher;

            std::shared_ptr<watchdog> _watchdog;
            uint32_t _read_buff_length;
            bool _allow_variable_length_payload = false;
            backend_frames_queue _queue;
            rs_usb_endpoint _read_endpoint;
            std::vector<rs_usb_request> _requests;
            std::shared_ptr<backend_frames_archive> _frames_archive;
            std::shared_ptr<active_object<>> _publish_frame_thread;
            std::shared_ptr<platform::usb_request_callback> _request_callback;

            void init();
            void flush();
        };
    }
}
