// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.
#pragma once

#include "../platform/hid-device.h"
#include "../platform/hid-device-info.h"
#include "win/win-helpers.h"

#include <Sensorsapi.h>
#include <wrl/client.h>
#include <rsutils/string/windows.h>

namespace librealsense
{
    namespace platform
    {

        class wmf_hid_sensor {
        public:
            wmf_hid_sensor(const hid_device_info& device_info, Microsoft::WRL::ComPtr<ISensor> pISensor) :
                _hid_device_info(device_info), _pISensor(pISensor)
            {
                BSTR fName = nullptr;
                auto res = pISensor->GetFriendlyName( &fName );
                if( FAILED( res ) )
                {
                    _name = "Unidentified HID Sensor";
                }
                else
                {
                    _name = rsutils::string::windows::win_to_utf( fName );
                    SysFreeString( fName );
                }
            };

            const std::string& get_sensor_name() const { return _name; }
            const Microsoft::WRL::ComPtr<ISensor>& get_sensor() const { return _pISensor; }

            HRESULT start_capture(ISensorEvents* sensorEvents)
            {
                return _pISensor->SetEventSink(sensorEvents);
            }

            HRESULT stop_capture()
            {
                return _pISensor->SetEventSink(nullptr);
            }

        private:

            hid_device_info _hid_device_info;
            Microsoft::WRL::ComPtr<ISensor> _pISensor;
            std::string _name;
        };

        class wmf_backend;

        class wmf_hid_device : public hid_device
        {
        public:
            static void foreach_hid_device(std::function<void(hid_device_info, Microsoft::WRL::ComPtr<ISensor>)> action);
            wmf_hid_device(const hid_device_info& info, std::shared_ptr<const wmf_backend> backend);

            void register_profiles(const std::vector<hid_profile>& hid_profiles) override { _hid_profiles = hid_profiles;}
            void open(const std::vector<hid_profile>&iio_profiles) override;
            void close() override;
            void stop_capture() override;
            void start_capture(hid_callback callback) override;
            std::vector<hid_sensor> get_sensors() override; // Get opened sensors
            std::vector<uint8_t> get_custom_report_data(const std::string& custom_sensor_name, const std::string& report_name, custom_sensor_report_field report_field) override;
            void set_gyro_scale_factor( double scale_factor ) override;

        private:
            // Don't move the position of wmf_backend member. This object must be destroyed only after COM objects.
            std::shared_ptr<const wmf_backend>           _backend;

            std::vector<std::shared_ptr<wmf_hid_sensor>> _connected_sensors; // Vector of all connected sensors of this device
            std::vector<std::shared_ptr<wmf_hid_sensor>> _opened_sensors;    // Vector of all opened sensors of this device (subclass of _connected_sensors)
            std::vector<std::shared_ptr<wmf_hid_sensor>> _streaming_sensors; // Vector of all streaming sensors of this device (subclass of _connected_sensors)

            Microsoft::WRL::ComPtr<ISensorEvents> _cb;
            std::vector<hid_profile> _hid_profiles;
            //10.0 was used for D400 before FW support to gyro sensitivity control
            double _gyro_scale_factor = 10.0;
        };
    }
}
