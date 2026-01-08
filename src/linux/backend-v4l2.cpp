// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015-2024 RealSense, Inc. All Rights Reserved.

#include "backend-v4l2.h"
#include <src/platform/command-transfer.h>
#include <src/platform/hid-data.h>
#include <src/core/time-service.h>
#include <src/core/notification.h>
#include "backend-hid.h"
#include "backend.h"
#include "types.h"
#if defined(USING_UDEV)
#include "udev-device-watcher.h"
#else
#include "../polling-device-watcher.h"
#endif
#include "usb/usb-enumerator.h"
#include "usb/usb-device.h"

#include <rsutils/string/from.h>

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <algorithm>
#include <functional>
#include <string>
#include <sstream>
#include <fstream>
#include <regex>
#include <thread>
#include <utility> // for pair
#include <chrono>
#include <thread>
#include <atomic>
#include <iomanip> // std::put_time

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <cmath>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h> // minor(...), major(...)
#include <linux/usb/video.h>
#include <linux/media.h>
#include <linux/uvcvideo.h>
#include <linux/videodev2.h>
#include <regex>
#include <list>

#include <cstddef> // offsetof

#include <sys/signalfd.h>
#include <signal.h>
#include "rsutils/accelerators/gpu.h"

#pragma GCC diagnostic ignored "-Woverflow"

const size_t MAX_DEV_PARENT_DIR = 10;
const double DEFAULT_KPI_FRAME_DROPS_PERCENTAGE = 0.05;

//D457 Dev. TODO -shall be refactored into the kernel headers.
constexpr uint32_t RS_STREAM_CONFIG_0                       = 0x4000;
constexpr uint32_t RS_CAMERA_CID_BASE                       = (V4L2_CTRL_CLASS_CAMERA | RS_STREAM_CONFIG_0);
constexpr uint32_t RS_CAMERA_CID_LASER_POWER                = (RS_CAMERA_CID_BASE+1);
constexpr uint32_t RS_CAMERA_CID_MANUAL_LASER_POWER         = (RS_CAMERA_CID_BASE+2);
constexpr uint32_t RS_CAMERA_DEPTH_CALIBRATION_TABLE_GET    = (RS_CAMERA_CID_BASE+3);
constexpr uint32_t RS_CAMERA_DEPTH_CALIBRATION_TABLE_SET    = (RS_CAMERA_CID_BASE+4);
constexpr uint32_t RS_CAMERA_COEFF_CALIBRATION_TABLE_GET    = (RS_CAMERA_CID_BASE+5);
constexpr uint32_t RS_CAMERA_COEFF_CALIBRATION_TABLE_SET    = (RS_CAMERA_CID_BASE+6);
constexpr uint32_t RS_CAMERA_CID_FW_VERSION                 = (RS_CAMERA_CID_BASE+7);
constexpr uint32_t RS_CAMERA_CID_GVD                        = (RS_CAMERA_CID_BASE+8);
constexpr uint32_t RS_CAMERA_CID_AE_ROI_GET                 = (RS_CAMERA_CID_BASE+9);
constexpr uint32_t RS_CAMERA_CID_AE_ROI_SET                 = (RS_CAMERA_CID_BASE+10);
constexpr uint32_t RS_CAMERA_CID_AE_SETPOINT_GET            = (RS_CAMERA_CID_BASE+11);
constexpr uint32_t RS_CAMERA_CID_AE_SETPOINT_SET            = (RS_CAMERA_CID_BASE+12);
constexpr uint32_t RS_CAMERA_CID_ERB                        = (RS_CAMERA_CID_BASE+13);
constexpr uint32_t RS_CAMERA_CID_EWB                        = (RS_CAMERA_CID_BASE+14);
constexpr uint32_t RS_CAMERA_CID_HWMC_LEGACY                = (RS_CAMERA_CID_BASE+15);

//const uint32_t RS_CAMERA_GENERIC_XU                     = (RS_CAMERA_CID_BASE+15); // RS_CAMERA_CID_HWMC duplicate??
constexpr uint32_t RS_CAMERA_CID_LASER_POWER_MODE           = (RS_CAMERA_CID_BASE+16); // RS_CAMERA_CID_LASER_POWER duplicate ???
constexpr uint32_t RS_CAMERA_CID_MANUAL_EXPOSURE            = (RS_CAMERA_CID_BASE+17);
constexpr uint32_t RS_CAMERA_CID_LASER_POWER_LEVEL          = (RS_CAMERA_CID_BASE+18); // RS_CAMERA_CID_MANUAL_LASER_POWER ??
constexpr uint32_t RS_CAMERA_CID_EXPOSURE_MODE              = (RS_CAMERA_CID_BASE+19);
constexpr uint32_t RS_CAMERA_CID_WHITE_BALANCE_MODE         = (RS_CAMERA_CID_BASE+20); // Similar to RS_CAMERA_CID_EWB ??
constexpr uint32_t RS_CAMERA_CID_PRESET                     = (RS_CAMERA_CID_BASE+21);
constexpr uint32_t RS_CAMERA_CID_EMITTER_FREQUENCY          = (RS_CAMERA_CID_BASE+22); // [MIPI - Select projector frequency values: 0->57[KHZ], 1->91[KHZ]
constexpr uint32_t RS_CAMERA_CID_HWMC                       = (RS_CAMERA_CID_BASE+32);

/* refe4rence for kernel 4.9 to be removed
#define UVC_CID_GENERIC_XU          (V4L2_CID_PRIVATE_BASE+15)
#define UVC_CID_LASER_POWER_MODE    (V4L2_CID_PRIVATE_BASE+16)
#define UVC_CID_MANUAL_EXPOSURE     (V4L2_CID_PRIVATE_BASE+17)
#define UVC_CID_LASER_POWER_LEVEL   (V4L2_CID_PRIVATE_BASE+18)
#define UVC_CID_EXPOSURE_MODE       (V4L2_CID_PRIVATE_BASE+19)
#define UVC_CID_WHITE_BALANCE_MODE  (V4L2_CID_PRIVATE_BASE+20)
#define UVC_CID_PRESET              (V4L2_CID_PRIVATE_BASE+21)
UVC_CID_MANUAL_EXPOSURE
*/
#ifdef ANDROID

// https://android.googlesource.com/platform/bionic/+/master/libc/include/bits/lockf.h
#define F_ULOCK 0
#define F_LOCK 1
#define F_TLOCK 2
#define F_TEST 3

// https://android.googlesource.com/platform/bionic/+/master/libc/bionic/lockf.cpp
int lockf64(int fd, int cmd, off64_t length)
{
    // Translate POSIX lockf into fcntl.
    struct flock64 fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_whence = SEEK_CUR;
    fl.l_start = 0;
    fl.l_len = length;

    if (cmd == F_ULOCK) {
        fl.l_type = F_UNLCK;
        cmd = F_SETLK64;
        return fcntl(fd, F_SETLK64, &fl);
    }

    if (cmd == F_LOCK) {
        fl.l_type = F_WRLCK;
        return fcntl(fd, F_SETLKW64, &fl);
    }

    if (cmd == F_TLOCK) {
        fl.l_type = F_WRLCK;
        return fcntl(fd, F_SETLK64, &fl);
    }
    if (cmd == F_TEST) {
        fl.l_type = F_RDLCK;
        if (fcntl(fd, F_GETLK64, &fl) == -1) return -1;
        if (fl.l_type == F_UNLCK || fl.l_pid == getpid()) return 0;
        errno = EACCES;
        return -1;
    }

    errno = EINVAL;
    return -1;
}

int lockf(int fd, int cmd, off_t length)
{
    return lockf64(fd, cmd, length);
}
#endif

namespace librealsense
{
    namespace platform
    {
        named_mutex::named_mutex(const std::string& device_path, unsigned timeout)
            : _device_path(device_path),
              _timeout(timeout), // TODO: try to lock with timeout
              _fildes( open( _device_path.c_str(), O_RDWR, 0 ) ),
              _lock_counter(0)
        {
            if( _fildes < 0)
                throw linux_backend_exception( rsutils::string::from() << __FUNCTION__ << ": Cannot open '" << _device_path << "'" );
        }

        named_mutex::~named_mutex()
        {
            try
            {
                // OFD lock will automatically release if locked only when file description closes (not file descriptor)
                // If process was forked or cloned and unlock() was not properly called after a lock() then file will
                // remain locked for the child process
                close( _fildes );
            }
            catch(...)
            {
                LOG_DEBUG( "Error while unlocking mutex" );
            }
        }

        void named_mutex::lock()
        {
            if( _lock_counter.fetch_add( 1 ) == 0 )
            {
                // Using OFD locks to synchronize both interprocess and interthread.
                // flock() is not good if we have multiple instances of the same device. i.e.
                //     auto devs = context.query_devices();
                //     auto dev1 = devs[0];
                //     auto dev2 = devs[0];
                // Closing file descriptor for one instance will unlock file for other instances.
                // Note - OFD locks are linux standart not POSIX.
                struct flock fl;
                memset( &fl, 0, sizeof( fl ) );
                fl.l_whence = SEEK_SET;
                fl.l_start = 0;
                fl.l_len = 0; // Lock the entire file
                fl.l_type = F_WRLCK;
                auto ret = fcntl( _fildes, F_OFD_SETLKW, &fl );
                if( 0 != ret )
                {
                    _lock_counter.fetch_add( -1 );
                    throw linux_backend_exception( rsutils::string::from() << __FUNCTION__ << ": locking failed" );
                }
            }
        }

        void named_mutex::unlock()
        {
            if( _lock_counter.fetch_add( -1 ) == 1 )
            {
                struct flock fl;
                memset( &fl, 0, sizeof( fl ) );
                fl.l_whence = SEEK_SET;
                fl.l_start = 0;
                fl.l_len = 0;
                fl.l_type = F_UNLCK;
                auto ret = fcntl( _fildes, F_OFD_SETLKW, &fl );
                if( 0 != ret )
                    throw linux_backend_exception( rsutils::string::from() << __FUNCTION__ << ": unlocking failed" );
            }
        }

        bool named_mutex::try_lock()
        {
            if( _lock_counter.fetch_add( 1 ) == 0 )
            {
                struct flock fl;
                memset( &fl, 0, sizeof( fl ) );
                fl.l_whence = SEEK_SET;
                fl.l_start = 0;
                fl.l_len = 0; // Lock the entire file
                fl.l_type = F_WRLCK;
                auto ret = fcntl( _fildes, F_OFD_SETLK, &fl );
                if( 0 != ret && errno == EAGAIN)
                {
                    _lock_counter.fetch_add( -1 );
                    return false;
                }
            }

            return true;
        }

