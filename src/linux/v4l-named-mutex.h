// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <atomic>
#include <cstdint>
#include <string>

namespace librealsense
{
    namespace platform
    {
        class named_mutex
        {
        public:
            named_mutex(const std::string& device_path, unsigned timeout);

            named_mutex(const named_mutex&) = delete;

            ~named_mutex();

            void lock();

            void unlock();

            bool try_lock();

        private:
            void ensure_fd_open();
            void close_fd();

            std::string _device_path;
            uint32_t _timeout;
            int _fildes;
            std::atomic< int > _lock_counter;
        };
    }  // namespace platform
}  // namespace librealsense
