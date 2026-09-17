// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "v4l-uvc-meta-device.h"

#include <src/librealsense-exception.h>
#include <rsutils/string/from.h>

#include <algorithm>
#include <cstring>

#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace librealsense
{
    namespace platform
    {
        v4l_uvc_meta_device::v4l_uvc_meta_device(const uvc_device_info& info, bool use_memory_map):
            v4l_uvc_device(info,use_memory_map),
            _md_fd(0),
            _md_name(info.metadata_node_id),
            _md_capabilities_assigned(false)
        {
        }

        v4l_uvc_meta_device::~v4l_uvc_meta_device()
        {
        }

        void v4l_uvc_meta_device::streamon() const
        {
            bool jetson_platform = is_platform_jetson();
            if ((_md_fd != -1) && jetson_platform)
            {
                // D457 development - added for mipi device, for IR because no metadata there
                // Metadata stream shall be configured first to allow sync with video node
                stream_ctl_on(_md_fd, _md_type);
            }

            // Invoke UVC streaming request
            v4l_uvc_device::streamon();

            // Metadata stream configured last for IPU6 and it will be in sync with video node
            if ((_md_fd != -1) && !jetson_platform)
            {
                stream_ctl_on(_md_fd, _md_type);
            }

        }

        void v4l_uvc_meta_device::streamoff() const
        {
            bool jetson_platform = is_platform_jetson();
            // IPU6 platform should stop md, then video
            if (jetson_platform)
                v4l_uvc_device::streamoff();

            if (_md_fd != -1)
            {
                // D457 development - added for mipi device, for IR because no metadata there
                stream_off(_md_fd, _md_type);
            }
            if (!jetson_platform)
                v4l_uvc_device::streamoff();
        }

        void v4l_uvc_meta_device::negotiate_kernel_buffers(size_t num) const
        {
            v4l_uvc_device::negotiate_kernel_buffers(num);

            if (_md_fd == -1)
            {
                // D457 development - added for mipi device, for IR because no metadata there
                return;
            }
            req_io_buff(_md_fd, num, _name,
                        _use_memory_map ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR,
                        _md_type);
        }

        void v4l_uvc_meta_device::allocate_io_buffers(size_t buffers)
        {
            v4l_uvc_device::allocate_io_buffers(buffers);

            if (buffers)
            {
                for(size_t i = 0; i < buffers; ++i)
                {
                    // D457 development - added for mipi device, for IR because no metadata there
                    if (_md_fd == -1)
                        continue;
                    _md_buffers.push_back(std::make_shared<buffer>(_md_fd, _md_type, _use_memory_map, i));
                }
            }
            else
            {
                for(size_t i = 0; i < _md_buffers.size(); i++)
                {
                    _md_buffers[i]->detach_buffer();
                }
                _md_buffers.resize(0);
            }
        }

        void v4l_uvc_meta_device::map_device_descriptor()
        {
            v4l_uvc_device::map_device_descriptor();

            if (_md_fd>0)
                throw linux_backend_exception(rsutils::string::from() << _md_name << " descriptor is already opened");

            _md_fd = open_v4l_node(_md_name);
            if(_md_fd < 0)
            {
                return;  // Does not throw, MIPI device metadata not received through UVC, no metadata here may be valid
            }

            _fds.push_back(_md_fd);
            _max_fd = *std::max_element(_fds.begin(),_fds.end());

            if (!_md_capabilities_assigned)
            {
                assign_md_device_capabilities();
                _md_capabilities_assigned = true;
            }

        }

        void v4l_uvc_meta_device::assign_md_device_capabilities()
        {
            v4l2_capability cap = {};
            if(xioctl(_md_fd, VIDIOC_QUERYCAP, &cap) < 0)
            {
                if(errno == EINVAL)
                    throw linux_backend_exception(_md_name + " is no V4L2 device");
                else
                    throw linux_backend_exception(_md_name +  " xioctl(VIDIOC_QUERYCAP) for metadata failed");
            }

            if(!(cap.capabilities & V4L2_CAP_META_CAPTURE))
                throw linux_backend_exception(_md_name + " is not metadata capture device");

            if(!(cap.capabilities & V4L2_CAP_STREAMING))
                throw linux_backend_exception(_md_name + " does not support metadata streaming I/O");

            if(cap.capabilities & V4L2_CAP_META_CAPTURE)
                _md_type = LOCAL_V4L2_BUF_TYPE_META_CAPTURE;
        }


        void v4l_uvc_meta_device::unmap_device_descriptor()
        {
            v4l_uvc_device::unmap_device_descriptor();

            if(::close(_md_fd) < 0)
            {
                return;  // Does not throw, MIPI device metadata not received through UVC, no metadata here may be valid
            }

            _md_fd = 0;
        }

        void v4l_uvc_meta_device::set_format(stream_profile profile)
        {
            // Select video node streaming format
            v4l_uvc_device::set_format(profile);

            // Configure metadata node stream format
            v4l2_format fmt{ };
            fmt.type = _md_type;

            if (xioctl(_md_fd, VIDIOC_G_FMT, &fmt))
            {
                return;  // Does not throw, MIPI device metadata not received through UVC, no metadata here may be valid
            }

            if (fmt.type != _md_type)
                throw linux_backend_exception("ioctl(VIDIOC_G_FMT): " + _md_name + " node is not metadata capture");

            bool success = false;

            for (const uint32_t& request : { V4L2_META_FMT_D4XX, V4L2_META_FMT_UVC})
            {
                // Configure metadata format - try d4xx, then fallback to currently retrieve UVC default header of 12 bytes
                memcpy(fmt.fmt.raw_data, &request, sizeof(request));
                // use only for IPU6?
                if ((_dev.cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) && !is_platform_jetson())
                {
                    /* Sakari patch for videodev2.h. This structure will be within kernel > 6.4 */
                    struct v4l2_meta_format {
                        uint32_t    dataformat;
                        uint32_t    buffersize;
                        uint32_t    width;
                        uint32_t    height;
                        uint32_t    bytesperline;
                    } meta;
                    // copy fmt from g_fmt ioctl
                    memcpy(&meta, fmt.fmt.raw_data, sizeof(meta));
                    // set fmt width, d4xx metadata is only one line
                    meta.dataformat = request;
                    meta.width      = profile.width;
                    meta.height     = 1;
                    memcpy(fmt.fmt.raw_data, &meta, sizeof(meta));
                }

                if(xioctl(_md_fd, VIDIOC_S_FMT, &fmt) >= 0)
                {
                    LOG_INFO("Metadata node was successfully configured to " << fourcc_to_string(request) << " format" <<", fd " << std::dec <<_md_fd);
                    success  =true;
                    break;
                }
                else
                {
                    LOG_WARNING("Metadata node configuration failed for " << fourcc_to_string(request));
                }
            }

            if (!success)
                throw linux_backend_exception(_md_name + " ioctl(VIDIOC_S_FMT) for metadata node failed");

        }

        void v4l_uvc_meta_device::prepare_capture_buffers()
        {
            if (_md_fd != -1)
            {
                // D457 development - added for mipi device, for IR because no metadata there
                // Meta node to be initialized first to enforce initial sync
                for (auto&& buf : _md_buffers) buf->prepare_for_streaming(_md_fd);
            }

            // Request streaming for video node
            v4l_uvc_device::prepare_capture_buffers();
        }

        // Retrieve metadata from a dedicated UVC node. For kernels 4.16+
        void v4l_uvc_meta_device::acquire_metadata(buffers_mgr & buf_mgr,fd_set &fds, bool)
        {
            //Use non-blocking metadata node polling
            if(_md_fd > 0 && FD_ISSET(_md_fd, &fds))
            {
                // In scenario if [md+vid] ->[md] ->[md,vid] the third md should not be retrieved but wait for next select
                if (buf_mgr.metadata_size())
                {
                    LOG_WARNING("Metadata override requested but avoided skipped");
                    // D457 wa - return removed
                    //return;
                    // In scenario: {vid[i]} ->{md[i]} ->{md[i+1],vid[i+1]}:
                    // - vid[i] will be uploaded to user callback without metadata (for now)
                    // - md[i] will be dropped
                    // - vid[i+1] and md[i+1] will be uploaded together - back to stable stream
                    auto md_buf = buf_mgr.get_buffers().at(e_metadata_buf);
                    md_buf._data_buf->request_next_frame(md_buf._file_desc,true);
                }
                FD_CLR(_md_fd,&fds);

                v4l2_buffer buf{};
                buf.type = _md_type;
                buf.memory = _use_memory_map ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;

                // W/O multiplexing this will create a blocking call for metadata node
                if(xioctl(_md_fd, VIDIOC_DQBUF, &buf) < 0)
                {
                    if (handle_enodev_on_dqbuf("md fd", _md_fd))
                        return;
                    LOG_DEBUG_V4L("Dequeued empty buf for md fd " << std::dec << _md_fd);
                }

                //V4l debugging message
                auto mdbuf = _md_buffers[buf.index]->get_frame_start();
                auto hwts = *(uint32_t*)((mdbuf+2));
                auto fn = *(uint32_t*)((mdbuf+38));
                LOG_DEBUG_V4L("Dequeued md buf " << std::dec << buf.index << " for fd " << _md_fd << " seq " << buf.sequence
                             << " fn " << fn << " hw ts " << hwts
                              << " v4lbuf ts usec " << buf.timestamp.tv_usec);

                auto buffer = _md_buffers[buf.index];
                buf_mgr.handle_buffer(e_metadata_buf, _md_fd, buf, buffer);

                // pushing metadata buffer to syncer
                _video_md_syncer.push_metadata({std::make_shared<v4l2_buffer>(buf), _md_fd, buf.index});
                buf_mgr.handle_buffer(e_metadata_buf, -1);
            }
        }
    }  // namespace platform
}  // namespace librealsense
