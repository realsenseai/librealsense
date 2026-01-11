// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#pragma once
#include <src/platform/uvc-device.h>
#include <src/platform/uvc-device-info.h>
#include <src/usb/usb-messenger.h>
#include <src/usb/usb-enumerator.h>

#include "uvc-types.h"

#include "stdio.h"
#include "stdlib.h"
#include <cstring>
#include <string>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>

typedef void(uvc_frame_callback_t)(struct librealsense::platform::frame_object *frame, void *user_ptr);

namespace librealsense
{
    namespace platform
    {
        class uvc_parser;
        class uvc_streamer;

        std::vector<uvc_device_info> query_uvc_devices_info();
        std::shared_ptr<uvc_device> create_rsuvc_device(uvc_device_info info);

        struct profile_and_callback
        {
            stream_profile profile;
            frame_callback callback = nullptr;
        };

        class rs_uvc_device : public uvc_device
        {
        public:
            rs_uvc_device(const rs_usb_device& usb_device, const uvc_device_info &info, uint8_t usb_request_count = 2);
            virtual ~rs_uvc_device();

            virtual void probe_and_commit(stream_profile profile, frame_callback callback, int buffers = DEFAULT_V4L2_FRAME_BUFFERS) override;
            virtual void stream_on(std::function<void(const notification& n)> error_handler = [](const notification& n){}) override;
            virtual void start_callbacks() override;
            virtual void stop_callbacks() override;
            virtual void close(stream_profile profile) override;

            virtual void set_power_state(power_state state) override;
            virtual power_state get_power_state() const override;

            void register_xu( platform::extension_unit && xu ) override {} // Not supported
            virtual bool set_xu(const extension_unit& xu, uint8_t ctrl, const uint8_t* data, int len) override;
            virtual bool get_xu(const extension_unit& xu, uint8_t ctrl, uint8_t* data, int len) const override;
            virtual control_range get_xu_range(const extension_unit& xu, uint8_t ctrl, int len) const override;

            virtual bool get_pu(rs2_option opt, int32_t& value) const override;
            virtual bool set_pu(rs2_option opt, int32_t value) override;
            virtual control_range get_pu_range(rs2_option opt) const override;

            virtual std::vector<stream_profile> get_profiles() const override;

            virtual void lock() const override;
            virtual void unlock() const override;

            virtual std::string get_device_location() const override;
            virtual usb_spec  get_usb_specification() const override;

            bool is_platform_jetson() const override { return false;}

        private:
            friend class source_reader_callback;

            void close_uvc_device();

            usb_status probe_stream_ctrl(const std::shared_ptr<uvc_stream_ctrl_t>& control);
            usb_status get_stream_ctrl_format_size(uvc_format_t format, const std::shared_ptr<uvc_stream_ctrl_t>& control);
            usb_status query_stream_ctrl(const std::shared_ptr<uvc_stream_ctrl_t>& control, uint8_t probe, int req);
            std::vector<uvc_format_t> get_available_formats_all() const;

            bool uvc_get_ctrl(uint8_t unit, uint8_t ctrl, void *data, int len, uvc_req_code req_code) const;
            bool uvc_set_ctrl(uint8_t unit, uint8_t ctrl, void *data, int len);

            int32_t rs2_value_translate(uvc_req_code action, rs2_option option, int32_t value) const;
            void play_profile(stream_profile profile, frame_callback callback);
            void stop_stream_cleanup(const stream_profile& profile, std::vector<profile_and_callback>::iterator& elem);
            void check_connection() const;

            int rs2_option_to_ctrl_selector(rs2_option option, int &unit) const;
            int32_t get_data_usb(uvc_req_code action, int control, int unit, unsigned int length = sizeof(uint32_t)) const;
            void set_data_usb(uvc_req_code action, int control, int unit, int value) const;

            void listen_to_interrupts();

            const uvc_device_info                   _info;
            power_state                             _power_state = D3; // power state change is unsupported
            std::recursive_mutex                    _power_lock;
            std::atomic< int >                      _power_counter;
            std::vector<profile_and_callback>       _streams;

            std::string                             _location;
            std::vector<stream_profile>             _profiles;
            std::vector<frame_callback>             _frame_callbacks;

            rs_usb_device                           _usb_device = nullptr;
            rs_usb_messenger                        _messenger;
            rs_usb_request                          _interrupt_request;
            rs_usb_request_callback                 _interrupt_callback;
            uint8_t                                 _usb_request_count;

            mutable dispatcher                      _action_dispatcher;
            // uvc internal
            std::shared_ptr<uvc_parser>             _parser;
            std::vector<std::shared_ptr<uvc_streamer>> _streamers;
        };
    }
}
