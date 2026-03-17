// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#pragma once
#include "../backend.h"
#include "hid-types.h"
#include "../platform/hid-device.h"
#include "../usb/usb-messenger.h"
#include "../usb/usb-enumerator.h"
#include <rsutils/concurrency/concurrency.h>
#include "stdio.h"
#include "stdlib.h"

#include <cstring>
#include <string>
#include <chrono>
#include <thread>
#include "../types.h"

#ifdef __APPLE__
#include <hidapi.h>
#endif

namespace librealsense
{
    namespace platform
    {
        std::vector<hid_device_info> query_hid_devices_info();
        std::shared_ptr<hid_device> create_rshid_device(hid_device_info info);

        class rs_hid_device : public hid_device
        {
        public:
            rs_hid_device(rs_usb_device usb_device);
            virtual ~rs_hid_device();

            void register_profiles(const std::vector<hid_profile>& hid_profiles) override { _hid_profiles = hid_profiles; }
            void open(const std::vector<hid_profile>& hid_profiles) override;
            void close() override;
            void stop_capture() override;
            void start_capture(hid_callback callback) override;
            std::vector<hid_sensor> get_sensors() override;
            virtual std::vector<uint8_t> get_custom_report_data(const std::string& custom_sensor_name,
                                                                const std::string& report_name,
                                                                custom_sensor_report_field report_field) override { return {}; }
            void set_gyro_scale_factor( double scale_factor ) override;

        private:
            void handle_interrupt();
            rs_usb_endpoint get_hid_endpoint();
            rs_usb_interface get_hid_interface();
            //for gyro sensitivity the default value we set in feature report is 0.1, which is mapped in FW to 30.5 millideg/sec
            usb_status set_feature_report( unsigned char power, int report_id, int fps = 0, double sensitivity = 0.1 );
#ifdef __APPLE__
           int hidapi_PowerDevice(unsigned char reportId);
#endif

            bool _running = false;
            dispatcher _action_dispatcher;

            hid_callback _callback;
            rs_usb_device _usb_device;
#ifdef __APPLE__
            hidapi_device* _hidapi_device = nullptr;
#else
            rs_usb_messenger _messenger;
            std::mutex _messenger_mutex;
            std::vector<rs_usb_request> _requests;
            std::shared_ptr<platform::usb_request_callback> _request_callback;
#endif

            std::vector<hid_profile> _hid_profiles;
            std::map<int, std::string> _id_to_sensor;
            std::map<std::string, int> _sensor_to_id;
            std::vector<hid_profile> _configured_profiles;
            single_consumer_queue<REALSENSE_HID_REPORT> _queue;
            std::shared_ptr<active_object<>> _handle_interrupts_thread;
            int _realsense_hid_report_actual_size = 32; // for FW version >=5.16 the struct changed to 38 bit
            double _gyro_scale_factor = 10.0;
        };
    }
}
