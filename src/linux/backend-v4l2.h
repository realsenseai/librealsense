// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015-2024 RealSense, Inc. All Rights Reserved.

#pragma once

#include "backend.h"
#include <src/platform/uvc-device.h>
#include <src/metadata.h>
#include "types.h"

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <algorithm>
#include <array>
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

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <cmath>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/usb/video.h>
#include <linux/uvcvideo.h>
#include <linux/videodev2.h>
#include <regex>
#include <list>

// Metadata streaming nodes are available with kernels 4.16+
#ifdef V4L2_META_FMT_UVC
constexpr bool metadata_node = true;
#else
#pragma message ( "\nLibrealsense notification: V4L2_META_FMT_UVC was not defined, adding metadata constructs")

constexpr bool metadata_node = false;

// Providing missing parts from videodev2.h
// V4L2_META_FMT_UVC >> V4L2_CAP_META_CAPTURE is also defined, but the opposite does not hold
#define V4L2_META_FMT_UVC    v4l2_fourcc('U', 'V', 'C', 'H') /* UVC Payload Header */

#ifndef V4L2_CAP_META_CAPTURE
#define V4L2_CAP_META_CAPTURE    0x00800000  /* Specified in kernel header v4.16 */
#endif // V4L2_CAP_META_CAPTURE

#endif // V4L2_META_FMT_UVC

#ifndef V4L2_META_FMT_D4XX
#define V4L2_META_FMT_D4XX      v4l2_fourcc('D', '4', 'X', 'X') /* D400 Payload Header metadata */
#endif

#undef DEBUG_V4L
#ifdef DEBUG_V4L
#define LOG_DEBUG_V4L(...)   do { CLOG(DEBUG   ,LIBREALSENSE_ELPP_ID) << __VA_ARGS__; } while(false)
#else
#define LOG_DEBUG_V4L(...)
#endif //DEBUG_V4L

// Use local definition of buf type to resolve for kernel versions
constexpr auto LOCAL_V4L2_BUF_TYPE_META_CAPTURE = (v4l2_buf_type)(13);

