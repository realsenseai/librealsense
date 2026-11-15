// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "v4l-kernel-buffers.h"

#include "backend.h"  // MAX_META_DATA_SIZE
#include "types.h"
#include <src/librealsense-exception.h>

#ifdef RS2_USE_CUDA_ZEROCOPY
#include "../cuda/cuda-frame-memory.h"  // register V4L2 buffers with CUDA for zero-copy GPU access
#endif

#include <cstdlib>
#include <cstring>

#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

namespace librealsense
{
    namespace platform
    {
        buffer::buffer(int fd, v4l2_buf_type type, bool use_memory_map, uint32_t index)
            : _type(type), _use_memory_map(use_memory_map), _index(index)
        {
            v4l2_buffer buf = {};
            struct v4l2_plane planes[VIDEO_MAX_PLANES] = {};
            buf.type = _type;
            buf.memory = use_memory_map ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;
            buf.index = index;
            buf.m.offset = 0;
            if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
                buf.m.planes = planes;
                buf.length = VIDEO_MAX_PLANES;
            }
            if(xioctl(fd, VIDIOC_QUERYBUF, &buf) < 0)
                throw linux_backend_exception("xioctl(VIDIOC_QUERYBUF) failed");

            // Prior to kernel 4.16 metadata payload was attached to the end of the video payload
            uint8_t md_extra = (V4L2_BUF_TYPE_VIDEO_CAPTURE==type) ? MAX_META_DATA_SIZE : 0;
            _original_length = buf.length;
            _offset = buf.m.offset;
            if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
                _original_length = buf.m.planes[0].length;
                _offset = buf.m.planes[0].m.mem_offset;
                md_extra = 0;
            }
            _length = _original_length + md_extra;

