// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "v4l-ioctl.h"

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>

namespace librealsense
{
    namespace platform
    {
        class buffer
        {
        public:
            buffer(int fd, v4l2_buf_type type, bool use_memory_map, uint32_t index);

            void prepare_for_streaming(int fd);

            ~buffer();

            void attach_buffer(const v4l2_buffer& buf);

            void detach_buffer();

            void request_next_frame(int fd, bool force=false);

            uint32_t get_full_length() const { return _length; }
            uint32_t get_length_frame_only() const { return _original_length; }

            uint8_t* get_frame_start() const { return _start; }

            bool use_memory_map() const { return _use_memory_map; }

        private:
            v4l2_buf_type _type;
            uint8_t* _start;
            uint32_t _length;
            uint32_t _original_length;
            uint32_t _offset;
            bool _use_memory_map;
            uint32_t _index;
            v4l2_buffer _buf;
            std::mutex _mutex;
            bool _must_enqueue = false;
            bool _zc_registered = false;  // this mmap buffer is registered with CUDA for zero-copy GPU access
        };

        enum supported_kernel_buf_types : uint8_t
        {
            e_video_buf,
            e_metadata_buf,
            e_max_kernel_buf_type
        };


        // RAII for buffer exchange with kernel
        struct kernel_buf_guard
        {
            ~kernel_buf_guard()
            {
                if (_data_buf && (!_managed))
                {
                    if (_file_desc > 0)
                    {
                        if (xioctl(_file_desc, (int)VIDIOC_QBUF, &_dq_buf) < 0)
                        {
                            LOG_DEBUG_V4L("xioctl(VIDIOC_QBUF) guard failed for fd " << std::dec << _file_desc);
                            if (xioctl(_file_desc, (int)VIDIOC_DQBUF, &_dq_buf) >= 0)
                            {
                                LOG_DEBUG_V4L("xioctl(VIDIOC_QBUF) Re-enqueue succeeded for fd " << std::dec << _file_desc);
                                if (xioctl(_file_desc, (int)VIDIOC_QBUF, &_dq_buf) < 0)
                                    LOG_DEBUG_V4L("xioctl(VIDIOC_QBUF) re-deque  failed for fd " << std::dec << _file_desc);
                                else
                                    LOG_DEBUG_V4L("xioctl(VIDIOC_QBUF) re-deque succeeded for fd " << std::dec << _file_desc);
                            }
                            else
                                LOG_DEBUG_V4L("xioctl(VIDIOC_QBUF) Re-enqueue failed for fd " << std::dec << _file_desc);
                        }
                        else
                            LOG_DEBUG_V4L("Enqueue (e) buf " << std::dec << _dq_buf.index << " for fd " << _file_desc);
                    }
                }
            }

            std::shared_ptr<platform::buffer>   _data_buf=nullptr;
            v4l2_buffer                         _dq_buf{};
            int                                 _file_desc=-1;
            bool                                _managed=false;
        };

        // RAII handling of kernel buffers interchanges
        class buffers_mgr
        {
        public:
            buffers_mgr(bool memory_mapped_buf) :
                _md_start(nullptr),
                _md_size(0),
                _mmap_bufs(memory_mapped_buf)
                {}

            ~buffers_mgr(){}

            void    request_next_frame();
            void    handle_buffer(supported_kernel_buf_types buf_type, int file_desc,
                                   v4l2_buffer buf= v4l2_buffer(),
                                   std::shared_ptr<platform::buffer> data_buf=nullptr);

            uint8_t metadata_size() const { return _md_size; }
            void*   metadata_start() const { return _md_start; }

            void    set_md_attributes(uint8_t md_size, void* md_start)
                    { _md_start = md_start; _md_size = md_size; }
            void    set_md_from_video_node(bool compressed);
            bool    verify_vd_md_sync() const;
            bool    md_node_present() const;

            std::array<kernel_buf_guard, e_max_kernel_buf_type>& get_buffers()
                    { return buffers; }

        private:
            void*                               _md_start;  // marks the address of metadata blob
            uint8_t                             _md_size;   // metadata size is bounded by 255 bytes by design
            bool                                _mmap_bufs;


            std::array<kernel_buf_guard, e_max_kernel_buf_type> buffers;
        };
    }  // namespace platform
}  // namespace librealsense
