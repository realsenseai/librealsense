// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "v4l-ioctl.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <queue>

namespace librealsense
{
    namespace platform
    {
        class v4l2_video_md_syncer
        {
        public:
            v4l2_video_md_syncer() : _is_ready(false){}

            struct sync_buffer
            {
                std::shared_ptr<v4l2_buffer> _v4l2_buf;
                int _fd;
                __u32 _buffer_index;
            };

            // pushing video buffer to the video queue
            void push_video(const sync_buffer& video_buffer);
            // pushing metadata buffer to the metadata queue
            void push_metadata(const sync_buffer& md_buffer);

            // pulling synced data
            // if returned value is true - the data could have been pulled
            // if returned value is false - no data is returned via the inout params because data could not be synced
            bool pull_video_with_metadata(std::shared_ptr<v4l2_buffer>& video_buffer, std::shared_ptr<v4l2_buffer>& md_buffer, int& video_fd, int& md_fd);

            inline void start() {_is_ready = true;}
            void stop();

            // true if a buffer discarded via QBUF revealed the device was physically removed (ENODEV).
            // Consumed (and reset) once by the owning device's poll(), so a real disconnect is reported exactly once.
            inline bool consume_device_disconnected() { return _qbuf_device_disconnected.exchange(false); }

        private:
            void enqueue_buffer_before_throwing_it(const sync_buffer& sb);
            void enqueue_front_buffer_before_throwing_it(std::queue<sync_buffer>& sync_queue);
            void flush_queues();
            // Call immediately after a failed QBUF, before anything else touches errno.
            void report_qbuf_failure(int fd);

            std::mutex _syncer_mutex;
            std::queue<sync_buffer> _video_queue;
            std::queue<sync_buffer> _md_queue;
            bool _is_ready;
            std::atomic<bool> _qbuf_device_disconnected{ false };
        };
    }  // namespace platform
}  // namespace librealsense
