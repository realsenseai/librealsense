// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "v4l-ioctl.h"

#include <src/librealsense-exception.h>
#include <rsutils/string/from.h>

#include <chrono>
#include <cstring>
#include <functional>
#include <memory>
#include <thread>

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace librealsense
{
    namespace platform
    {
        int xioctl(int fh, unsigned long request, void *arg)
        {
            int r = 0;
            do {
                r = ioctl(fh, request, arg);
            } while (r < 0 && errno == EINTR);
            return r;
        }

        int open_v4l_node( const std::string & name )
        {
            auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds( 5 );
            int fd, open_errno = 0;
            // /run/udev/queue exists while udev still has events pending, so a node it has not reached yet
            // is not really ours to give up on.
            while( ( fd = open( name.c_str(), O_RDWR | O_NONBLOCK, 0 ) ) < 0  &&  ( open_errno = errno ) == EACCES
                   &&  ! access( "/run/udev/queue", F_OK )
                   &&  std::chrono::steady_clock::now() < deadline )
                std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
            if( fd < 0 )
                errno = open_errno;  // access() above may have overwritten what the caller reports
            return fd;
        }

        // Retrieve device video capabilities to discriminate video capturing and metadata nodes
        v4l2_capability get_dev_capabilities(const std::string dev_name)
        {
            // RAII to handle exceptions
            std::unique_ptr<int, std::function<void(int*)> > fd(
                        new int (open(dev_name.c_str(), O_RDWR | O_NONBLOCK, 0)),
                        [](int* d){ if (d && (*d)) {::close(*d); } delete d; });

            if(*fd < 0)
                throw linux_backend_exception(rsutils::string::from() << __FUNCTION__ << ": Cannot open '" << dev_name);

            v4l2_capability cap = {};
            if(xioctl(*fd, VIDIOC_QUERYCAP, &cap) < 0)
            {
                if(errno == EINVAL)
                    throw linux_backend_exception(rsutils::string::from() << __FUNCTION__ << " " << dev_name << " is no V4L2 device");
                else
                    throw linux_backend_exception(rsutils::string::from() <<__FUNCTION__ << " xioctl(VIDIOC_QUERYCAP) failed");
            }

            return cap;
        }

        std::string fourcc_to_string(uint32_t id)
        {
            uint32_t device_fourcc = id;
            char fourcc_buff[sizeof(device_fourcc)+1];
            std::memcpy( fourcc_buff, &device_fourcc, sizeof( device_fourcc ) );
            fourcc_buff[sizeof(device_fourcc)] = 0;
            return fourcc_buff;
        }

        void stream_ctl_on(int fd, v4l2_buf_type type)
        {
            if(xioctl(fd, VIDIOC_STREAMON, &type) < 0)
                throw linux_backend_exception(rsutils::string::from() << "xioctl(VIDIOC_STREAMON) failed for buf_type=" << type);
        }

        void stream_off(int fd, v4l2_buf_type type)
        {
            if(xioctl(fd, VIDIOC_STREAMOFF, &type) < 0)
                throw linux_backend_exception(rsutils::string::from() << "xioctl(VIDIOC_STREAMOFF) failed for buf_type=" << type);
        }

        void req_io_buff(int fd, uint32_t count, std::string dev_name,
                        v4l2_memory mem_type, v4l2_buf_type type)
        {
            struct v4l2_requestbuffers req = { count, type, mem_type, {}};

            if(xioctl(fd, VIDIOC_REQBUFS, &req) < 0)
            {
                if(errno == EINVAL)
                    LOG_ERROR(dev_name + " does not support memory mapping");
                else
                    return;
                    //D457 - fails on close (when num = 0)
                    //throw linux_backend_exception("xioctl(VIDIOC_REQBUFS) failed");
            }
        }
    }  // namespace platform
}  // namespace librealsense
