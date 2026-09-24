// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "v4l-enumerator.h"
#include "v4l-frame-drop-monitor.h"
#include "v4l-ioctl.h"
#include "v4l-kernel-buffers.h"
#include "v4l-named-mutex.h"
#include "v4l-video-md-syncer.h"

#include "backend.h"
#include <src/platform/uvc-device.h>
#include <src/metadata.h>
#include "types.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace librealsense
{
    namespace platform
    {
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

        class v4l_uvc_device : public uvc_device, public v4l_uvc_interface
        {
        public:
            v4l_uvc_device(const uvc_device_info& info, bool use_memory_map = false);

            virtual ~v4l_uvc_device() override;

            void probe_and_commit(stream_profile profile, frame_callback callback, int buffers) override;

            void stream_on(std::function<void(const notification& n)> error_handler) override;

            void start_callbacks() override;

            void stop_callbacks() override;

            void close(stream_profile) override;

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
            void assign_device_capabilities();
            void subscribe_to_ctrl_event(uint32_t control_id);
            void unsubscribe_from_ctrl_event(uint32_t control_id);
            bool pend_for_ctrl_status_event();
            void upload_video_and_metadata_from_syncer(buffers_mgr& buf_mgr);
            void populate_imu_data(metadata_hid_raw& meta_data, uint8_t* frame_start, uint8_t& md_size, void** md_start) const;
            // Call immediately after a failed DQBUF, before anything else touches errno. Returns true (and
            // marks the device disconnected) if the failure was ENODEV; the caller should bail out on true.
            bool handle_enodev_on_dqbuf(const char* fd_label, int fd);
            // checking if metadata is streamed
            virtual inline bool is_metadata_streamed() const { return false;}
            virtual inline std::shared_ptr<buffer> get_video_buffer(__u32 index) const {return _buffers[index];}
            virtual inline std::shared_ptr<buffer> get_md_buffer(__u32 index) const {return nullptr;}


            power_state _state = D3;
            std::string _name = "";
            std::string _device_path = "";
            usb_spec _device_usb_spec = usb_undefined;
            uvc_device_info _info;

            std::vector<std::shared_ptr<buffer>> _buffers;
            stream_profile _profile;
            bool _variable_frame_size = false; // some frames may arrive in shorter size than the buffer - skip the partial frame check
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
            bool _are_device_capabilities_assigned;
            // true if a video or metadata buffer DQBUF revealed the device was physically removed (ENODEV).
            // Consumed (and reset) once at the top of the next poll(), see poll() for details.
            std::atomic<bool> _device_disconnected{ false };

        private:
            int _stop_pipe_fd[2]; // write to _stop_pipe_fd[1] and read from _stop_pipe_fd[0]

        };
    }  // namespace platform
}  // namespace librealsense