            if (use_memory_map)
            {
                _start = static_cast<uint8_t*>(mmap(nullptr, _original_length,
                                                    PROT_READ | PROT_WRITE, MAP_SHARED,
                                                    fd, _offset));
                if(_start == MAP_FAILED)
                    throw linux_backend_exception("mmap failed");
#ifdef RS2_USE_CUDA_ZEROCOPY
                // Pin+map this V4L2 buffer into CUDA's address space once, so a captured frame that
                // points directly at it can be read by GPU kernels with no host-to-device copy. No-op
                // off integrated GPUs; on failure the GPU path simply falls back to copying.
                if( rs_v4l2_zc_register( _start, _original_length ) )
                    _zc_registered = true;
#endif
            }
            else
            {
                //_length += (V4L2_BUF_TYPE_VIDEO_CAPTURE==type) ? MAX_META_DATA_SIZE : 0;
#ifdef RS2_USE_CUDA_ZEROCOPY
                // USERPTR: GPU-visible buffer so a zero-copy frame aliasing it stays GPU-resident (as MMAP/RSUSB do).
                _start = static_cast<uint8_t*>(rs_frame_zc_alloc( _length ));
                if (!_start) throw linux_backend_exception("rs_frame_zc_alloc for USERPTR buffer failed!");
#else
                _start = static_cast<uint8_t*>(malloc( _length));
                if (!_start) throw linux_backend_exception("User_p allocation failed!");
#endif
                memset(_start, 0, _length);
            }
        }

        void buffer::prepare_for_streaming(int fd)
        {
            v4l2_buffer buf = {};
            struct v4l2_plane planes[VIDEO_MAX_PLANES] = {};
            buf.type = _type;
            buf.memory = _use_memory_map ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;
            buf.index = _index;
            buf.length = _length;
            if (_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
                buf.m.planes = planes;
                buf.length = 1;
            }
            if ( !_use_memory_map )
            {
                buf.m.userptr = reinterpret_cast<unsigned long>(_start);
            }
            if(xioctl(fd, VIDIOC_QBUF, &buf) < 0)
                throw linux_backend_exception("xioctl(VIDIOC_QBUF) failed");
            else
                LOG_DEBUG_V4L("prepare_for_streaming fd " << std::dec << fd);
        }

        buffer::~buffer()
        {
            if (_use_memory_map)
            {
#ifdef RS2_USE_CUDA_ZEROCOPY
               if( _zc_registered )
                   rs_v4l2_zc_unregister( _start );
#endif
               if(munmap(_start, _original_length) < 0)
                   LOG_DEBUG_V4L("munmap failed on buffer Dtor");
            }
            else
            {
#ifdef RS2_USE_CUDA_ZEROCOPY
               rs_frame_zc_free( _start );
#else
               free(_start);
#endif
            }
        }

        void buffer::attach_buffer(const v4l2_buffer& buf)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _buf = buf;
            _must_enqueue = true;
        }

        void buffer::detach_buffer()
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _must_enqueue = false;
        }

        void buffer::request_next_frame(int fd, bool force)
        {
            std::lock_guard<std::mutex> lock(_mutex);

            if (_must_enqueue || force)
            {
                if (!_use_memory_map)
                {
                    auto metadata_offset = get_full_length() - MAX_META_DATA_SIZE;
                    memset((uint8_t *)(get_frame_start()) + metadata_offset, 0, MAX_META_DATA_SIZE);
                }

                LOG_DEBUG_V4L("Enqueue buf " << std::dec << _buf.index << " for fd " << fd);
                if (xioctl(fd, VIDIOC_QBUF, &_buf) < 0)
                {
                    LOG_ERROR("xioctl(VIDIOC_QBUF) failed when requesting new frame! fd: " << fd << " error: " << strerror(errno));
                }

                _must_enqueue = false;
            }
        }

        void buffers_mgr::handle_buffer(supported_kernel_buf_types  buf_type,
                                           int                      file_desc,
                                           v4l2_buffer              v4l_buf,
                                           std::shared_ptr<platform::buffer> data_buf
                                           )
        {
            if (e_max_kernel_buf_type <=buf_type)
                throw linux_backend_exception("invalid kernel buffer type request");

            // D457 development - changed from 1 to 0
            if (file_desc < 0)
            {
                // QBUF to be performed by a 3rd party
                this->buffers.at(buf_type)._managed  = true;
            }
            else
            {
                buffers.at(buf_type)._file_desc = file_desc;
                buffers.at(buf_type)._managed = false;
                buffers.at(buf_type)._data_buf = data_buf;
                buffers.at(buf_type)._dq_buf = v4l_buf;
            }
        }

        void buffers_mgr::request_next_frame()
        {
            for (auto& buf : buffers)
            {
                LOG_DEBUG_V4L("request_next_frame with fd = " << buf._file_desc);
                if (buf._data_buf && (buf._file_desc >= 0))
                    buf._data_buf->request_next_frame(buf._file_desc);
            };
            _md_start = nullptr;
            _md_size = 0;
        }

        void buffers_mgr::set_md_from_video_node(bool compressed)
        {
            void* md_start = nullptr;
            auto md_size = 0;

            if (buffers.at(e_video_buf)._file_desc >=0)
            {
                static const int d4xx_md_size = 248;
                auto buffer = buffers.at(e_video_buf)._data_buf;
                auto dq  = buffers.at(e_video_buf)._dq_buf;
                auto fr_payload_size = buffer->get_length_frame_only();

                // For compressed data assume D4XX metadata struct
                // TODO - devise SKU-agnostic heuristics
                auto md_appendix_sz = 0L;
                if (compressed && (dq.bytesused < fr_payload_size))
                    md_appendix_sz = d4xx_md_size;
                else
                    md_appendix_sz = long(dq.bytesused) - fr_payload_size;

                if (md_appendix_sz >0 )
                {
                    md_start = buffer->get_frame_start() + dq.bytesused - md_appendix_sz;
                    md_size = (*(static_cast<uint8_t*>(md_start)));
                    int md_flags = (*(static_cast<uint8_t*>(md_start)+1));
                    // Use heuristics for metadata validation
                    if ((md_appendix_sz != md_size) || (!val_in_range(md_flags, {0x8e, 0x8f})))
                    {
                        md_size = 0;
                        md_start=nullptr;
                    }
                }
            }

            if (nullptr == md_start)
            {
                LOG_DEBUG("Could not parse metadata");
            }
            set_md_attributes(static_cast<uint8_t>(md_size),md_start);
        }

        bool buffers_mgr::verify_vd_md_sync() const
        {
            if ((buffers[e_video_buf]._file_desc > 0) && (buffers[e_metadata_buf]._file_desc > 0))
            {
                if (buffers[e_video_buf]._dq_buf.sequence != buffers[e_metadata_buf]._dq_buf.sequence)
                {
                    LOG_ERROR("Non-sequential Video and Metadata v4l buffers - video seq = " << buffers[e_video_buf]._dq_buf.sequence << ", md seq = " << buffers[e_metadata_buf]._dq_buf.sequence);
                    return false;
                }
            }
            return true;
        }

        bool  buffers_mgr::md_node_present() const
        {
            return (buffers[e_metadata_buf]._file_desc > 0);
        }
    }  // namespace platform
}  // namespace librealsense