        static int xioctl(int fh, unsigned long request, void *arg)
        {
            int r=0;
            do {
                r = ioctl(fh, request, arg);
            } while (r < 0 && errno == EINTR);
            return r;
        }

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
            }
            else
            {
                //_length += (V4L2_BUF_TYPE_VIDEO_CAPTURE==type) ? MAX_META_DATA_SIZE : 0;
                _start = static_cast<uint8_t*>(malloc( _length));
                if (!_start) throw linux_backend_exception("User_p allocation failed!");
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
               if(munmap(_start, _original_length) < 0)
                   LOG_DEBUG_V4L("munmap failed on buffer Dtor");
            }
            else
            {
               free(_start);
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

        // retrieve the USB specification attributed to a specific USB device.
        // This functionality is required to find the USB connection type for UVC device
        // Note that the input parameter is passed by value
        static usb_spec get_usb_connection_type(std::string path)
        {
            usb_spec res{usb_undefined};

            char usb_actual_path[PATH_MAX] = {0};
            if (realpath(path.c_str(), usb_actual_path) != nullptr)
            {
                path = std::string(usb_actual_path);
                std::string camera_usb_version;
                if(!(std::ifstream(path + "/version") >> camera_usb_version))
                    throw linux_backend_exception("Failed to read usb version specification");

                // go through the usb_name_to_spec map to find a usb type where the first element is contained in 'camera_usb_version'
                // (contained and not strictly equal because of differences like "3.2" vs "3.20")
                for(const auto usb_type : usb_name_to_spec ) {
                    std::string usb_name = usb_type.first;
                    if (std::string::npos != camera_usb_version.find(usb_name))
                    {
                         res = usb_type.second;
                         return res;
                    }
                }
            }
            return res;
        }

        // Retrieve device video capabilities to discriminate video capturing and metadata nodes
        static v4l2_capability get_dev_capabilities(const std::string dev_name)
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

        void stream_ctl_on(int fd, v4l2_buf_type type=V4L2_BUF_TYPE_VIDEO_CAPTURE)
        {
            if(xioctl(fd, VIDIOC_STREAMON, &type) < 0)
                throw linux_backend_exception(rsutils::string::from() << "xioctl(VIDIOC_STREAMON) failed for buf_type=" << type);
        }

        void stream_off(int fd, v4l2_buf_type type=V4L2_BUF_TYPE_VIDEO_CAPTURE)
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

        std::vector<v4l_uvc_device::path_and_identifier> v4l_uvc_device::collect_dev_video_path_and_identifier()
        {
            // building vector of /dev/videoX files with path, major, minor
            std::vector<path_and_identifier> dev_videos;
            DIR * dev_dir = opendir("/dev");
            if (!dev_dir)
            {
                LOG_ERROR("Cannot access /dev");
                throw linux_backend_exception(rsutils::string::from() << "Cannot access /dev");
            }

            // searching for match in /dev, in means of major, minor
            while (dirent * entry = readdir(dev_dir))
            {
                std::string name = entry->d_name;
                std::string dev_path = "/dev/" + name;

                struct stat st = {};
                if (stat(dev_path.c_str(), &st) < 0)
                {
                    continue;
                }
                dev_videos.push_back({dev_path, {major(st.st_rdev), minor(st.st_rdev)}});
            }
            closedir(dev_dir);

            return dev_videos;
        }

        bool v4l_uvc_device::get_identifier_from_v4l_video_path(const std::string& v4l_video_path, identifier& key)
        {
            // dev file is in paths:
            // - /sys/class/video4linux/videoX/dev for video files
            // - /sys/class/d4xx-class/d4xx-dfu-30-XXXa for mipi dfu files

            // dev file contains major_number:minor_number
            // while major_number is typically 81 for video4linux devices and 506 for d4xx-class devices

            std::ifstream dev_file(v4l_video_path + "/dev");
            if (!dev_file)
            {
                LOG_ERROR("Cannot access " + v4l_video_path + "/dev");
                return false;
            }

            std::string dev_line;
            std::getline(dev_file, dev_line);
            char sep = '\0';
            std::istringstream iss(dev_line);
            if (!(iss >> key.major >> sep >> key.minor) || sep != ':') {
                LOG_ERROR("Could not read major and minor from " + v4l_video_path + "/dev");
                return false;
            }
            return true;
        }

        std::vector<std::pair <std::string, std::string>> v4l_uvc_device::generate_v4l_to_dev_video_paths(const std::vector<path_and_identifier>& v4l_videos,
                                                                                                          const std::vector<path_and_identifier>& dev_videos)
        {
            std::vector<std::pair<std::string, std::string>> v4l_to_dev_video_paths;

            // going over video paths like /sys/class/video4linux/videoX
            // find their mapping in /dev/videoY
            // above videoX and videoY are often the same, but not always
            // for example when working with unprivileged containers, the name in /dev may not contain the string "video"
            // it depends on how the devices have been mapped (bind-mounted) into the container
            for(auto&& v4l_video : v4l_videos)
            {
                // searching for match in /dev, in means of major, minor
                for (auto&& dev_video : dev_videos)
                {
                    if (v4l_video.key== dev_video.key)
                    {
                        v4l_to_dev_video_paths.push_back(std::make_pair(v4l_video.path, dev_video.path));
                        break;
                    }
                }
            }
            return v4l_to_dev_video_paths;
        }

        bool v4l_uvc_device::get_devname_from_mipi_dfu_path(const path_and_identifier& dfu_path, std::string& dev_name)
        {
            DIR * dev_dir = opendir("/dev");
            if (!dev_dir)
            {
                LOG_ERROR("Cannot access /dev");
                throw linux_backend_exception(rsutils::string::from() << "Cannot access /dev");
            }

            // searching for match in /dev, in means of major, minor
            while (dirent * entry = readdir(dev_dir))
            {
                std::string name = entry->d_name;
                if (name.compare(0, 8, "d4xx-dfu") != 0)
                {
                    continue;
                }
                std::string dev_path = "/dev/" + name;

                struct stat st = {};
                if (stat(dev_path.c_str(), &st) < 0)
                {
                    continue;
                }
                identifier st_key {major(st.st_rdev), minor(st.st_rdev)};
                if (dfu_path.key == st_key)
                {
                    dev_name = name;
                    closedir(dev_dir);
                    return true;
                }
            }
            closedir(dev_dir);
            return false;
        }

        bool v4l_uvc_device::get_devname_from_v4l_video_path(const std::string& v4l_video_path, std::string& dev_name,
                                                         const std::vector<std::pair <std::string, std::string>>& v4l_to_dev_video_paths)
        {
            for (auto&& v4l_to_dev_pair : v4l_to_dev_video_paths)
            {
                if (v4l_to_dev_pair.first == v4l_video_path)
                {
                    dev_name = v4l_to_dev_pair.second;
                    return true;
                }
            }
            return false;
        }

        std::vector<v4l_uvc_device::path_and_identifier> v4l_uvc_device::collect_v4l_video_path_and_identifier()
        {
            std::vector<path_and_identifier> v4l_videos;
            // Enumerate all subdevices present on the system
            DIR * dir = opendir("/sys/class/video4linux");
            if(!dir)
            {
                LOG_INFO("Cannot access /sys/class/video4linux");
                return v4l_videos;
            }
            while (dirent * entry = readdir(dir))
            {
                std::string name = entry->d_name;
                if(name == "." || name == "..") continue;

                // Resolve a pathname to ignore virtual video devices and  sub-devices
                static const std::regex video_dev_pattern("(\\/video\\d+)$");

                std::string path = "/sys/class/video4linux/" + name;
                std::string v4l_path{};
                char buff[PATH_MAX] = {0};
                if (realpath(path.c_str(), buff) != nullptr)
                {
                    v4l_path = std::string(buff);
                    if (v4l_path.find("virtual") != std::string::npos)
                        continue;
                    if (!std::regex_search(v4l_path, video_dev_pattern))
                    {
                        //LOG_INFO("Skipping Video4Linux entry " << v4l_path << " - not a device");
                        continue;
                    }
                    identifier key{0, 0};
                    if (!get_identifier_from_v4l_video_path(v4l_path, key))
                        continue;

                    v4l_videos.push_back({v4l_path, key});
                }
            }
            closedir(dir);

            // UVC nodes shall be traversed in ascending order for metadata nodes assignment ("dev/video1, Video2..
            // Replace lexicographic with numeric sort to ensure "video2" is listed before "video11"
            std::sort(v4l_videos.begin(), v4l_videos.end(),
                      [](const path_and_identifier& first, const path_and_identifier& second)
            {
                // getting videoXX
                std::string first_video = first.path.substr(first.path.find_last_of('/') + 1);
                std::string second_video = second.path.substr(second.path.find_last_of('/') + 1);

                // getting the index XX from videoXX
                std::stringstream first_index(first_video.substr(first_video.find_first_of("0123456789")));
                std::stringstream second_index(second_video.substr(second_video.find_first_of("0123456789")));
                int left_id = 0, right_id = 0;
                first_index >> left_id;
                second_index >> right_id;
                return left_id < right_id;
            });
            return v4l_videos;
        }

        std::vector<std::string> v4l_uvc_device::get_mipi_dfu_paths()
        {
            std::vector<std::string> dfu_paths;
            static const std::regex dfu_dev_pattern("./d4xx-dfu.");
            // Enumerate all d4xx dfu devices present on the system
            DIR * dir = opendir("/sys/class/d4xx-class");
            if(!dir)
            {
                LOG_INFO("Cannot access /sys/class/d4xx-class");
                return dfu_paths;
            }
            while (dirent * entry = readdir(dir))
            {
                std::string name = entry->d_name;
                if(name == "." || name == "..") continue;
                std::string path = "/sys/class/d4xx-class/" + name;
                std::string real_path{};
                char buff[PATH_MAX] = {0};
                if (realpath(path.c_str(), buff) != nullptr)
                {
                    real_path = std::string(buff);
                    if (real_path.find("virtual") == std::string::npos)
                        continue;
                    if (!std::regex_search(real_path, dfu_dev_pattern))
                    {
                        continue;
                    }
                    identifier key{0, 0};
                    if (!get_identifier_from_v4l_video_path(real_path, key))
                    {
                        continue;
                    }

                    path_and_identifier dfu_path {real_path, key};
                    std::string devname;
                    if (get_devname_from_mipi_dfu_path(dfu_path, devname))
                    {
                        if (devname.empty())
                        {
                            LOG_ERROR("No DEVNAME found for " + real_path);
                            continue;
                        }
                        if ( std::find(dfu_paths.begin(), dfu_paths.end(), devname) == dfu_paths.end() )
                        {
                            dfu_paths.push_back(devname);
                        }
                    }
                    else
                    {
                        continue;
                    }
                }
            }
            closedir(dir);
            return dfu_paths;
        }

        bool v4l_uvc_device::is_usb_path_valid(const std::string& usb_video_path, const std::string& dev_name,
                                               std::string& busnum, std::string& devnum, std::string& devpath)
        {
            struct stat st = {};
            if(stat(dev_name.c_str(), &st) < 0)
            {
                throw linux_backend_exception(rsutils::string::from() << "Cannot identify '" << dev_name);
            }
            if(!S_ISCHR(st.st_mode))
                throw linux_backend_exception(dev_name + " is no device");

            // Search directory and up to three parent directories to find busnum/devnum
            auto valid_path = false;
            std::ostringstream ss;
            ss << "/sys/dev/char/" << major(st.st_rdev) << ":" << minor(st.st_rdev) << "/device/";

            auto char_dev_path = ss.str();

            for(auto i=0U; i < MAX_DEV_PARENT_DIR; ++i)
            {
                if(std::ifstream(char_dev_path + "busnum") >> busnum)
                {
                    if(std::ifstream(char_dev_path + "devnum") >> devnum)
                    {
                        if(std::ifstream(char_dev_path + "devpath") >> devpath)
                        {
                            valid_path = true;
                            break;
                        }
                    }
                }
                char_dev_path += "../";
            }
            return valid_path;
        }

        uvc_device_info v4l_uvc_device::get_info_from_usb_device_path(const std::string& video_path, const std::string& dev_name, const std::string& name,
                                                                      const std::vector<std::pair <std::string, std::string>>& sys_to_dev_video_paths)
        {
            std::string busnum, devnum, devpath;

            if (!is_usb_path_valid(video_path, dev_name, busnum, devnum, devpath))
            {
#ifndef RS2_USE_CUDA
                if (rsutils::rs2_is_gpu_available())
                {
                    /* On the Jetson TX, the camera module is CSI & I2C and does not report as this code expects
                    Patch suggested by JetsonHacks: https://github.com/jetsonhacks/buildLibrealsense2TX */
                    LOG_INFO("Failed to read busnum/devnum. Device Path: " << ("/sys/class/video4linux/" + name));
                }
#endif
               throw linux_backend_exception("Failed to read busnum/devnum of usb device");
            }

            LOG_INFO("Enumerating UVC " << name << " v4l_path=" << video_path << " dev_name=" << dev_name);
            uint16_t vid{}, pid{}, mi{};
            usb_spec usb_specification(usb_undefined);

            std::string modalias;
            if(!(std::ifstream("/sys/class/video4linux/" + name + "/device/modalias") >> modalias))
                throw linux_backend_exception("Failed to read modalias");
            if(modalias.size() < 14 || modalias.substr(0,5) != "usb:v" || modalias[9] != 'p')
                throw linux_backend_exception("Not a usb format modalias");
            if(!(std::istringstream(modalias.substr(5,4)) >> std::hex >> vid))
                throw linux_backend_exception("Failed to read vendor ID");
            if(!(std::istringstream(modalias.substr(10,4)) >> std::hex >> pid))
                throw linux_backend_exception("Failed to read product ID");
            if(!(std::ifstream("/sys/class/video4linux/" + name + "/device/bInterfaceNumber") >> std::hex >> mi))
                throw linux_backend_exception("Failed to read interface number");

            // Find the USB specification (USB2/3) type from the underlying device
            // Use device mapping obtained in previous step to traverse node tree
            // and extract the required descriptors
            // Traverse from
            // /sys/devices/pci0000:00/0000:00:xx.0/ABC/M-N/3-6:1.0/video4linux/video0
            // to
            // /sys/devices/pci0000:00/0000:00:xx.0/ABC/M-N/version
            usb_specification = get_usb_connection_type(video_path + "/../../../");

            uvc_device_info info{};
            info.pid = pid;
            info.vid = vid;
            info.mi = mi;
            info.id = dev_name;
            info.device_path = video_path;
            info.unique_id = busnum + "-" + devpath + "-" + devnum;
            info.conn_spec = usb_specification;
            info.uvc_capabilities = get_dev_capabilities(dev_name).device_caps;

            return info;
        }

        bool v4l_uvc_device::is_format_supported_on_node(const std::string& dev_name, std::string v4l_4cc_fmt)
        {
            int fd = open(dev_name.c_str(), O_RDWR);
            if (fd < 0)
                throw linux_backend_exception("Mipi device format could not be grabbed");

            struct v4l2_fmtdesc fmtdesc;
            memset(&fmtdesc,0,sizeof(fmtdesc));
            fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

            uint32_t format;
            memcpy(&format, v4l_4cc_fmt.c_str(), sizeof(format));

            while (ioctl(fd,VIDIOC_ENUM_FMT,&fmtdesc) == 0)
            {
                if (fmtdesc.pixelformat == format)
                {
                    ::close(fd);
                    return true;
                }

                fmtdesc.index++;
            }

            ::close(fd);

            return false;
        }

        bool v4l_uvc_device::is_device_depth_node(const std::string& dev_name)
        {
            bool is_depth = false;

            // first search video node links, for example, video-rs-depth-0
            std::smatch match;
            static std::regex video_dev_rs("video-rs-");
            static std::regex video_dev_depth("video-rs-depth-\\d+$");
            if(std::regex_search(dev_name, match, video_dev_rs))
            {
                if (std::regex_search(dev_name, match, video_dev_depth))
                    is_depth = true;
                else
                    is_depth = false;
            }
            // then search video nodes to find the depth node
            else if (is_format_supported_on_node(dev_name, "Z16 "))
                is_depth = true;
            else
                is_depth = false;

            return is_depth;
        }

        uint16_t v4l_uvc_device::get_mipi_device_pid(const std::string& dev_name)
        {
            // GVD product ID
            const uint8_t GVD_PID_OFFSET    = 4;

            const uint8_t GVD_PID_D457      = 0x12;
            const uint8_t GVD_PID_D401_GMSL = 0x13;
            const uint8_t GVD_PID_D430_GMSL = 0x0F;
            const uint8_t GVD_PID_D415_GMSL = 0x06;

            // device PID
            uint16_t device_pid = 0;

            int fd = open(dev_name.c_str(), O_RDWR);
            if (fd < 0)
                throw linux_backend_exception("Mipi device PID could not be found");

            uint8_t gvd[276];
            struct v4l2_ext_control ctrl;

            memset(gvd,0,276);

            ctrl.id = RS_CAMERA_CID_GVD;
            ctrl.size = sizeof(gvd);
            ctrl.p_u8 = gvd;

            struct v4l2_ext_controls ext;

            ext.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
            ext.controls = &ctrl;
            ext.count = 1;

            int retries = 5;
            bool opcode_ok = false;
            while (!opcode_ok && retries--)
            {
                if (xioctl(fd, VIDIOC_G_EXT_CTRLS, &ext) == 0)
                {
                    auto opcode = gvd[0];
                    if (opcode != 0x10)
                    {
                        LOG_WARNING("Wrong opcode when pulling GVD: gvd[0] returned as: " << opcode);
                        continue;
                    }
                    else {
                        opcode_ok = true;
                    }

                    uint8_t product_pid = gvd[4 + GVD_PID_OFFSET];

                    switch(product_pid)
                    {
                        case(GVD_PID_D457):
                            device_pid = D457_PID;
                            break;

                        case(GVD_PID_D430_GMSL):
                            device_pid = D430_GMSL_PID;
                            break;

                        case(GVD_PID_D415_GMSL):
                            device_pid = D415_GMSL_PID;
                            break;

                        case(GVD_PID_D401_GMSL):
                            device_pid = D401_GMSL_PID;
                            break;

                        default:
                            LOG_WARNING("Unidentified MIPI device product id: 0x" << std::hex << (int) product_pid << "remaining retries = " << retries);
                            device_pid = 0x0000;
                            break;
                    }
                }
                std::this_thread::sleep_for( std::chrono::milliseconds(100));
            }

            ::close(fd);

            return device_pid;
        }

        void v4l_uvc_device::get_mipi_device_info(const std::string& dev_name,
                                                  std::string& bus_info, std::string& card)
        {
            struct v4l2_capability vcap;
            int fd = open(dev_name.c_str(), O_RDWR);
            if (fd < 0)
                throw linux_backend_exception("Mipi device capability could not be grabbed");
            int err = ioctl(fd, VIDIOC_QUERYCAP, &vcap);
            if (err)
            {
                struct media_device_info mdi;

                err = ioctl(fd, MEDIA_IOC_DEVICE_INFO, &mdi);
                if (!err)
                {
                    if (mdi.bus_info[0])
                        bus_info = mdi.bus_info;
                    else
                        bus_info = std::string("platform:") + mdi.driver;

                    if (mdi.model[0])
                        card = mdi.model;
                    else
                        card = mdi.driver;
                 }
            }
            else
            {
                bus_info = reinterpret_cast<const char *>(vcap.bus_info);
                card = reinterpret_cast<const char *>(vcap.card);
            }

            ::close(fd);
        }

        uvc_device_info v4l_uvc_device::get_info_from_mipi_device_path(const std::string& video_path, const std::string& name)
        {
            uint16_t vid{}, pid{}, mi{};
            usb_spec usb_specification(usb_undefined);
            std::string bus_info, card;

            auto dev_name = "/dev/" + name;

            get_mipi_device_info(dev_name, bus_info, card);

            // find device PID from depth video node
            static uint16_t device_pid = 0;
            try
            {
                if (is_device_depth_node(dev_name))
                {
                    device_pid = get_mipi_device_pid(dev_name);
                }
            }
            catch(const std::exception & e)
            {
                LOG_WARNING("MIPI device product id detection issue, device will be skipped: " << e.what());
                device_pid = 0;
            }

            vid = 0x8086;
            pid = device_pid;

//          std::cout << "video_path:" << video_path << ", name:" << dev_name << ", pid=" << std::hex << (int) pid << std::endl;

            static std::regex video_dev_index("\\d+$");
            std::smatch match;
            uint8_t ind{};
            if (std::regex_search(name, match, video_dev_index))
            {
                ind = static_cast<uint8_t>(std::stoi(match[0]));
            }
            else
            {
                LOG_WARNING("Unresolved Video4Linux device pattern: " << name << ", device is skipped");
                throw linux_backend_exception("Unresolved Video4Linux device, device is skipped");
            }

            //  D457 exposes (assuming first_video_index = 0):
            // - video0 for Depth and video1 for Depth's md.
            // - video2 for RGB and video3 for RGB's md.
            // - video4 for IR and video5 for IR's md.
            // - video6 for IMU (accel or gyro TBD)
            // next several lines permit to use D457 even if a usb device has already "taken" the video0,1,2 (for example)
            // further development is needed to permit use of several mipi devices
            static int  first_video_index = ind;

            // Use camera_video_nodes as number of /dev/video[%d] for each camera sensor subset
            const int camera_video_nodes = 7;
            int cam_id = ind / camera_video_nodes;
            ind = (ind - first_video_index) % camera_video_nodes; // offset from first mipi video node and assume 6 nodes per mipi camera

            if (device_pid == D401_GMSL_PID)
            {
                if( ind == 0 || ind == 2 || ind == 4 )
                    mi = 0;  // video node indicator
                else if( ind == 1 || ind == 3 || ind == 5 )
                    mi = 3;  // metadata node indicator
                else
                {
                    LOG_WARNING( "Unresolved Video4Linux device mi, device is skipped" );
                    throw linux_backend_exception( "Unresolved Video4Linux device, device is skipped" );
                }
            }
            else
            {
                if( ind == 0 || ind == 2 || ind == 4 )
                    mi = 0;  // video node indicator
                else if( ind == 1 || ind == 3 || ind == 5 )
                    mi = 3;  // metadata node indicator
                else if( ind == 6 )
                    mi = 4;  // IMU node indicator
                else
                {
                    LOG_WARNING( "Unresolved Video4Linux device mi, device is skipped" );
                    throw linux_backend_exception( "Unresolved Video4Linux device, device is skipped" );
                }
            }
            

            uvc_device_info info{};
            info.pid = pid;
            info.vid = vid;
            info.mi = mi;
            info.id = dev_name;
            info.device_path = video_path;
            // unique id for MIPI: This will assign sensor set for each camera.
            // it cannot be generated as in usb, because the params busnum, devpath and devnum
            // are not available via mipi
            // assign unique id for mipi by appending camera id to bus_info (bus_info is same for each mipi port)
            // Note - jetson can use only bus_info, as card is different for each sensor and metadata node.
            info.unique_id = bus_info + "-" + std::to_string(cam_id); // use bus_info as per camera unique id for mipi
            // Get DFU node for MIPI camera
            std::vector<std::string> dfu_device_paths = get_mipi_dfu_paths();

            for (const auto& dfu_device_path: dfu_device_paths) {
                auto mipi_dfu_chardev = "/dev/" + dfu_device_path;
                int vfd = open(mipi_dfu_chardev.c_str(), O_RDONLY | O_NONBLOCK);
                if (vfd >= 0) {
                    // Use legacy DFU device node used in firmware_update_manager
                    info.dfu_device_path = mipi_dfu_chardev;
                    ::close(vfd); // file exists, close file and continue to assign it
                    break;
                }
            }
            info.conn_spec = usb_specification;
            info.uvc_capabilities = get_dev_capabilities(dev_name).device_caps;

            return info;
        }

        bool v4l_uvc_device::is_usb_device_path(const std::string& video_path)
        {
            static const std::regex uvc_pattern("(\\/usb\\d+\\/)\\w+"); // Locate UVC device path pattern ../usbX/...
            return std::regex_search(video_path, uvc_pattern);
        }

        void v4l_mipi_device::foreach_mipi_device(
                std::function<void(const mipi_device_info&,
                                   const std::string&)> action)
        {
            typedef std::pair<mipi_device_info,std::string> node_info;
            std::vector<node_info> mipi_devices;

            std::vector<std::string> mipi_dfu_paths = get_mipi_dfu_paths();
            for(auto it = mipi_dfu_paths.begin(); mipi_dfu_paths.size() && it != mipi_dfu_paths.end(); ++it)
            {
                auto mipi_dfu_path = "/dev/" + *it;
                std::string dfu_ver;
                for (int retry = 10; retry; retry--) {
                    std::ifstream fw_path_in_device(mipi_dfu_path);
                    std::getline(fw_path_in_device, dfu_ver);
                    if (dfu_ver.size()) {
                        fw_path_in_device.close();
                        break;
                    }
                    fw_path_in_device.close();
                }
                // look for "recovery" string, if no - skip.
                if (dfu_ver.find("recovery") == std::string::npos)
                    continue;
                mipi_device_info info{};
                info.pid = 0xbbcd;
                info.vid = 0x8086;
                info.id = *it;
                info.device_path = mipi_dfu_path;
                info.unique_id = *it;
                info.dfu_device_path = mipi_dfu_path;
                // The expected string is  "DFU info:  recovery:  209443110028"
                std::string delimiter = ":";
                dfu_ver.erase(0, dfu_ver.find(delimiter) + delimiter.length());
                dfu_ver.erase(0, dfu_ver.find(delimiter) + delimiter.length());
                // Trim leading whitespace
                dfu_ver.erase(0, dfu_ver.find_first_not_of(' '));
                info.serial_number = dfu_ver;
                mipi_devices.emplace_back(info, *it);
            }
            try
            {
                // Dispatch registration for enumerated MIPI Recovery devices
                for (auto&& dev : mipi_devices)
                    action(dev.first, dev.second);
            }
            catch(const std::exception & e)
            {
                LOG_ERROR("Registration of MIPI recovery device failed: " << e.what());
            }
        }

        std::vector<node_info> v4l_uvc_device::get_mipi_rs_enum_nodes()
        {
            std::vector<node_info> mipi_rs_enum_nodes;

            /* Enumerate mipi nodes by links with usage of rs-enum script */
            std::vector<std::string> video_sensors = {"depth", "color", "ir", "imu"};
            const int MAX_V4L2_DEVICES = 8; // assume maximum 8 mipi devices

            for ( int i = 0; i < MAX_V4L2_DEVICES; i++ ) {
                for (const auto &vs: video_sensors) {
                    int vfd = -1;
                    std::string device_path = "video-rs-" + vs + "-" + std::to_string(i);
                    std::string device_md_path = "video-rs-" + vs + "-md-" + std::to_string(i);
                    std::string video_path = "/dev/" + device_path;
                    std::string video_md_path = "/dev/" + device_md_path;
                    std::string dfu_device_path = "/dev/d4xx-dfu-" + std::to_string(i);
                    uvc_device_info info{};

                    // Get Video node
                    // Check if file on video_path is exists
                    vfd = open(video_path.c_str(), O_RDONLY | O_NONBLOCK);

                    if (vfd < 0) // file does not exists, continue to the next one
                        continue;
                    else
                        ::close(vfd); // file exists, close file and continue to assign it
                    try
                    {
                        info = get_info_from_mipi_device_path(video_path, device_path);
                    }
                    catch(const std::exception & e)
                    {
                        LOG_WARNING("MIPI video device issue: " << e.what());
                        continue;
                    }

                    // Get DFU node for MIPI camera
                    vfd = open(dfu_device_path.c_str(), O_RDONLY | O_NONBLOCK);

                    if (vfd >= 0) {
                        ::close(vfd); // file exists, close file and continue to assign it
                        info.dfu_device_path = dfu_device_path;
                    }

                    info.mi = vs.compare("imu") ? 0 : 4;
                    info.unique_id += "-" + std::to_string(i);
                    info.uvc_capabilities &= ~(V4L2_CAP_META_CAPTURE); // clean caps
                    mipi_rs_enum_nodes.emplace_back(info, video_path);
                    // Get metadata node
                    // Check if file on video_md_path is exists
                    vfd = open(video_md_path.c_str(), O_RDONLY | O_NONBLOCK);

                    if (vfd < 0) // file does not exists, continue to the next one
                        continue;
                    else
                        ::close(vfd); // file exists, close file and continue to assign it

                    try
                    {
                        info = get_info_from_mipi_device_path(video_md_path, device_md_path);
                    }
                    catch(const std::exception & e)
                    {
                        LOG_WARNING("MIPI video metadata device issue: " << e.what());
                        continue;
                    }
                    info.mi = 3;
                    info.unique_id += "-" + std::to_string(i);
                    mipi_rs_enum_nodes.emplace_back(info, video_md_path);
                }
            }
            return mipi_rs_enum_nodes;
        }

        std::vector<node_info> v4l_uvc_device::match_video_with_metadata_nodes(const std::vector<node_info>& uvc_nodes)
        {
            // Assume uvc_nodes is already sorted according to videoXX (video0, then video1...)
            // Assume for each metadata node with index N there is a origin streaming node with index (N-1)
            std::vector<node_info> uvc_devices;
            for (auto&& cur_node : uvc_nodes)
            {
                try
                {
                    if (!(cur_node.first.uvc_capabilities & V4L2_CAP_META_CAPTURE))
                        uvc_devices.emplace_back(cur_node);
                    else
                    {
                        if (uvc_devices.empty())
                        {
                            LOG_ERROR("UVC meta-node with no video streaming node encountered: " << std::string(cur_node.first));
                            continue;
                        }

                        // Update the preceding uvc item with metadata node info
                        auto uvc_node = uvc_devices.back();

                        if (uvc_node.first.uvc_capabilities & V4L2_CAP_META_CAPTURE)
                        {
                            LOG_ERROR("Consequtive UVC meta-nodes encountered: " << std::string(uvc_node.first) << " and " << std::string(cur_node.first) );
                            continue;
                        }

                        if (uvc_node.first.has_metadata_node)
                        {
                            LOG_ERROR( "Metadata node for uvc device: " << std::string(uvc_node.first) << " was previously assigned ");
                            continue;
                        }

                        // modify the last element with metadata node info
                        uvc_node.first.has_metadata_node = true;
                        uvc_node.first.metadata_node_id = cur_node.first.id;
                        uvc_devices.back() = uvc_node;
                    }
                }
                catch(const std::exception & e)
                {
                    LOG_ERROR("Failed to map meta-node "  << std::string(cur_node.first) <<", error encountered: " << e.what());
                }
            }
            return uvc_devices;
        }

        bool v4l_uvc_device::get_info_from_v4l_video_path(const std::string& v4l_video_path, const std::string& dev_name, uvc_device_info& info, bool is_mipi_rs_enum_nodes_empty,
                                                      const std::vector<std::pair <std::string, std::string>>& v4l_to_dev_video_paths)
        {
            bool res = false;

            // following line grabs video0 from "/sys/devices/.../video0" paths
            auto name = v4l_video_path.substr(v4l_video_path.find_last_of('/') + 1);

            if (is_usb_device_path(v4l_video_path))
            {
                info = get_info_from_usb_device_path(v4l_video_path, dev_name, name, v4l_to_dev_video_paths);
                res = true;
            }
            else if(is_mipi_rs_enum_nodes_empty) //video4linux devices that are not USB devices and not previously enumerated by rs links
            {
                // filter out all possible codecs, work only with compatible driver
                static const std::regex rs_mipi_compatible(".vi:|ipu6");
                info = get_info_from_mipi_device_path(v4l_video_path, name);
                if (regex_search(info.unique_id, rs_mipi_compatible)) {
                    res = true;
                }
            }
            else // continue as we already have mipi nodes enumerated by rs links in uvc_nodes
            {
                // empty
            }
            return res;
        }

        std::vector<node_info> v4l_uvc_device::collect_uvc_nodes(const std::vector<path_and_identifier>& v4l_videos,
                                                                 const std::vector<node_info>& mipi_rs_enum_nodes,
                                                                 const std::vector<std::pair <std::string, std::string>>& v4l_to_dev_video_paths)
        {
            std::vector<node_info> uvc_nodes;
            // Append mipi nodes to uvc nodes list
            if(mipi_rs_enum_nodes.size())
            {
                uvc_nodes.insert(uvc_nodes.end(), mipi_rs_enum_nodes.begin(), mipi_rs_enum_nodes.end());
            }

            for(auto&& v4l_video : v4l_videos)
            {
                try
                {
                    std::string dev_name;
                    if (!get_devname_from_v4l_video_path(v4l_video.path, dev_name, v4l_to_dev_video_paths))
                    {
                        continue;

                    }
                    uvc_device_info info;
                    if (get_info_from_v4l_video_path(v4l_video.path, dev_name, info, mipi_rs_enum_nodes.empty(), v4l_to_dev_video_paths))
                    {
                        uvc_nodes.emplace_back(info, dev_name);
                    }
                }
                catch(const std::exception & e)
                {
                    LOG_INFO("Not a USB video device: " << e.what());
                }
            }
            return uvc_nodes;
        }

        void v4l_uvc_device::foreach_uvc_device(
                std::function<void(const uvc_device_info&,
                                   const std::string&)> action)
        {
            // building vector of /sys/class/video4linux/.../videoX files with path, major, minor
            std::vector<path_and_identifier> v4l_videos = collect_v4l_video_path_and_identifier();
            // building vector of /dev/videoX files with path, major, minor
            std::vector<path_and_identifier> dev_videos = collect_dev_video_path_and_identifier();
            // generate map of "/sys/class/video4linux/.../videoX" to "/dev/videoY"
            auto v4l_to_dev_video_paths = generate_v4l_to_dev_video_paths(v4l_videos, dev_videos);

            std::vector<node_info> mipi_rs_enum_nodes = get_mipi_rs_enum_nodes();

            // Collect UVC nodes info to bundle metadata and video
            std::vector<node_info> uvc_nodes = collect_uvc_nodes(v4l_videos, mipi_rs_enum_nodes, v4l_to_dev_video_paths);

            // Matching video and metadata nodes
            std::vector<node_info> uvc_devices = match_video_with_metadata_nodes(uvc_nodes);

            try
            {
                // Dispatch registration for enumerated uvc devices
                for (auto&& dev : uvc_devices)
                    action(dev.first, dev.second);
            }
            catch(const std::exception & e)
            {
                LOG_ERROR("Registration of UVC device failed: " << e.what());
            }
        }

        bool frame_drop_monitor::update_and_check_kpi(const stream_profile& profile, const timeval& timestamp)
        {
            bool is_kpi_violated = false;
            long int timestamp_usec = static_cast<long int> (timestamp.tv_sec * 1000000 + timestamp.tv_usec);

            // checking if the current profile is already in the drops_per_stream container
            auto it = std::find_if(drops_per_stream.begin(), drops_per_stream.end(), 
                [profile](std::pair<stream_profile, std::deque<long int>>& sp_deq)
                {return  profile == sp_deq.first; });

            // if the profile is already in the drops_per_stream container, 
            // checking kpi with the new partial frame caught
            if (it != drops_per_stream.end())
            {
                // setting the kpi checking to be done on the last 30 seconds
                int time_limit = 30;
                
                // max number of drops that can be received in the time_limit, without violation of the kpi
                int max_num_of_drops = profile.fps * _kpi_frames_drops_pct * time_limit;

                auto& queue_for_profile = it->second;
                // removing too old timestamps of partial frames
                while (queue_for_profile.size() > 0)
                {
                    auto delta_ts_usec = timestamp_usec - queue_for_profile.front();
                    if (delta_ts_usec > (time_limit * 1000000))
                    {
                        queue_for_profile.pop_front();
                    }
                    else
                        break; // correct because the frames are added chronologically
                }
                // checking kpi violation
                if (queue_for_profile.size() >= max_num_of_drops)
                {
                    is_kpi_violated = true;
                    queue_for_profile.clear();
                }
                else
                    queue_for_profile.push_back(timestamp_usec);
            }
            else
            {
                // adding the the current partial frame's profile and timestamp to the container
                std::deque<long int> deque_to_add;
                deque_to_add.push_back(timestamp_usec);
                drops_per_stream.push_back(std::make_pair(profile, deque_to_add));
            }
            return is_kpi_violated;
        }

        v4l_uvc_device::v4l_uvc_device(const uvc_device_info& info, bool use_memory_map)
            : _name(info.id), 
              _device_path(info.device_path),
              _device_usb_spec(info.conn_spec),
              _info(info),
              _is_capturing(false),
              _is_alive(true),
              _is_started(false),
              _thread(nullptr),
              _named_mtx(nullptr),
              _use_memory_map(use_memory_map),
              _fd(-1),
              _stop_pipe_fd{},
              _buf_dispatch(use_memory_map),
              _frame_drop_monitor(DEFAULT_KPI_FRAME_DROPS_PERCENTAGE)
        {
            _named_mtx = std::unique_ptr<named_mutex>(new named_mutex(_name, 5000));
        }

        v4l_uvc_device::~v4l_uvc_device()
        {
            _is_capturing = false;
            if (_thread && _thread->joinable()) _thread->join();
            for (auto&& fd : _fds)
            {
                try { if (fd) ::close(fd);} catch (...) {}
            }
        }

        void v4l_uvc_device::probe_and_commit(stream_profile profile, frame_callback callback, int buffers)
        {
            if(!_is_capturing && !_callback)
            {
                v4l2_fmtdesc pixel_format = {};
                pixel_format.type = _dev.buf_type;

                while (ioctl(_fd, VIDIOC_ENUM_FMT, &pixel_format) == 0)
                {
                    v4l2_frmsizeenum frame_size = {};
                    frame_size.pixel_format = pixel_format.pixelformat;

                    uint32_t fourcc = (const big_endian<int> &)pixel_format.pixelformat;

                    if (pixel_format.pixelformat == 0)
                    {
                        // Microsoft Depth GUIDs for R400 series are not yet recognized
                        // by the Linux kernel, but they do not require a patch, since there
                        // are "backup" Z16 and Y8 formats in place
                        static const std::set<std::string> pending_formats = {
                            "00000050-0000-0010-8000-00aa003",
                            "00000032-0000-0010-8000-00aa003",
                        };

                        if (std::find(pending_formats.begin(),
                                      pending_formats.end(),
                                      (const char*)pixel_format.description) ==
                            pending_formats.end())
                        {
                            const std::string s(rsutils::string::from() << "!" << pixel_format.description);
                            std::regex rgx("!([0-9a-f]+)-.*");
                            std::smatch match;

                            if (std::regex_search(s.begin(), s.end(), match, rgx))
                            {
                                std::stringstream ss;
                                ss <<  match[1];
                                int id;
                                ss >> std::hex >> id;
                                fourcc = (const big_endian<int> &)id;

                                if (fourcc == profile.format)
                                {
                                    throw linux_backend_exception(rsutils::string::from() << "The requested pixel format '"  << fourcc_to_string(id)
                                                                  << "' is not natively supported by the running Linux kernel and likely requires a patch");
                                }
                            }
                        }
                    }
                    ++pixel_format.index;
                }

                set_format(profile);

                v4l2_streamparm parm = {};
                parm.type = _dev.buf_type;
                if(xioctl(_fd, VIDIOC_G_PARM, &parm) < 0)
                    throw linux_backend_exception("xioctl(VIDIOC_G_PARM) failed");

                parm.parm.capture.timeperframe.numerator = 1;
                parm.parm.capture.timeperframe.denominator = profile.fps;
                if(xioctl(_fd, VIDIOC_S_PARM, &parm) < 0)
                    throw linux_backend_exception("xioctl(VIDIOC_S_PARM) failed");

                // Init memory mapped IO
                negotiate_kernel_buffers(static_cast<size_t>(buffers));
                allocate_io_buffers(static_cast<size_t>(buffers));

                _profile =  profile;
                _callback = callback;
            }
            else
            {
                throw wrong_api_call_sequence_exception("Device already streaming!");
            }
        }

        void v4l_uvc_device::stream_on(std::function<void(const notification& n)> error_handler)
        {
            if(!_is_capturing)
            {
                _error_handler = error_handler;

                // Start capturing
                prepare_capture_buffers();

                // Synchronise stream requests for meta and video data.
                streamon();

                _is_capturing = true;
                _thread = std::unique_ptr<std::thread>(new std::thread([this](){ capture_loop(); }));

                // Starting the video/metadata syncer
                _video_md_syncer.start();
            }
        }

        void v4l_uvc_device::prepare_capture_buffers()
        {
            for (auto&& buf : _buffers) buf->prepare_for_streaming(_fd);
        }

        void v4l_uvc_device::stop_data_capture()
        {
            _is_capturing = false;
            _is_started = false;

            // Stop nn-demand frames polling
            signal_stop();

            _thread->join();
            _thread.reset();

            // Notify kernel
            streamoff();
        }

        void v4l_uvc_device::start_callbacks()
        {
            _is_started = true;
        }

        void v4l_uvc_device::stop_callbacks()
        {
            _is_started = false;
        }

        void v4l_uvc_device::close(stream_profile)
        {
            if(_is_capturing)
            {
                stop_data_capture();
            }

            if (_callback)
            {
                // Release allocated buffers
                allocate_io_buffers(0);

                // Release IO
                negotiate_kernel_buffers(0);

                _callback = nullptr;
            }
        }

        std::string v4l_uvc_device::fourcc_to_string(uint32_t id) const
        {
            uint32_t device_fourcc = id;
            char fourcc_buff[sizeof(device_fourcc)+1];
            std::memcpy( fourcc_buff, &device_fourcc, sizeof( device_fourcc ) );
            fourcc_buff[sizeof(device_fourcc)] = 0;
            return fourcc_buff;
        }

        void v4l_uvc_device::signal_stop()
        {
            _video_md_syncer.stop();;
            char buff[1]={};
            if (write(_stop_pipe_fd[1], buff, 1) < 0)
            {
                 throw linux_backend_exception("Could not signal video capture thread to stop. Error write to pipe.");
            }
        }

        std::string time_in_HH_MM_SS_MMM()
        {
            using namespace std::chrono;

            // get current time
            auto now = system_clock::now();

            // get number of milliseconds for the current second
            // (remainder after division into seconds)
            auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

            // convert to std::time_t in order to convert to std::tm (broken time)
            auto timer = system_clock::to_time_t(now);

            // convert to broken time
            std::tm bt = *std::localtime(&timer);

            std::ostringstream oss;

            oss << std::put_time(&bt, "%H:%M:%S"); // HH:MM:SS
            oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
            return oss.str();
        }

        void v4l_uvc_device::poll()
        {
             fd_set fds{};
             FD_ZERO(&fds);
             for (auto fd : _fds)
             {
                 FD_SET(fd, &fds);
             }

            struct timespec mono_time;
            int ret = clock_gettime(CLOCK_MONOTONIC, &mono_time);
            if (ret) throw linux_backend_exception("could not query time!");

            struct timeval expiration_time = { mono_time.tv_sec + 5, mono_time.tv_nsec / 1000 };
            int val = 0;

            auto realtime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            auto time_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
            LOG_DEBUG_V4L("Select initiated at " << time_in_HH_MM_SS_MMM() << ", mono time " << time_since_epoch << ", host time " << realtime );
            do {
                struct timeval remaining;
                ret = clock_gettime(CLOCK_MONOTONIC, &mono_time);
                if (ret) throw linux_backend_exception("could not query time!");

                struct timeval current_time = { mono_time.tv_sec, mono_time.tv_nsec / 1000 };
                timersub(&expiration_time, &current_time, &remaining);
                // timercmp fails cpp check, reduce macro function from time.h
# define timercmp_lt(a, b) \
  (((a)->tv_sec == (b)->tv_sec) ? ((a)->tv_usec < (b)->tv_usec) : ((a)->tv_sec < (b)->tv_sec))
                if (timercmp_lt(&current_time, &expiration_time)) {
                    val = select(_max_fd + 1, &fds, nullptr, nullptr, &remaining);
                }
                else {
                    val = 0;
                    LOG_DEBUG_V4L("Select timeouted");
                }

                if (val< 0)
                    LOG_DEBUG_V4L("Select interrupted, val = " << val << ", error = " << errno);
            } while (val < 0 && errno == EINTR);

            LOG_DEBUG_V4L("Select done, val = " << val << " at " << time_in_HH_MM_SS_MMM());
            if(val < 0)
            {
                _is_capturing = false;
                _is_started = false;

                // Notify kernel
                streamoff();
            }
            else
            {
                if(val > 0)
                {
                    if(FD_ISSET(_stop_pipe_fd[0], &fds) || FD_ISSET(_stop_pipe_fd[1], &fds))
                    {
                        if(!_is_capturing)
                        {
                            LOG_INFO("V4L stream is closed");
                            return;
                        }
                        else
                        {
                            LOG_ERROR("Stop pipe was signalled during streaming");
                            return;
                        }
                    }
                    else // Check and acquire data buffers from kernel
                    {
                        bool md_extracted = false;
                        bool keep_md = false;
                        bool wa_applied = false;
                        buffers_mgr buf_mgr(_use_memory_map);
                        if (_buf_dispatch.metadata_size())
                        {
                            buf_mgr = _buf_dispatch;    // Handle over MD buffer from the previous cycle
                            md_extracted = true;
                            wa_applied = true;
                            _buf_dispatch.set_md_attributes(0,nullptr);
                        }

                        // Relax the required frame size for compressed formats, i.e. MJPG, Z16H
                        bool compressed_format = val_in_range(_profile.format, { 0x4d4a5047U , 0x5a313648U});

                        // METADATA STREAM
                        // Read metadata. Metadata node performs a blocking call to ensure video and metadata sync
                        acquire_metadata(buf_mgr,fds,compressed_format);
                        md_extracted = true;

                        if (wa_applied)
                        {
                            auto fn = *(uint32_t*)((char*)(buf_mgr.metadata_start())+28);
                            LOG_DEBUG_V4L("Extracting md buff, fn = " << fn);
                        }

                        // VIDEO STREAM
                        if(FD_ISSET(_fd, &fds))
                        {
                            FD_CLR(_fd,&fds);
                            v4l2_buffer buf = {};
                            struct v4l2_plane planes[VIDEO_MAX_PLANES] = {};
                            buf.type = _dev.buf_type;
                            buf.memory = _use_memory_map ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;
                            if (_dev.buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
                                buf.m.planes = planes;
                                buf.length = VIDEO_MAX_PLANES;
                            }
                            if(xioctl(_fd, VIDIOC_DQBUF, &buf) < 0)
                            {
                                LOG_DEBUG_V4L("Dequeued empty buf for fd " << std::dec << _fd);
                            }
                            LOG_DEBUG_V4L("Dequeued buf " << std::dec << buf.index << " for fd " << _fd << " seq " << buf.sequence);
                            buf.type = _dev.buf_type;
                            buf.memory = _use_memory_map ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;
                            if (_dev.buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
                                buf.bytesused = buf.m.planes[0].bytesused;
                            }
                            auto buffer = _buffers[buf.index];
                            buf_mgr.handle_buffer(e_video_buf, _fd, buf, buffer);

                            if (_is_started)
                            {
                                if(buf.bytesused == 0)
                                {
                                    LOG_DEBUG_V4L("Empty video frame arrived, index " << buf.index);
                                    return;
                                }

                                // Drop partial and overflow frames (assumes D4XX metadata only)
                                bool partial_frame = (!compressed_format && (buf.bytesused < buffer->get_full_length() - MAX_META_DATA_SIZE));
                                bool overflow_frame = (buf.bytesused ==  buffer->get_length_frame_only() + MAX_META_DATA_SIZE);
                                if (_dev.buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
                                    /* metadata size is one line of profile, temporary disable validation */
                                    partial_frame = false;
                                    overflow_frame = false;
                                }
                                if (partial_frame || overflow_frame)
                                {
                                    auto percentage = (100 * buf.bytesused) / buffer->get_full_length();
                                    std::stringstream s;
                                    if (partial_frame)
                                    {
                                        s << "Incomplete video frame detected!\nSize " << buf.bytesused
                                            << " out of " << buffer->get_full_length() << " bytes (" << percentage << "%)";
                                        if (overflow_frame)
                                        {
                                            s << ". Overflow detected: payload size " << buffer->get_length_frame_only();
                                            LOG_ERROR("Corrupted UVC frame data, underflow and overflow reported:\n" << s.str().c_str());
                                        }
                                    }
                                    else
                                    {
                                        if (overflow_frame)
                                            s << "overflow video frame detected!\nSize " << buf.bytesused
                                                << ", payload size " << buffer->get_length_frame_only();
                                    }
                                    LOG_DEBUG("Incomplete frame received: " << s.str()); // Ev -try1
                                    bool kpi_violated = _frame_drop_monitor.update_and_check_kpi(_profile, buf.timestamp);
                                    if (kpi_violated)
                                    {
                                        librealsense::notification n = { RS2_NOTIFICATION_CATEGORY_FRAME_CORRUPTED, 0, RS2_LOG_SEVERITY_WARN, s.str() };
                                        _error_handler(n);
                                    }
                                    
                                    // Check if metadata was already allocated
                                    if (buf_mgr.metadata_size())
                                    {
                                        LOG_WARNING("Metadata was present when partial frame arrived, mark md as extracted");
                                        md_extracted = true;
                                        LOG_DEBUG_V4L("Discarding md due to invalid video payload");
                                        auto md_buf = buf_mgr.get_buffers().at(e_metadata_buf);
                                        md_buf._data_buf->request_next_frame(md_buf._file_desc,true);
                                    }
                                }
                                else
                                {
                                    if (!_info.has_metadata_node)
                                    {
                                        if(has_metadata())
                                        {
                                            auto timestamp = (double)buf.timestamp.tv_sec*1000.f + (double)buf.timestamp.tv_usec/1000.f;
                                            timestamp = monotonic_to_realtime(timestamp);

                                            // Read metadata. Metadata node performs a blocking call to ensure video and metadata sync
                                            acquire_metadata(buf_mgr,fds,compressed_format);
                                            md_extracted = true;

                                            if (wa_applied)
                                            {
                                                auto fn = *(uint32_t*)((char*)(buf_mgr.metadata_start())+28);
                                                LOG_DEBUG_V4L("Extracting md buff, fn = " << fn);
                                            }

                                            auto frame_sz = buf_mgr.md_node_present() ? buf.bytesused :
                                                                std::min(buf.bytesused - buf_mgr.metadata_size(), buffer->get_length_frame_only());
                                            frame_object fo{ frame_sz, buf_mgr.metadata_size(),
                                                             buffer->get_frame_start(), buf_mgr.metadata_start(), timestamp };

                                            buffer->attach_buffer(buf);
                                            buf_mgr.handle_buffer(e_video_buf,-1); // transfer new buffer request to the frame callback

                                            if (buf_mgr.verify_vd_md_sync())
                                            {
                                                //Invoke user callback and enqueue next frame
                                                _callback(_profile, fo, [buf_mgr]() mutable {
                                                    buf_mgr.request_next_frame();
                                                });
                                            }
                                            else
                                            {
                                                LOG_WARNING("Video frame dropped, video and metadata buffers inconsistency");
                                            }
                                        }
                                        else // when metadata is not enabled at all, streaming only video
                                        {
                                            auto timestamp = (double)buf.timestamp.tv_sec * 1000.f + (double)buf.timestamp.tv_usec / 1000.f;
                                            timestamp = monotonic_to_realtime(timestamp);

                                            LOG_DEBUG_V4L("no metadata streamed");
                                            if (buf_mgr.verify_vd_md_sync())
                                            {
                                                buffer->attach_buffer(buf);
                                                buf_mgr.handle_buffer(e_video_buf, -1); // transfer new buffer request to the frame callback


                                                auto frame_sz = buf_mgr.md_node_present() ? buf.bytesused :
                                                                    std::min(buf.bytesused - buf_mgr.metadata_size(),
                                                                             buffer->get_length_frame_only());

                                                uint8_t md_size = buf_mgr.metadata_size();
                                                void* md_start = buf_mgr.metadata_start();

                                                // D457 development - hid over uvc - md size for IMU is 64
                                                metadata_hid_raw meta_data{};
                                                if (md_size == 0 && buffer->get_length_frame_only() <= 64)
                                                {
                                                    // Populate HID IMU data - Header
                                                    populate_imu_data(meta_data, buffer->get_frame_start(), md_size, &md_start);
                                                }

                                                frame_object fo{ frame_sz, md_size,
                                                            buffer->get_frame_start(), md_start, timestamp };

                                                //Invoke user callback and enqueue next frame
                                                _callback(_profile, fo, [buf_mgr]() mutable {
                                                    buf_mgr.request_next_frame();
                                                });
                                            }
                                            else
                                            {
                                                LOG_WARNING("Video frame dropped, video and metadata buffers inconsistency");
                                            }
                                        }
                                    }
                                    else
                                    {
                                        // saving video buffer to syncer
                                        _video_md_syncer.push_video({std::make_shared<v4l2_buffer>(buf), _fd, buf.index});
                                        buf_mgr.handle_buffer(e_video_buf, -1);
                                    }
                                }
                            }
                            else
                            {
                                LOG_DEBUG_V4L("Video frame arrived in idle mode."); // TODO - verification
                            }
                        }
                        else
                        {
                            if (_is_started)
                                keep_md = true;
                            LOG_DEBUG("FD_ISSET: no data on video node sink");
                        }

                        // pulling synchronized video and metadata and uploading them to user's callback
                        upload_video_and_metadata_from_syncer(buf_mgr);
                    }
                }
                else // (val==0)
                {
                    LOG_WARNING("Frames didn't arrived within 5 seconds");
                    librealsense::notification n = {RS2_NOTIFICATION_CATEGORY_FRAMES_TIMEOUT, 0, RS2_LOG_SEVERITY_WARN,  "Frames didn't arrived within 5 seconds"};

                    _error_handler(n);
                }
            }
        }

        void v4l_uvc_device::populate_imu_data(metadata_hid_raw& meta_data, uint8_t* frame_start, uint8_t& md_size, void** md_start) const
        {
            meta_data.header.report_type = md_hid_report_type::hid_report_imu;
            meta_data.header.length = hid_header_size + metadata_imu_report_size;
            meta_data.header.timestamp = *(reinterpret_cast<uint64_t *>(frame_start + offsetof(hid_mipi_data, hwTs)));
            // Payload:
            meta_data.report_type.imu_report.header.md_type_id = md_type::META_DATA_HID_IMU_REPORT_ID;
            meta_data.report_type.imu_report.header.md_size = metadata_imu_report_size;

            md_size = sizeof(metadata_hid_raw);
            *md_start = &meta_data;
        }


        void v4l_uvc_device::upload_video_and_metadata_from_syncer(buffers_mgr& buf_mgr)
        {
            // uploading to user's callback
            std::shared_ptr<v4l2_buffer> video_v4l2_buffer;
            std::shared_ptr<v4l2_buffer> md_v4l2_buffer;

            if (_is_started && is_metadata_streamed())
            {
                int video_fd = -1, md_fd = -1;
                if (_video_md_syncer.pull_video_with_metadata(video_v4l2_buffer, md_v4l2_buffer, video_fd, md_fd))
                {
                    // Preparing video buffer
                    auto video_buffer = get_video_buffer(video_v4l2_buffer->index);
                    video_buffer->attach_buffer(*video_v4l2_buffer);

                    // happens when the video did not arrive on
                    // the current polling iteration (was taken from the syncer's video queue)
                    if (buf_mgr.get_buffers()[e_video_buf]._file_desc == -1)
                    {
                        buf_mgr.handle_buffer(e_video_buf, video_fd, *video_v4l2_buffer, video_buffer);
                    }
                    buf_mgr.handle_buffer(e_video_buf, -1); // transfer new buffer request to the frame callback

                    // Preparing metadata buffer
                    auto metadata_buffer = get_md_buffer(md_v4l2_buffer->index);
                    set_metadata_attributes(buf_mgr, md_v4l2_buffer->bytesused, metadata_buffer->get_frame_start());
                    metadata_buffer->attach_buffer(*md_v4l2_buffer);

                    if (buf_mgr.get_buffers()[e_metadata_buf]._file_desc == -1)
                    {
                        buf_mgr.handle_buffer(e_metadata_buf, md_fd, *md_v4l2_buffer, metadata_buffer);
                    }
                    buf_mgr.handle_buffer(e_metadata_buf, -1); // transfer new buffer request to the frame callback

                    auto frame_sz = buf_mgr.md_node_present() ? video_v4l2_buffer->bytesused :
                                        std::min(video_v4l2_buffer->bytesused - buf_mgr.metadata_size(),
                                                 video_buffer->get_length_frame_only());

                    auto timestamp = (double)video_v4l2_buffer->timestamp.tv_sec * 1000.f + (double)video_v4l2_buffer->timestamp.tv_usec / 1000.f;
                    timestamp = monotonic_to_realtime(timestamp);

                    // D457 work - to work with "normal camera", use frame_sz as the first input to the following frame_object:
                    //frame_object fo{ buf.bytesused - MAX_META_DATA_SIZE, buf_mgr.metadata_size(),
                    frame_object fo{ frame_sz, buf_mgr.metadata_size(),
                                     video_buffer->get_frame_start(), buf_mgr.metadata_start(), timestamp };

                    //Invoke user callback and enqueue next frame
                    _callback(_profile, fo, [buf_mgr]() mutable {
                        buf_mgr.request_next_frame();
                    });
                }
                else
                {
                    LOG_DEBUG("video_md_syncer - synchronized video and md could not be pulled");
                }
            }
        }

        void v4l_uvc_device::set_metadata_attributes(buffers_mgr& buf_mgr, __u32 bytesused, uint8_t* md_start)
        {
            size_t uvc_md_start_offset = sizeof(uvc_meta_buffer::ns) + sizeof(uvc_meta_buffer::sof);
            buf_mgr.set_md_attributes(bytesused - uvc_md_start_offset,
                                        md_start + uvc_md_start_offset);
        }

        void v4l_mipi_device::set_metadata_attributes(buffers_mgr& buf_mgr, __u32 bytesused, uint8_t* md_start)
        {
            buf_mgr.set_md_attributes(bytesused, md_start);
        }

        bool v4l_mipi_device::is_platform_jetson() const
        {
            v4l2_capability cap = get_dev_capabilities(_name);

            std::string driver_str = reinterpret_cast<char*>(cap.driver);
            // checking if "tegra" is part of the driver string
            size_t pos = driver_str.find("tegra");
            return pos != std::string::npos;
        }

        void v4l_uvc_device::acquire_metadata(buffers_mgr& buf_mgr,fd_set &, bool compressed_format)
        {
            if (has_metadata())
                buf_mgr.set_md_from_video_node(compressed_format);
            else
                buf_mgr.set_md_attributes(0, nullptr);
        }

        void v4l_uvc_device::set_power_state(power_state state)
        {
            if (state == D0 && _state == D3)
            {
                map_device_descriptor();
            }
            if (state == D3 && _state == D0)
            {
                close(_profile);
                unmap_device_descriptor();
            }
            _state = state;
        }

        bool v4l_uvc_device::set_xu(const extension_unit& xu, uint8_t control, const uint8_t* data, int size)
        {
            uvc_xu_control_query q = {static_cast<uint8_t>(xu.unit), control, UVC_SET_CUR,
                                      static_cast<uint16_t>(size), const_cast<uint8_t *>(data)};
            if(xioctl(_fd, UVCIOC_CTRL_QUERY, &q) < 0)
            {
                if (errno == EIO || errno == EAGAIN || errno == EBUSY)
                    return false;

                throw linux_backend_exception("set_xu(...). xioctl(UVCIOC_CTRL_QUERY) failed");
            }

            return true;
        }
        bool v4l_uvc_device::get_xu(const extension_unit& xu, uint8_t control, uint8_t* data, int size) const
        {
            memset(data, 0, size);
            uvc_xu_control_query q = {static_cast<uint8_t>(xu.unit), control, UVC_GET_CUR,
                                      static_cast<uint16_t>(size), const_cast<uint8_t *>(data)};
            if(xioctl(_fd, UVCIOC_CTRL_QUERY, &q) < 0)
            {
                if (errno == EIO || errno == EAGAIN || errno == EBUSY)
                    return false;

                throw linux_backend_exception("get_xu(...). xioctl(UVCIOC_CTRL_QUERY) failed");
            }

            return true;
        }
        control_range v4l_uvc_device::get_xu_range(const extension_unit& xu, uint8_t control, int len) const
        {
            control_range result{};
            __u16 size = 0;
            //__u32 value = 0;      // all of the real sense extended controls are up to 4 bytes
                                    // checking return value for UVC_GET_LEN and allocating
                                    // appropriately might be better
            //__u8 * data = (__u8 *)&value;
            // MS XU controls are partially supported only
            struct uvc_xu_control_query xquery = {};
            memset(&xquery, 0, sizeof(xquery));
            xquery.query = UVC_GET_LEN;
            xquery.size = 2; // size seems to always be 2 for the LEN query, but
                             //doesn't seem to be documented. Use result for size
                             //in all future queries of the same control number
            xquery.selector = control;
            xquery.unit = xu.unit;
            xquery.data = (__u8 *)&size;

            if(-1 == ioctl(_fd,UVCIOC_CTRL_QUERY,&xquery)){
                throw linux_backend_exception("xioctl(UVC_GET_LEN) failed");
            }

            assert(size<=len);

            std::vector<uint8_t> buf;
            auto buf_size = std::max((size_t)len,sizeof(__u32));
            buf.resize(buf_size);

            xquery.query = UVC_GET_MIN;
            xquery.size = size;
            xquery.selector = control;
            xquery.unit = xu.unit;
            xquery.data = buf.data();
            if(-1 == ioctl(_fd,UVCIOC_CTRL_QUERY,&xquery)){
                throw linux_backend_exception("xioctl(UVC_GET_MIN) failed");
            }
            result.min.resize(buf_size);
            std::copy(buf.begin(), buf.end(), result.min.begin());

            xquery.query = UVC_GET_MAX;
            xquery.size = size;
            xquery.selector = control;
            xquery.unit = xu.unit;
            xquery.data = buf.data();
            if(-1 == ioctl(_fd,UVCIOC_CTRL_QUERY,&xquery)){
                throw linux_backend_exception("xioctl(UVC_GET_MAX) failed");
            }
            result.max.resize(buf_size);
            std::copy(buf.begin(), buf.end(), result.max.begin());

            xquery.query = UVC_GET_DEF;
            xquery.size = size;
            xquery.selector = control;
            xquery.unit = xu.unit;
            xquery.data = buf.data();
            if(-1 == ioctl(_fd,UVCIOC_CTRL_QUERY,&xquery)){
                throw linux_backend_exception("xioctl(UVC_GET_DEF) failed");
            }
            result.def.resize(buf_size);
            std::copy(buf.begin(), buf.end(), result.def.begin());

            xquery.query = UVC_GET_RES;
            xquery.size = size;
            xquery.selector = control;
            xquery.unit = xu.unit;
            xquery.data = buf.data();
            if(-1 == ioctl(_fd,UVCIOC_CTRL_QUERY,&xquery)){
                throw linux_backend_exception("xioctl(UVC_GET_CUR) failed");
            }
            result.step.resize(buf_size);
            std::copy(buf.begin(), buf.end(), result.step.begin());

           return result;
        }

        bool v4l_uvc_device::get_pu(rs2_option opt, int32_t& value) const
        {
            struct v4l2_control control = {get_cid(opt), 0};
            if (xioctl(_fd, VIDIOC_G_CTRL, &control) < 0)
            {
                if (errno == EIO || errno == EAGAIN || errno == EBUSY)
                    return false;

                throw linux_backend_exception("xioctl(VIDIOC_G_CTRL) failed");
            }

            if (RS2_OPTION_ENABLE_AUTO_EXPOSURE==opt)  { control.value = (V4L2_EXPOSURE_MANUAL==control.value) ? 0 : 1; }
            value = control.value;

            return true;
        }

        bool v4l_uvc_device::set_pu(rs2_option opt, int32_t value)
        {
            struct v4l2_control control = {get_cid(opt), value};
            if (RS2_OPTION_ENABLE_AUTO_EXPOSURE==opt) { control.value = value ? V4L2_EXPOSURE_APERTURE_PRIORITY : V4L2_EXPOSURE_MANUAL; }
            
            // We chose not to protect the subscribe / unsubscribe with mutex due to performance reasons,
            // we prefer returning on timeout (and let the retry mechanism try again if exist) than blocking the main thread on every set command


            // RAII to handle unsubscribe in case of exceptions
            std::unique_ptr< uint32_t, std::function< void( uint32_t * ) > > unsubscriber(
                new uint32_t( control.id ),
                [this]( uint32_t * id ) {
                    if (id)
                    {
                        // `unsubscribe_from_ctrl_event()` may throw so we first release the memory allocated and than call it.
                        auto local_id = *id;
                        delete id;
                        unsubscribe_from_ctrl_event( local_id );
                    }
                } );

            subscribe_to_ctrl_event(control.id);

            // Set value
            if (xioctl(_fd, VIDIOC_S_CTRL, &control) < 0)
            {
                if (errno == EIO || errno == EAGAIN || errno == EBUSY)
                    return false;

                throw linux_backend_exception("xioctl(VIDIOC_S_CTRL) failed");
            }

            if (!pend_for_ctrl_status_event())
                return false;

            return true;
        }

        control_range v4l_uvc_device::get_pu_range(rs2_option option) const
        {
            // Auto controls range is trimed to {0,1} range
            if(option >= RS2_OPTION_ENABLE_AUTO_EXPOSURE && option <= RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE)
            {
                static const int32_t min = 0, max = 1, step = 1, def = 1;
                control_range range(min, max, step, def);

                return range;
            }

            struct v4l2_query_ext_ctrl query = {};
            query.id = get_cid(option);
            if (xioctl(_fd, VIDIOC_QUERY_EXT_CTRL, &query) < 0)
            {
                // Some controls (exposure, auto exposure, auto hue) do not seem to work on V4L2
                // Instead of throwing an error, return an empty range. This will cause this control to be omitted on our UI sample.
                // TODO: Figure out what can be done about these options and make this work
                query.minimum = query.maximum = 0;
            }

            control_range range(query.minimum, query.maximum, query.step, query.default_value);

            return range;
        }

        std::vector<stream_profile> v4l_uvc_device::get_profiles() const
        {
            std::vector<stream_profile> results;

            // Retrieve the caps one by one, first get pixel format, then sizes, then
            // frame rates. See http://linuxtv.org/downloads/v4l-dvb-apis for reference.
            v4l2_fmtdesc pixel_format = {};
            pixel_format.type = _dev.buf_type;
            while (ioctl(_fd, VIDIOC_ENUM_FMT, &pixel_format) == 0)
            {
                v4l2_frmsizeenum frame_size = {};
                frame_size.pixel_format = pixel_format.pixelformat;

                uint32_t fourcc = (const big_endian<int> &)pixel_format.pixelformat;

                if (pixel_format.pixelformat == 0)
                {
                    // Microsoft Depth GUIDs for R400 series are not yet recognized
                    // by the Linux kernel, but they do not require a patch, since there
                    // are "backup" Z16 and Y8 formats in place
                    std::set<std::string> known_problematic_formats = {
                        "00000050-0000-0010-8000-00aa003",
                        "00000032-0000-0010-8000-00aa003",
                    };

                    if (std::find(known_problematic_formats.begin(),
                                  known_problematic_formats.end(),
                                  (const char*)pixel_format.description) ==
                        known_problematic_formats.end())
                    {
                        const std::string s(rsutils::string::from() << "!" << pixel_format.description);
                        std::regex rgx("!([0-9a-f]+)-.*");
                        std::smatch match;

                        if (std::regex_search(s.begin(), s.end(), match, rgx))
                        {
                            std::stringstream ss;
                            ss <<  match[1];
                            int id;
                            ss >> std::hex >> id;
                            fourcc = (const big_endian<int> &)id;

                            auto format_str = fourcc_to_string(id);
                            LOG_WARNING("Pixel format " << pixel_format.description << " likely requires patch for fourcc code " << format_str << "!");
                        }
                    }
                }
                else
                {
                    LOG_DEBUG("Recognized pixel-format " << pixel_format.description);
                }

                while (ioctl(_fd, VIDIOC_ENUM_FRAMESIZES, &frame_size) == 0)
                {
                    v4l2_frmivalenum frame_interval = {};
                    frame_interval.pixel_format = pixel_format.pixelformat;
                    frame_interval.width = frame_size.discrete.width;
                    frame_interval.height = frame_size.discrete.height;
                    while (ioctl(_fd, VIDIOC_ENUM_FRAMEINTERVALS, &frame_interval) == 0)
                    {
                        if (frame_interval.type == V4L2_FRMIVAL_TYPE_DISCRETE)
                        {
                            if (frame_interval.discrete.numerator != 0)
                            {
                                auto fps =
                                    static_cast<float>(frame_interval.discrete.denominator) /
                                    static_cast<float>(frame_interval.discrete.numerator);

                                // On D585S, we need to distinguish the occupancy and the label point cloud streams.
                                // The condition currently support 2 resolutions for LPC
                                // This needs to be refactored!
                                if (this->_info.pid == 0X0B6B && frame_size.discrete.width == 2880 && (frame_size.discrete.height == 1040 || frame_size.discrete.height == 260)) // 0x0B6B pid for D585S_PID
                                {
                                    fourcc = 0x50414c38; // PAL8 used instead of GREY in order to distinguish between occupancy and point cloud streams
                                }

                                stream_profile p{};
                                p.format = fourcc;
                                p.width = frame_size.discrete.width;
                                p.height = frame_size.discrete.height;
                                p.fps = fps;
                                if (fourcc != 0) results.push_back(p);
                            }
                        }

                        ++frame_interval.index;
                    }

                     ++frame_size.index;
                }

                ++pixel_format.index;
            }
            return results;
        }

        void v4l_uvc_device::lock() const
        {
            _named_mtx->lock();
        }
        void v4l_uvc_device::unlock() const
        {
            _named_mtx->unlock();
        }

        uint32_t v4l_uvc_device::get_cid(rs2_option option) const
        {
            switch(option)
            {
            case RS2_OPTION_BACKLIGHT_COMPENSATION: return V4L2_CID_BACKLIGHT_COMPENSATION;
            case RS2_OPTION_BRIGHTNESS: return V4L2_CID_BRIGHTNESS;
            case RS2_OPTION_CONTRAST: return V4L2_CID_CONTRAST;
            case RS2_OPTION_EXPOSURE: return V4L2_CID_EXPOSURE_ABSOLUTE; // Is this actually valid? I'm getting a lot of VIDIOC error 22s...
            case RS2_OPTION_GAIN: return V4L2_CID_GAIN;
            case RS2_OPTION_GAMMA: return V4L2_CID_GAMMA;
            case RS2_OPTION_HUE: return V4L2_CID_HUE;
            case RS2_OPTION_SATURATION: return V4L2_CID_SATURATION;
            case RS2_OPTION_SHARPNESS: return V4L2_CID_SHARPNESS;
            case RS2_OPTION_WHITE_BALANCE: return V4L2_CID_WHITE_BALANCE_TEMPERATURE;
            case RS2_OPTION_ENABLE_AUTO_EXPOSURE: return V4L2_CID_EXPOSURE_AUTO; // Automatic gain/exposure control
            case RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE: return V4L2_CID_AUTO_WHITE_BALANCE;
            case RS2_OPTION_POWER_LINE_FREQUENCY : return V4L2_CID_POWER_LINE_FREQUENCY;
            case RS2_OPTION_AUTO_EXPOSURE_PRIORITY: return V4L2_CID_EXPOSURE_AUTO_PRIORITY;
            default: throw linux_backend_exception(rsutils::string::from() << "no v4l2 cid for option " << option);
            }
        }

        void v4l_uvc_device::capture_loop()
        {
            try
            {
                while(_is_capturing)
                {
                    poll();
                }
            }
            catch (const std::exception& ex)
            {
                LOG_ERROR(ex.what());

                librealsense::notification n = {RS2_NOTIFICATION_CATEGORY_UNKNOWN_ERROR, 0, RS2_LOG_SEVERITY_ERROR, ex.what()};

                _error_handler(n);
            }
        }

        bool v4l_uvc_device::has_metadata() const
        {
            return !_use_memory_map;
        }

        void v4l_uvc_device::streamon() const
        {
            stream_ctl_on(_fd, _dev.buf_type);
        }

        void v4l_uvc_device::streamoff() const
        {
            stream_off(_fd, _dev.buf_type);
        }

        void v4l_uvc_device::negotiate_kernel_buffers(size_t num) const
        {
            req_io_buff(_fd, num, _name,
                        _use_memory_map ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR,
                        _dev.buf_type);
        }

        void v4l_uvc_device::allocate_io_buffers(size_t buffers)
        {
            if (buffers)
            {
                for(size_t i = 0; i < buffers; ++i)
                {
                    _buffers.push_back(std::make_shared<buffer>(_fd, _dev.buf_type, _use_memory_map, i));
                }
            }
            else
            {
                for(size_t i = 0; i < _buffers.size(); i++)
                {
                    _buffers[i]->detach_buffer();
                }
                _buffers.resize(0);
            }
        }

        void v4l_uvc_device::map_device_descriptor()
        {
            _fd = open(_name.c_str(), O_RDWR | O_NONBLOCK, 0);
            if(_fd < 0)
                throw linux_backend_exception(rsutils::string::from() <<__FUNCTION__ << " Cannot open '" << _name);

            if (pipe(_stop_pipe_fd) < 0)
                throw linux_backend_exception(rsutils::string::from() <<__FUNCTION__ << " Cannot create pipe!");

            if (_fds.size())
                throw linux_backend_exception(rsutils::string::from() <<__FUNCTION__ << " Device descriptor is already allocated");

            _fds.insert(_fds.end(),{_fd,_stop_pipe_fd[0],_stop_pipe_fd[1]});
            _max_fd = *std::max_element(_fds.begin(),_fds.end());

            v4l2_capability cap = {};
            if(xioctl(_fd, VIDIOC_QUERYCAP, &cap) < 0)
            {
                if(errno == EINVAL)
                    throw linux_backend_exception(_name + " is not V4L2 device");
                else
                    throw linux_backend_exception("xioctl(VIDIOC_QUERYCAP) failed");
            }
            if(!(cap.capabilities & (V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_CAPTURE)))
                throw linux_backend_exception(_name + " is no video capture device");

            if(!(cap.capabilities & V4L2_CAP_STREAMING))
                throw linux_backend_exception(_name + " does not support streaming I/O");
            _info.uvc_capabilities = cap.capabilities;
            _dev.cap = cap;
            /* supporting only one plane for IPU6 */
            _dev.num_planes = 1;
            if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
                _dev.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            } else if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) {
                _dev.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            } else {
                throw linux_backend_exception(_name + " Buffer type is unknown!");
            }
            // Select video input, video standard and tune here.
            v4l2_cropcap cropcap = {};
            cropcap.type = _dev.buf_type;
            if(xioctl(_fd, VIDIOC_CROPCAP, &cropcap) == 0)
            {
                v4l2_crop crop = {};
                crop.type = _dev.buf_type;
                crop.c = cropcap.defrect; // reset to default
                if(xioctl(_fd, VIDIOC_S_CROP, &crop) < 0)
                {
                    switch (errno)
                    {
                    case EINVAL: break; // Cropping not supported
                    default: break; // Errors ignored
                    }
                }
                _dev.cropcap = cropcap;
            } else {} // Errors ignored
        }

        void v4l_uvc_device::unmap_device_descriptor()
        {
            if(::close(_fd) < 0)
                throw linux_backend_exception("v4l_uvc_device: close(_fd) failed");

            if(::close(_stop_pipe_fd[0]) < 0)
               throw linux_backend_exception("v4l_uvc_device: close(_stop_pipe_fd[0]) failed");
            if(::close(_stop_pipe_fd[1]) < 0)
               throw linux_backend_exception("v4l_uvc_device: close(_stop_pipe_fd[1]) failed");

            _fd = 0;
            _stop_pipe_fd[0] = _stop_pipe_fd[1] = 0;
            _fds.clear();
        }

        void v4l_uvc_device::set_format(stream_profile profile)
        {
            v4l2_format fmt = {};
            fmt.type = _dev.buf_type;
            if (_dev.buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
                fmt.fmt.pix_mp.width       = profile.width;
                fmt.fmt.pix_mp.height      = profile.height;
                fmt.fmt.pix_mp.pixelformat = (const big_endian<int> &)profile.format;
                fmt.fmt.pix_mp.field       = V4L2_FIELD_NONE;
                fmt.fmt.pix_mp.num_planes = _dev.num_planes;
                fmt.fmt.pix_mp.flags = 0;

                for (int i = 0; i < fmt.fmt.pix_mp.num_planes; i++) {
                    fmt.fmt.pix_mp.plane_fmt[i].bytesperline = 0;
                    fmt.fmt.pix_mp.plane_fmt[i].sizeimage = 0;
                }
            } else {
                fmt.fmt.pix.width       = profile.width;
                fmt.fmt.pix.height      = profile.height;
                fmt.fmt.pix.pixelformat = (const big_endian<int> &)profile.format;
                fmt.fmt.pix.field       = V4L2_FIELD_NONE;
            }
            if(xioctl(_fd, VIDIOC_S_FMT, &fmt) < 0)
            {
                throw linux_backend_exception(rsutils::string::from() << "xioctl(VIDIOC_S_FMT) failed, errno=" << errno);
            }
            else
                LOG_INFO("Video node was successfully configured to " << fourcc_to_string(fmt.fmt.pix.pixelformat) << " format" <<", fd " << std::dec << _fd);

            LOG_INFO("Trying to configure fourcc " << fourcc_to_string(fmt.fmt.pix.pixelformat));
        }

        void v4l_uvc_device::subscribe_to_ctrl_event( uint32_t control_id )
        {
            struct v4l2_event_subscription event_subscription ;
            event_subscription.flags = V4L2_EVENT_SUB_FL_ALLOW_FEEDBACK;
            event_subscription.type =  V4L2_EVENT_CTRL;
            event_subscription.id = control_id;
            memset(event_subscription.reserved,0, sizeof(event_subscription.reserved));
            if  (xioctl(_fd, VIDIOC_SUBSCRIBE_EVENT, &event_subscription) < 0)
            {
                throw linux_backend_exception(rsutils::string::from() << "xioctl(VIDIOC_SUBSCRIBE_EVENT) with control_id = " << control_id << " failed");
            }
        }

        void v4l_uvc_device::unsubscribe_from_ctrl_event( uint32_t control_id )
        {
            struct v4l2_event_subscription event_subscription ;
            event_subscription.flags = V4L2_EVENT_SUB_FL_ALLOW_FEEDBACK;
            event_subscription.type =  V4L2_EVENT_CTRL;
            event_subscription.id = control_id;
            memset(event_subscription.reserved,0, sizeof(event_subscription.reserved));
            if  (xioctl(_fd, VIDIOC_UNSUBSCRIBE_EVENT, &event_subscription) < 0)
            {
                throw linux_backend_exception(rsutils::string::from() << "xioctl(VIDIOC_UNSUBSCRIBE_EVENT) with control_id = " << control_id << " failed");
            }
        }


        bool v4l_uvc_device::pend_for_ctrl_status_event()
        {
            struct v4l2_event event;
            memset(&event, 0 , sizeof(event));

            // Poll registered events and verify that set control event raised (wait max of 10 * 2 = 20 [ms])
            static int MAX_POLL_RETRIES = 10;
            for ( int i = 0 ; i < MAX_POLL_RETRIES && event.type != V4L2_EVENT_CTRL ; i++)
            {
                if(xioctl(_fd, VIDIOC_DQEVENT, &event) < 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            }

            return event.type == V4L2_EVENT_CTRL;
        }

        v4l_uvc_meta_device::v4l_uvc_meta_device(const uvc_device_info& info, bool use_memory_map):
            v4l_uvc_device(info,use_memory_map),
            _md_fd(0),
            _md_name(info.metadata_node_id)
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

            _md_fd = open(_md_name.c_str(), O_RDWR | O_NONBLOCK, 0);
            if(_md_fd < 0)
            {
                return;  // Does not throw, MIPI device metadata not received through UVC, no metadata here may be valid
            }

            //The minimal video/metadata nodes syncer will be implemented by using two blocking calls:
            // 1. Obtain video node data.
            // 2. Obtain metadata
            //     To revert to multiplexing mode uncomment the next line
            _fds.push_back(_md_fd);
            _max_fd = *std::max_element(_fds.begin(),_fds.end());

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

        v4l_mipi_device::v4l_mipi_device(const uvc_device_info& info, bool use_memory_map):
            v4l_uvc_meta_device(info,use_memory_map)
        {}

        v4l_mipi_device::~v4l_mipi_device()
        {}

        // D457 controls map- temporal solution to bypass backend interface with actual codes
        // DS5 depth XU identifiers
        const uint8_t RS_HWMONITOR                       = 1;
        const uint8_t RS_DEPTH_EMITTER_ENABLED           = 2;
        const uint8_t RS_EXPOSURE                        = 3;
        const uint8_t RS_LASER_POWER                     = 4;
        const uint8_t RS_HARDWARE_PRESET                 = 6;
        const uint8_t RS_ERROR_REPORTING                 = 7;
        const uint8_t RS_EXT_TRIGGER                     = 8;
        const uint8_t RS_ASIC_AND_PROJECTOR_TEMPERATURES = 9;
        const uint8_t RS_ENABLE_AUTO_WHITE_BALANCE       = 0xA;
        const uint8_t RS_ENABLE_AUTO_EXPOSURE            = 0xB;
        const uint8_t RS_LED_PWR                         = 0xE;
        const uint8_t RS_EMITTER_FREQUENCY               = 0x10; // Match to DS5_EMITTER_FREQUENCY


        bool v4l_mipi_device::get_pu(rs2_option opt, int32_t& value) const
        {
            v4l2_ext_control control{get_cid(opt), 0, 0, 0};
            // Extract the control group from the underlying control query
            v4l2_ext_controls ctrls_block { control.id&0xffff0000, 1, 0, 0, 0, &control};

            if (xioctl(_fd, VIDIOC_G_EXT_CTRLS, &ctrls_block) < 0)
            {
                if (errno == EIO || errno == EAGAIN) // TODO: Log?
                    return false;

                throw linux_backend_exception("xioctl(VIDIOC_G_EXT_CTRLS) failed");
            }

            if (opt == RS2_OPTION_ENABLE_AUTO_EXPOSURE)
                control.value = (V4L2_EXPOSURE_MANUAL==control.value) ? 0 : 1;
            value = control.value;

            return true;
        }

        bool v4l_mipi_device::set_pu(rs2_option opt, int32_t value)
        {
            v4l2_ext_control control{get_cid(opt), 0, 0, value};
            if (opt == RS2_OPTION_ENABLE_AUTO_EXPOSURE)
                control.value = value ? V4L2_EXPOSURE_APERTURE_PRIORITY : V4L2_EXPOSURE_MANUAL;

            // Extract the control group from the underlying control query
            v4l2_ext_controls ctrls_block{ control.id & 0xffff0000, 1, 0, 0, 0, &control };
            if (xioctl(_fd, VIDIOC_S_EXT_CTRLS, &ctrls_block) < 0)
            {
                if (errno == EIO || errno == EAGAIN) // TODO: Log?
                    return false;

                throw linux_backend_exception("xioctl(VIDIOC_S_EXT_CTRLS) failed");
            }

            return true;
        }

        bool v4l_mipi_device::set_xu(const extension_unit& xu, uint8_t control, const uint8_t* data, int size)
        {
            v4l2_ext_control xctrl{xu_to_cid(xu,control), uint32_t(size), 0, 0};
            switch (size)
            {
                case 1: xctrl.value   = *(reinterpret_cast<const uint8_t*>(data)); break;
                case 2: xctrl.value   = *reinterpret_cast<const uint16_t*>(data); break; // TODO check signed/unsigned
                case 4: xctrl.value   = *reinterpret_cast<const int32_t*>(data); break;
                case 8: xctrl.value64 = *reinterpret_cast<const int64_t*>(data); break;
                default:
                    xctrl.p_u8 = const_cast<uint8_t*>(data); // TODO aggregate initialization with union
            }

            if (control == RS_ENABLE_AUTO_EXPOSURE)
                xctrl.value = xctrl.value ? V4L2_EXPOSURE_APERTURE_PRIORITY : V4L2_EXPOSURE_MANUAL;

            // Extract the control group from the underlying control query
            v4l2_ext_controls ctrls_block { xctrl.id&0xffff0000, 1, 0, 0, 0, &xctrl };

            int retVal = xioctl(_fd, VIDIOC_S_EXT_CTRLS, &ctrls_block);
            if (retVal < 0)
            {
                if (errno == EIO || errno == EAGAIN) // TODO: Log?
                    return false;

                throw linux_backend_exception("xioctl(VIDIOC_S_EXT_CTRLS) failed");
            }
            return true;
        }

        bool v4l_mipi_device::get_xu(const extension_unit& xu, uint8_t control, uint8_t* data, int size) const
        {
            v4l2_ext_control xctrl{xu_to_cid(xu,control), uint32_t(size), 0, 0};
            xctrl.p_u8 = data;

            v4l2_ext_controls ext {xctrl.id & 0xffff0000, 1, 0, 0, 0, &xctrl};

            // the ioctl fails once when performing send and receive right after it
            // it succeeds on the second time
            int tries = 2;
            while(tries--)
            {
                int ret = xioctl(_fd, VIDIOC_G_EXT_CTRLS, &ext);
                if (ret < 0)
                {
                    // exception is thrown if the ioctl fails twice
                    continue;
                }

                if (control == RS_ENABLE_AUTO_EXPOSURE)
                  xctrl.value = (V4L2_EXPOSURE_MANUAL == xctrl.value) ? 0 : 1;

                // used to parse the data when only a value is returned (e.g. laser power),
                // and not a pointer to a buffer of data (e.g. gvd)
                if (size < sizeof(__s64))
                    memcpy(data,(void*)(&xctrl.value), size);

                return true;
            }

            // sending error on ioctl failure
            if (errno == EIO || errno == EAGAIN) // TODO: Log?
                return false;
            throw linux_backend_exception("xioctl(VIDIOC_G_EXT_CTRLS) failed");
        }

        control_range v4l_mipi_device::get_xu_range(const extension_unit& xu, uint8_t control, int len) const
        {
            v4l2_query_ext_ctrl xctrl_query{};
            xctrl_query.id = xu_to_cid(xu,control);

            if(0 > ioctl(_fd,VIDIOC_QUERY_EXT_CTRL,&xctrl_query)){
                throw linux_backend_exception(rsutils::string::from() << "xioctl(VIDIOC_QUERY_EXT_CTRL) failed, errno=" << errno);
            }

            if ((xctrl_query.elems !=1 ) ||
                (xctrl_query.minimum < std::numeric_limits<int32_t>::min()) ||
                (xctrl_query.maximum > std::numeric_limits<int32_t>::max()))
                throw linux_backend_exception(rsutils::string::from() << "Mipi Control range for " << xctrl_query.name
                    << " is not compliant with backend interface: [min,max,default,step]:\n"
                    << xctrl_query.minimum << ", " << xctrl_query.maximum << ", "
                    << xctrl_query.default_value << ", " << xctrl_query.step
                    << "\n Elements = " << xctrl_query.elems);

            if (control == RS_ENABLE_AUTO_EXPOSURE)
                return {0, 1, 1, 1};
            return { static_cast<int32_t>(xctrl_query.minimum), static_cast<int32_t>(xctrl_query.maximum),
                     static_cast<int32_t>(xctrl_query.step), static_cast<int32_t>(xctrl_query.default_value)};
        }

        control_range v4l_mipi_device::get_pu_range(rs2_option option) const
        {
            // Auto controls range is trimed to {0,1} range
            if(option >= RS2_OPTION_ENABLE_AUTO_EXPOSURE && option <= RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE)
            {
                static const int32_t min = 0, max = 1, step = 1, def = 1;
                control_range range(min, max, step, def);

                return range;
            }

            struct v4l2_query_ext_ctrl query = {};
            query.id = get_cid(option);
            if (xioctl(_fd, VIDIOC_QUERY_EXT_CTRL, &query) < 0)
            {
                // Some controls (exposure, auto exposure, auto hue) do not seem to work on V4L2
                // Instead of throwing an error, return an empty range. This will cause this control to be omitted on our UI sample.
                // TODO: Figure out what can be done about these options and make this work
                query.minimum = query.maximum = 0;
            }

            control_range range(query.minimum, query.maximum, query.step, query.default_value);

            return range;
        }

        uint32_t v4l_mipi_device::get_cid(rs2_option option) const
        {
            switch(option)
            {
                case RS2_OPTION_BACKLIGHT_COMPENSATION: return V4L2_CID_BACKLIGHT_COMPENSATION;
                case RS2_OPTION_BRIGHTNESS: return V4L2_CID_BRIGHTNESS;
                case RS2_OPTION_CONTRAST: return V4L2_CID_CONTRAST;
                case RS2_OPTION_EXPOSURE: return V4L2_CID_EXPOSURE_ABSOLUTE; // Is this actually valid? I'm getting a lot of VIDIOC error 22s...
                case RS2_OPTION_GAIN: return V4L2_CTRL_CLASS_IMAGE_SOURCE | 0x903; // v4l2-ctl --list-ctrls -d /dev/video0
                case RS2_OPTION_GAMMA: return V4L2_CID_GAMMA;
                case RS2_OPTION_HUE: return V4L2_CID_HUE;
                case RS2_OPTION_LASER_POWER: return V4L2_CID_EXPOSURE_ABSOLUTE;
                case RS2_OPTION_EMITTER_ENABLED: return V4L2_CID_EXPOSURE_AUTO;
//            case RS2_OPTION_SATURATION: return V4L2_CID_SATURATION;
//            case RS2_OPTION_SHARPNESS: return V4L2_CID_SHARPNESS;
//            case RS2_OPTION_WHITE_BALANCE: return V4L2_CID_WHITE_BALANCE_TEMPERATURE;
                case RS2_OPTION_ENABLE_AUTO_EXPOSURE: return V4L2_CID_EXPOSURE_AUTO; // Automatic gain/exposure control
//            case RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE: return V4L2_CID_AUTO_WHITE_BALANCE;
//            case RS2_OPTION_POWER_LINE_FREQUENCY : return V4L2_CID_POWER_LINE_FREQUENCY;
//            case RS2_OPTION_AUTO_EXPOSURE_PRIORITY: return V4L2_CID_EXPOSURE_AUTO_PRIORITY;
                default: throw linux_backend_exception(rsutils::string::from() << "no v4l2 mipi mapping cid for option " << option);
            }
        }

        // D457 controls map - temporal solution to bypass backend interface with actual codes
        uint32_t v4l_mipi_device::xu_to_cid(const extension_unit& xu, uint8_t control) const
        {
            if (0==xu.subdevice)
            {
                switch(control)
                {
                    case RS_HWMONITOR: return RS_CAMERA_CID_HWMC;
                    case RS_DEPTH_EMITTER_ENABLED: return RS_CAMERA_CID_LASER_POWER;
                    case RS_EXPOSURE: return V4L2_CID_EXPOSURE_ABSOLUTE;//RS_CAMERA_CID_MANUAL_EXPOSURE; V4L2_CID_EXPOSURE_ABSOLUTE
                    case RS_LASER_POWER: return RS_CAMERA_CID_MANUAL_LASER_POWER;
                    case RS_ENABLE_AUTO_WHITE_BALANCE : return RS_CAMERA_CID_WHITE_BALANCE_MODE;
                    case RS_ENABLE_AUTO_EXPOSURE: return V4L2_CID_EXPOSURE_AUTO; //RS_CAMERA_CID_EXPOSURE_MODE;
                    case RS_HARDWARE_PRESET : return RS_CAMERA_CID_PRESET;
                    case RS_EMITTER_FREQUENCY : return RS_CAMERA_CID_EMITTER_FREQUENCY;
                    // D457 Missing functionality
                    //case RS_ERROR_REPORTING: TBD;
                    //case RS_EXT_TRIGGER: TBD;
                    //case RS_ASIC_AND_PROJECTOR_TEMPERATURES: TBD;
                    //case RS_LED_PWR: TBD;

                    default: throw linux_backend_exception(rsutils::string::from() << "no v4l2 mipi cid for XU depth control " << std::dec << int(control));
                }
            }
            else
                throw linux_backend_exception(rsutils::string::from() << "MIPI Controls mapping is for Depth XU only, requested for subdevice " << xu.subdevice);
        }

        std::shared_ptr<uvc_device> v4l_backend::create_uvc_device(uvc_device_info info) const
        {
            bool mipi_device = (mipi_devices_pid.count(info.pid) > 0);

            auto v4l_uvc_dev =        mipi_device ?         std::make_shared<v4l_mipi_device>(info) :
                              ((!info.has_metadata_node) ?  std::make_shared<v4l_uvc_device>(info) :
                                                            std::make_shared<v4l_uvc_meta_device>(info));

            return std::make_shared<platform::retry_controls_work_around>(v4l_uvc_dev);
        }

        std::vector<uvc_device_info> v4l_backend::query_uvc_devices() const
        {
            std::vector<uvc_device_info> uvc_nodes;

            v4l_uvc_device::foreach_uvc_device(
            [&uvc_nodes](const uvc_device_info& i, const std::string&)
            {
                uvc_nodes.push_back(i);
            });

            return uvc_nodes;
        }

        std::shared_ptr<command_transfer> v4l_backend::create_usb_device(usb_device_info info) const
        {
            auto dev = usb_enumerator::create_usb_device(info);
             if(dev)
                 return std::make_shared<platform::command_transfer_usb>(dev);
             return nullptr;
        }

        std::vector<usb_device_info> v4l_backend::query_usb_devices() const
        {
            auto device_infos = usb_enumerator::query_devices_info();
            return device_infos;
        }

        std::vector<mipi_device_info> v4l_backend::query_mipi_devices() const
        {
            std::vector<mipi_device_info> mipi_nodes;

            v4l_mipi_device::foreach_mipi_device(
            [&mipi_nodes](const mipi_device_info& i, const std::string&)
            {
                mipi_nodes.push_back(i);
            });

            return mipi_nodes;
        }
        std::shared_ptr<hid_device> v4l_backend::create_hid_device(hid_device_info info) const
        {
            return std::make_shared<v4l_hid_device>(info);
        }

        std::vector<hid_device_info> v4l_backend::query_hid_devices() const
        {
            std::vector<hid_device_info> results;
            v4l_hid_device::foreach_hid_device([&](const hid_device_info& hid_dev_info){
                results.push_back(hid_dev_info);
            });
            return results;
        }

        std::shared_ptr<device_watcher> v4l_backend::create_device_watcher() const
        {
#if defined(USING_UDEV)
            return std::make_shared< udev_device_watcher >( this );
#else
            return std::make_shared< polling_device_watcher >( this );
#endif
        }

        std::shared_ptr<backend> create_backend()
        {
            return std::make_shared<v4l_backend>();
        }

        void v4l2_video_md_syncer::push_video(const sync_buffer& video_buffer)
        {
            std::lock_guard<std::mutex> lock(_syncer_mutex);
            if(!_is_ready)
            {
                LOG_DEBUG_V4L("video_md_syncer - push_video called but syncer not ready");
                return;
            }
            _video_queue.push(video_buffer);
            LOG_DEBUG_V4L("video_md_syncer - video pushed with sequence " << video_buffer._v4l2_buf->sequence << ", buf " << video_buffer._buffer_index);

            // remove old video_buffer
            if (_video_queue.size() > 2)
            {
                // Enqueue of video buffer before throwing its content away
                enqueue_front_buffer_before_throwing_it(_video_queue);
            }
        }

        void v4l2_video_md_syncer::push_metadata(const sync_buffer& md_buffer)
        {
            std::lock_guard<std::mutex> lock(_syncer_mutex);
            if(!_is_ready)
            {
                LOG_DEBUG_V4L("video_md_syncer - push_metadata called but syncer not ready");
                return;
            }
            // override front buffer if it has the same sequence that the new buffer - happens with metadata sequence 0
            if (_md_queue.size() > 0 && _md_queue.front()._v4l2_buf->sequence == md_buffer._v4l2_buf->sequence)
            {
                LOG_DEBUG_V4L("video_md_syncer - calling enqueue_front_buffer_before_throwing_it - md buf " << md_buffer._buffer_index << " and md buf " << _md_queue.front()._buffer_index << " have same sequence");
                enqueue_front_buffer_before_throwing_it(_md_queue);
            }
            _md_queue.push(md_buffer);
            LOG_DEBUG_V4L("video_md_syncer - md pushed with sequence " << md_buffer._v4l2_buf->sequence << ", buf " << md_buffer._buffer_index);
            LOG_DEBUG_V4L("video_md_syncer - md queue size = " << _md_queue.size());

            // remove old md_buffer
            if (_md_queue.size() > 2)
            {
                LOG_DEBUG_V4L("video_md_syncer - calling enqueue_front_buffer_before_throwing_it - md queue size is: " << _md_queue.size());
                // Enqueue of md buffer before throwing its content away
                enqueue_front_buffer_before_throwing_it(_md_queue);
            }
        }

        bool v4l2_video_md_syncer::pull_video_with_metadata(std::shared_ptr<v4l2_buffer>& video_buffer, std::shared_ptr<v4l2_buffer>& md_buffer,
                                                            int& video_fd, int& md_fd)
        {
            std::lock_guard<std::mutex> lock(_syncer_mutex);
            if(!_is_ready)
            {
                LOG_DEBUG_V4L("video_md_syncer - pull_video_with_metadata called but syncer not ready");
                return false;
            }
            if (_video_queue.empty())
            {
                LOG_DEBUG_V4L("video_md_syncer - video queue is empty");
                return false;
            }

            if (_md_queue.empty())
            {
                LOG_DEBUG_V4L("video_md_syncer - md queue is empty");
                return false;
            }

            sync_buffer video_candidate = _video_queue.front();
            sync_buffer md_candidate = _md_queue.front();

            // set video and md file descriptors
            video_fd = video_candidate._fd;
            md_fd = md_candidate._fd;

            // sync is ok if latest video and md have the same sequence
            if (video_candidate._v4l2_buf->sequence == md_candidate._v4l2_buf->sequence)
            {
                video_buffer = video_candidate._v4l2_buf;
                md_buffer = md_candidate._v4l2_buf;
                // removing from queues
                _video_queue.pop();
                _md_queue.pop();
                LOG_DEBUG_V4L("video_md_syncer - video and md pulled with sequence " << video_candidate._v4l2_buf->sequence);
                return true;
            }

            LOG_DEBUG_V4L("video_md_syncer - video_candidate seq " << video_candidate._v4l2_buf->sequence << ", md_candidate seq " << md_candidate._v4l2_buf->sequence);

            if (video_candidate._v4l2_buf->sequence > md_candidate._v4l2_buf->sequence && _md_queue.size() > 1)
            {
                // Enqueue of md buffer before throwing its content away
                enqueue_buffer_before_throwing_it(md_candidate);
                _md_queue.pop();

                // checking remaining metadata buffer in queue
                auto alternative_md_candidate = _md_queue.front();
                // sync is ok if latest video and md have the same sequence
                if (video_candidate._v4l2_buf->sequence == alternative_md_candidate._v4l2_buf->sequence)
                {
                    video_buffer = video_candidate._v4l2_buf;
                    md_buffer = alternative_md_candidate._v4l2_buf;
                    // removing from queues
                    _video_queue.pop();
                    _md_queue.pop();
                    LOG_DEBUG_V4L("video_md_syncer - video and md pulled with sequence " << video_candidate._v4l2_buf->sequence);
                    return true;
                }
            }
            if (video_candidate._v4l2_buf->sequence < md_candidate._v4l2_buf->sequence && _video_queue.size() > 1)
            {
                // Enqueue of md buffer before throwing its content away
                enqueue_buffer_before_throwing_it(video_candidate);
                _video_queue.pop();

                // checking remaining video buffer in queue
                auto alternative_video_candidate = _video_queue.front();
                // sync is ok if latest video and md have the same sequence
                if (alternative_video_candidate._v4l2_buf->sequence == md_candidate._v4l2_buf->sequence)
                {
                    video_buffer = alternative_video_candidate._v4l2_buf;
                    md_buffer = md_candidate._v4l2_buf;
                    // removing from queues
                    _video_queue.pop();
                    _md_queue.pop();
                    LOG_DEBUG_V4L("video_md_syncer - video and md pulled with sequence " << md_candidate._v4l2_buf->sequence);
                    return true;
                }
            }
            return false;
        }

        void v4l2_video_md_syncer::enqueue_buffer_before_throwing_it(const sync_buffer& sb) const
        {
            // Enqueue of buffer before throwing its content away
            LOG_DEBUG_V4L("video_md_syncer - Enqueue buf " << std::dec << sb._buffer_index << " for fd " << sb._fd << " before dropping it");
            if (xioctl(sb._fd, VIDIOC_QBUF, sb._v4l2_buf.get()) < 0)
            {
                LOG_ERROR("xioctl(VIDIOC_QBUF) failed when requesting new frame! fd: " << sb._fd << " error: " << strerror(errno));
            }
        }

        void v4l2_video_md_syncer::enqueue_front_buffer_before_throwing_it(std::queue<sync_buffer>& sync_queue)
        {
            // Enqueue of buffer before throwing its content away
            LOG_DEBUG_V4L("video_md_syncer - Enqueue buf " << std::dec << sync_queue.front()._buffer_index << " for fd " << sync_queue.front()._fd << " before dropping it");
            if (xioctl(sync_queue.front()._fd, VIDIOC_QBUF, sync_queue.front()._v4l2_buf.get()) < 0)
            {
                LOG_ERROR("xioctl(VIDIOC_QBUF) failed when requesting new frame! fd: " << sync_queue.front()._fd << " error: " << strerror(errno));
            }
            sync_queue.pop();
        }


        void v4l2_video_md_syncer::stop()
        {
             _is_ready = false;
             flush_queues();
        }

        void v4l2_video_md_syncer::flush_queues()
        {
            // Empty queues
            LOG_DEBUG_V4L("video_md_syncer - flush video and md queues");
            std::lock_guard<std::mutex> lock(_syncer_mutex);
            while(!_video_queue.empty())
            {
                _video_queue.pop();
            }
            while(!_md_queue.empty())
            {
                _md_queue.pop();
            }
            LOG_DEBUG_V4L("video_md_syncer - flush video and md queues done - mq_q size = " << _md_queue.size() << ", video_q size = " << _video_queue.size());
        }
    }
}