#pragma pack(push, 1)
// The struct definition is identical to uvc_meta_buf defined uvcvideo.h/ kernel 4.16 headers, and is provided to allow for cross-kernel compilation
struct uvc_meta_buffer {
    __u64 ns;               // system timestamp of the payload in nanoseconds
    __u16 sof;              // USB Frame Number
    __u8 length;            // length of the payload metadata header
    __u8 flags;             // payload header flags
    __u8* buf;              //device-specific metadata payload data
};
#pragma pack(pop)


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
            std::string _device_path;
            uint32_t _timeout;
            int _fildes;
            std::atomic< int > _lock_counter;
        };
        static int xioctl(int fh, unsigned long request, void *arg);

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

        class v4l_uvc_interface
        {
            virtual void capture_loop() = 0;

            virtual bool has_metadata() const = 0;

            virtual void streamon() const = 0;
            virtual void streamoff() const = 0;
            virtual void negotiate_kernel_buffers(size_t num) const = 0;

            virtual void allocate_io_buffers(size_t num) = 0;
            virtual void map_device_descriptor() = 0;
            virtual void unmap_device_descriptor() = 0;
            virtual void set_format(stream_profile profile) = 0;
            virtual void prepare_capture_buffers() = 0;
            virtual void stop_data_capture() = 0;
            virtual void acquire_metadata(buffers_mgr & buf_mgr,fd_set &fds, bool compressed_format) = 0;
        };

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

        private:
            void enqueue_buffer_before_throwing_it(const sync_buffer& sb) const;
            void enqueue_front_buffer_before_throwing_it(std::queue<sync_buffer>& sync_queue);
            void flush_queues();

            std::mutex _syncer_mutex;
            std::queue<sync_buffer> _video_queue;
            std::queue<sync_buffer> _md_queue;
            bool _is_ready;
        };

        // The aim of the frame_drop_monitor is to check the frames drops kpi - which requires
        // that no more than some percentage of the frames are dropped
        // It is checked using the fps, and the previous corrupted frames, on the last 30 seconds
        // for example, for frame rate of 30 fps, and kpi of 5%, the criteria will be:
        // if at least 45 frames (= 30[fps] * 5%[kpi]* 30[sec]) drops have occured in the previous 30 seconds,
        // then the kpi is violated
        class frame_drop_monitor
        {
        public:
            frame_drop_monitor(double kpi_frames_drops_percentage) : _kpi_frames_drops_pct(kpi_frames_drops_percentage) {}
            // update_and_check_kpi method returns whether the kpi has been violated
            // it should be called each time a partial frame is caught
            bool update_and_check_kpi(const stream_profile& profile, const timeval& timestamp); 

        private:
            // container used to store the latest timestamps of the partial frames, per profile
            std::vector<std::pair<stream_profile, std::deque<long int>>> drops_per_stream;
            double _kpi_frames_drops_pct;
        };

        typedef std::pair<uvc_device_info,std::string> node_info;

        class v4l_uvc_device : public uvc_device, public v4l_uvc_interface
        {
        public:
            static void foreach_uvc_device(
                    std::function<void(const uvc_device_info&,
                                       const std::string&)> action);

            static std::vector<std::string> get_mipi_dfu_paths();

            static bool is_usb_path_valid(const std::string& usb_video_path, const std::string &dev_name,
                                          std::string &busnum, std::string &devnum, std::string &devpath);

            static bool is_usb_device_path(const std::string& video_path);

            static uvc_device_info get_info_from_usb_device_path(const std::string& video_path, const std::string& dev_name, const std::string& name, const std::vector<std::pair <std::string, std::string>>& sys_to_dev_video_paths);

            static uvc_device_info get_info_from_mipi_device_path(const std::string& video_path, const std::string& name);

            static bool is_format_supported_on_node(const std::string& dev_name, std::string v4l_4cc_fmt);
            static bool is_device_depth_node(const std::string& dev_name);
            static uint16_t get_mipi_device_pid(const std::string& dev_name);
            static void get_mipi_device_info(const std::string& dev_name,
                                             std::string& bus_info, std::string& card);

            v4l_uvc_device(const uvc_device_info& info, bool use_memory_map = false);

            virtual ~v4l_uvc_device() override;

            void probe_and_commit(stream_profile profile, frame_callback callback, int buffers) override;

            void stream_on(std::function<void(const notification& n)> error_handler) override;

            void start_callbacks() override;

            void stop_callbacks() override;

            void close(stream_profile) override;

            std::string fourcc_to_string(uint32_t id) const;

            void signal_stop();

            void poll();

            void set_power_state(power_state state) override;
            power_state get_power_state() const override { return _state; }

            void init_xu(const extension_unit&) override {}
            bool set_xu(const extension_unit& xu, uint8_t control, const uint8_t* data, int size) override;
            bool get_xu(const extension_unit& xu, uint8_t control, uint8_t* data, int size) const override;
            control_range get_xu_range(const extension_unit& xu, uint8_t control, int len) const override;

            bool get_pu(rs2_option opt, int32_t& value) const override;

            bool set_pu(rs2_option opt, int32_t value) override;

            control_range get_pu_range(rs2_option option) const override;

            std::vector<stream_profile> get_profiles() const override;

            void lock() const override;
            void unlock() const override;

            std::string get_device_location() const override { return _device_path; }
            usb_spec get_usb_specification() const override { return _device_usb_spec; }

            bool is_platform_jetson() const override {return false;}

        protected:
            virtual uint32_t get_cid(rs2_option option) const;

            virtual void capture_loop() override;

            virtual bool has_metadata() const override;

            virtual void streamon() const override;
            virtual void streamoff() const override;
            virtual void negotiate_kernel_buffers(size_t num) const override;

            virtual void allocate_io_buffers(size_t num) override;
            virtual void map_device_descriptor() override;
            virtual void unmap_device_descriptor() override;
            virtual void set_format(stream_profile profile) override;
            virtual void prepare_capture_buffers() override;
            virtual void stop_data_capture() override;
            virtual void acquire_metadata(buffers_mgr & buf_mgr,fd_set &fds, bool compressed_format = false) override;
            virtual void set_metadata_attributes(buffers_mgr& buf_mgr, __u32 bytesused, uint8_t* md_start);
            void subscribe_to_ctrl_event(uint32_t control_id);
            void unsubscribe_from_ctrl_event(uint32_t control_id);
            bool pend_for_ctrl_status_event();
            void upload_video_and_metadata_from_syncer(buffers_mgr& buf_mgr);
            void populate_imu_data(metadata_hid_raw& meta_data, uint8_t* frame_start, uint8_t& md_size, void** md_start) const;
            // checking if metadata is streamed
            virtual inline bool is_metadata_streamed() const { return false;}
            virtual inline std::shared_ptr<buffer> get_video_buffer(__u32 index) const {return _buffers[index];}
            virtual inline std::shared_ptr<buffer> get_md_buffer(__u32 index) const {return nullptr;}

            struct identifier
            {
                unsigned int major;
                unsigned int minor;

                bool operator== (const identifier& other) const { return major == other.major && minor == other.minor;}
            };

            struct path_and_identifier
            {
                std::string path;
                identifier key;
            };

            static std::vector<path_and_identifier> collect_v4l_video_path_and_identifier();
            static bool get_identifier_from_v4l_video_path(const std::string& v4l_video_path, identifier& key);
            static bool get_devname_from_v4l_video_path(const std::string& v4l_video_path, std::string& devname,
                                                    const std::vector<std::pair <std::string, std::string>>& v4l_to_dev_video_paths);
            static bool get_devname_from_mipi_dfu_path(const path_and_identifier& dfu_path, std::string& dev_name);

            static std::vector<std::pair <std::string, std::string>> generate_v4l_to_dev_video_paths(const std::vector<path_and_identifier>& v4l_videos,
                                                                                                     const std::vector<path_and_identifier>& dev_videos);
            static std::vector<node_info> get_mipi_rs_enum_nodes();
            static std::vector<node_info> collect_uvc_nodes(const std::vector<path_and_identifier>& v4l_videos, const std::vector<node_info>& mipi_rs_enum_nodes,
                                                            const std::vector<std::pair <std::string, std::string>>& v4l_to_dev_video_paths);
            static std::vector<node_info> match_video_with_metadata_nodes(const std::vector<node_info>& uvc_nodes);
            static bool get_info_from_v4l_video_path(const std::string& v4l_video_path, const std::string& dev_name, uvc_device_info& info, bool is_mipi_rs_enum_nodes_empty,
                                                 const std::vector<std::pair <std::string, std::string>>& v4l_to_dev_video_paths);
            static std::vector<path_and_identifier> collect_dev_video_path_and_identifier();
            static uint16_t assign_mipi_mi(const std::string& name);

            power_state _state = D3;
            std::string _name = "";
            std::string _device_path = "";
            usb_spec _device_usb_spec = usb_undefined;
            uvc_device_info _info;

            std::vector<std::shared_ptr<buffer>> _buffers;
            stream_profile _profile;
            frame_callback _callback;
            std::atomic<bool> _is_capturing;
            std::atomic<bool> _is_alive;
            std::atomic<bool> _is_started;
            std::unique_ptr<std::thread> _thread;
            std::unique_ptr<named_mutex> _named_mtx;
            struct device {
                enum v4l2_buf_type buf_type;
                unsigned char num_planes;
                struct v4l2_capability cap;
                struct v4l2_cropcap cropcap;
            } _dev;
            bool _use_memory_map;
            int _max_fd = 0;                    // specifies the maximal pipe number the polling process will monitor
            std::vector<int>  _fds;             // list the file descriptors to be monitored during frames polling
            buffers_mgr     _buf_dispatch;      // Holder for partial (MD only) frames that shall be preserved between 'select' calls when polling v4l buffers
            int _fd = 0;
            frame_drop_monitor _frame_drop_monitor;           // used to check the frames drops kpi
            v4l2_video_md_syncer _video_md_syncer;

        private:
            int _stop_pipe_fd[2]; // write to _stop_pipe_fd[1] and read from _stop_pipe_fd[0]

        };

        // Composition layer for uvc/metadata split nodes introduced with kernel 4.16
        class v4l_uvc_meta_device : public v4l_uvc_device
        {
        public:
            v4l_uvc_meta_device(const uvc_device_info& info, bool use_memory_map = false);

            virtual ~v4l_uvc_meta_device();

            bool is_platform_jetson() const override {return false;}

        protected:

            void streamon() const;
            void streamoff() const;
            void negotiate_kernel_buffers(size_t num) const;
            void allocate_io_buffers(size_t num);
            void map_device_descriptor();
            void unmap_device_descriptor();
            void set_format(stream_profile profile);
            void prepare_capture_buffers();
            virtual void acquire_metadata(buffers_mgr & buf_mgr,fd_set &fds, bool compressed_format=false);
            // checking if metadata is streamed
            virtual inline bool is_metadata_streamed() const { return _md_fd > 0;}
            virtual inline std::shared_ptr<buffer> get_md_buffer(__u32 index) const {return _md_buffers[index];}
            int _md_fd = -1;
            std::string _md_name = "";
            v4l2_buf_type _md_type = LOCAL_V4L2_BUF_TYPE_META_CAPTURE;

            std::vector<std::shared_ptr<buffer>> _md_buffers;
        };


        const uint16_t D457_PID      = 0xABCD;
        const uint16_t D430_GMSL_PID = 0xABCE;
        const uint16_t D415_GMSL_PID = 0xABCF;
        const uint16_t D401_GMSL_PID = 0xABCC;

        static const std::set<std::uint16_t> mipi_devices_pid = {
            D457_PID,
            D430_GMSL_PID,
            D415_GMSL_PID,
            D401_GMSL_PID
        };

        // D457 Development. To be merged into underlying class
        class v4l_mipi_device : public v4l_uvc_meta_device
        {
        public:
            v4l_mipi_device(const uvc_device_info& info, bool use_memory_map = true);
            v4l_mipi_device(const mipi_device_info& info, bool use_memory_map = true);

            static void foreach_mipi_device(
                    std::function<void(const mipi_device_info&,
                                       const std::string&)> action);

            static std::vector<std::string> get_video_paths();

            static mipi_device_info get_info_from_mipi_device_path(
                    const std::string& video_path, const std::string& name);

            static void get_mipi_device_info(const std::string& dev_name,
                                             std::string& bus_info, std::string& card);

            virtual ~v4l_mipi_device();

            bool get_pu(rs2_option opt, int32_t& value) const override;
            bool set_pu(rs2_option opt, int32_t value) override;
            bool set_xu(const extension_unit& xu, uint8_t control, const uint8_t* data, int size) override;
            bool get_xu(const extension_unit& xu, uint8_t control, uint8_t* data, int size) const override;
            control_range get_xu_range(const extension_unit& xu, uint8_t control, int len) const override;
            control_range get_pu_range(rs2_option option) const override;
            void set_metadata_attributes(buffers_mgr& buf_mgr, __u32 bytesused, uint8_t* md_start) override;
            bool is_platform_jetson() const override;
        protected:
            virtual uint32_t get_cid(rs2_option option) const;
            uint32_t xu_to_cid(const extension_unit& xu, uint8_t control) const; // Find the mapping of XU to the underlying control
        };

        class v4l_backend : public backend
        {
        public:
            std::shared_ptr<uvc_device> create_uvc_device(uvc_device_info info) const override;
            std::vector<uvc_device_info> query_uvc_devices() const override;

            std::shared_ptr<command_transfer> create_usb_device(usb_device_info info) const override;
            std::vector<usb_device_info> query_usb_devices() const override;

            std::shared_ptr<hid_device> create_hid_device(hid_device_info info) const override;
            std::vector<hid_device_info> query_hid_devices() const override;

            std::vector<mipi_device_info> query_mipi_devices() const override;

            std::shared_ptr<device_watcher> create_device_watcher() const override;
        };
    }
}
