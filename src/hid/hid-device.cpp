// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#include <queue>
#include "hid-device.h"
#include <src/platform/hid-data.h>

#include <rsutils/string/from.h>
#include <rsutils/string/hexdump.h>


const int USB_REQUEST_COUNT = 1;

namespace librealsense
{
    namespace platform
    {
        std::vector<hid_device_info> query_hid_devices_info()
        {
            std::vector<std::string> hid_sensors = { gyro, accel, custom };

            std::vector<hid_device_info> rv;
            auto usb_devices = platform::usb_enumerator::query_devices_info();
            for (auto&& info : usb_devices) {
                if(info.cls != RS2_USB_CLASS_HID)
                    continue;
                platform::hid_device_info device_info;
                device_info.vid = rsutils::string::from() << std::uppercase << rsutils::string::hexdump( info.vid );
                device_info.pid = rsutils::string::from() << std::uppercase << rsutils::string::hexdump( info.pid );
                device_info.unique_id = info.unique_id;
                device_info.device_path = info.unique_id;//the device unique_id is the USB port

                for(auto&& hs : hid_sensors)
                {
                    device_info.id = hs;
                    // LOG_INFO("Found HID device: " << std::string(device_info).c_str());
                    rv.push_back(device_info);
                }
            }
            return rv;
        }

        std::shared_ptr<hid_device> create_rshid_device(hid_device_info info)
        {
            auto devices = usb_enumerator::query_devices_info();
            for (auto&& usb_info : devices)
            {
                if(usb_info.unique_id != info.unique_id || usb_info.cls != RS2_USB_CLASS_HID)
                    continue;

                auto dev = usb_enumerator::create_usb_device(usb_info);
                return std::make_shared<rs_hid_device>(dev);
            }

            return nullptr;
        }

        rs_hid_device::rs_hid_device(rs_usb_device usb_device)
            : _usb_device(usb_device),
              _action_dispatcher(10, rsutils::concurrency::thread_category_usb_io, "hid-dev-d")
        {
            _id_to_sensor[REPORT_ID_GYROMETER_3D] = gyro;
            _id_to_sensor[REPORT_ID_ACCELEROMETER_3D] = accel;
            _id_to_sensor[REPORT_ID_CUSTOM] = custom;
            _sensor_to_id[gyro] = REPORT_ID_GYROMETER_3D;
            _sensor_to_id[accel] = REPORT_ID_ACCELEROMETER_3D;
            _sensor_to_id[custom] = REPORT_ID_CUSTOM;
#ifdef __APPLE__
            hidapi_device_info *devices = hid_enumerate(0x8086, 0x0);
            if(devices)
            {
                _hidapi_device = hid_open(devices[0].vendor_id, devices[0].product_id, devices[0].serial_number);
                hidapi_PowerDevice(REPORT_ID_ACCELEROMETER_3D);
                hidapi_PowerDevice(REPORT_ID_GYROMETER_3D);
            }
#endif
            _action_dispatcher.start();
        }

        rs_hid_device::~rs_hid_device()
        {
#ifdef __APPLE__
            if( _hidapi_device )
                hid_close( _hidapi_device );
#endif
            _action_dispatcher.stop();
        }

        std::vector<hid_sensor> rs_hid_device::get_sensors()
        {
            std::vector<hid_sensor> sensors;

            for (auto& sensor : _hid_profiles)
                sensors.push_back({ sensor.sensor_name });

            return sensors;
        }

        void rs_hid_device::open(const std::vector<hid_profile>& hid_profiles)
        {
            for(auto&& p : hid_profiles)
            {
                set_feature_report( DEVICE_POWER_D0, _sensor_to_id[p.sensor_name], p.frequency, p.sensitivity );
            }
            _configured_profiles = hid_profiles;
        }

        void rs_hid_device::close()
        {
            set_feature_report(DEVICE_POWER_D4, REPORT_ID_ACCELEROMETER_3D);
            set_feature_report(DEVICE_POWER_D4, REPORT_ID_GYROMETER_3D);
        }

        void rs_hid_device::stop_capture()
        {
            _action_dispatcher.invoke_and_wait([this](dispatcher::cancellable_timer c)
            {
                if(!_running)
                    return;
#ifndef __APPLE__
                _request_callback->cancel();

                _queue.clear();

                for (auto&& r : _requests)
                    _messenger->cancel_request(r);

                _requests.clear();
#endif
                _handle_interrupts_thread->stop();
#ifndef __APPLE__
                _messenger.reset();
#endif
               _running = false;
            }, [this](){ return !_running; });
        }

        void rs_hid_device::start_capture(hid_callback callback)
        {
            _action_dispatcher.invoke_and_wait([this, callback](dispatcher::cancellable_timer c)
            {
                if(_running)
                    return;

                _callback = callback;
#ifndef __APPLE__
                auto in = get_hid_interface()->get_number();
                _messenger = _usb_device->open(in);
#endif
                _handle_interrupts_thread = std::make_shared<active_object<>>([this](dispatcher::cancellable_timer cancellable_timer)
                {
                    handle_interrupt();
                }, rsutils::concurrency::thread_category_sensor_io, "hid-intr");

                _handle_interrupts_thread->start();

#ifndef __APPLE__
                _request_callback = std::make_shared<usb_request_callback>([&](platform::rs_usb_request r)
                    {
                        _action_dispatcher.invoke([this, r](dispatcher::cancellable_timer c)
                        {
                            if(!_running)
                                return;
                            
                            if( r->get_actual_length() == _realsense_hid_report_actual_size )
                            {
                                // for FW version < 5.16 the actual struct is 32 bytes (each IMU axis is 16 bit), so we
                                // can not use memcpy for
                                // the whole struct as the new struct (size 38) expect 32 bits for each.
                                // For FW >= 5.16 we can just use memcpy as the structs size match
                                REALSENSE_HID_REPORT report;
                                if( _realsense_hid_report_actual_size != sizeof( REALSENSE_HID_REPORT ) )
                                {

                                    // x,y,z are all short with: x at offset 10
                                    //                           y at offset 12
                                    //                           z at offset 14
                                    memcpy( &report, r->get_buffer().data(), 10 );
                                    const int16_t * x
                                        = reinterpret_cast< const int16_t * >( r->get_buffer().data() + 10 );
                                    const int16_t * y
                                        = reinterpret_cast< const int16_t * >( r->get_buffer().data() + 12 );
                                    const int16_t * z
                                        = reinterpret_cast< const int16_t * >( r->get_buffer().data() + 14 );
                                    report.x = *x;
                                    report.y = *y;
                                    report.z = *z;
                                    memcpy( &report.customValue1, r->get_buffer().data() + 16, 16 );

                                }
                                else
                                {
                                    // the rest of the data in the old struct size (after z element) starts from offset
                                    // 16 and has 16 bytes till end
                                    memcpy( &report, r->get_buffer().data(), r->get_actual_length() );
                                }
                                _queue.enqueue(std::move(report));
                            }
                            auto sts = _messenger->submit_request(r);
                            if (sts != platform::RS2_USB_STATUS_SUCCESS)
                                LOG_ERROR("failed to submit UVC request");
                        });

                    });

                _requests = std::vector<rs_usb_request>(USB_REQUEST_COUNT);
                for(auto&& r : _requests)
                {
                    r = _messenger->create_request(get_hid_endpoint());
                    r->set_buffer(std::vector<uint8_t>(sizeof(REALSENSE_HID_REPORT)));
                    r->set_callback(_request_callback);
                }
#endif
                _running = true;
#ifndef __APPLE__
                for(auto&& r : _requests)
                    _messenger->submit_request(r);
#endif

            }, [this](){ return _running; });
        }

        void rs_hid_device::handle_interrupt()
        {
            REALSENSE_HID_REPORT report;

            // for FW version < 5.16 the actual struct is 32 bytes (each IMU axis is 16 bit), so we can not use memcpy for
            // the whole struct as the new struct (size 38) expect 32 bits for each.
            // For FW >= 5.16 we can just use memcpy as the structs size match
           

#ifdef __APPLE__
            unsigned char tmp_buffer[100] = { 0 };
            hid_read( _hidapi_device, tmp_buffer, _realsense_hid_report_actual_size );
            if( _realsense_hid_report_actual_size != sizeof( REALSENSE_HID_REPORT ) )
            {
               
                // x,y,z are all short with: x at offset 10
                //                           y at offset 12
                //                           z at offset 14
                memcpy( &report, tmp_buffer, 10 );
                const int16_t * x = reinterpret_cast< const int16_t * >( tmp_buffer + 10 );
                const int16_t * y = reinterpret_cast< const int16_t * >( tmp_buffer + 12 );
                const int16_t * z = reinterpret_cast< const int16_t * >( tmp_buffer + 14 );
                report.x = *x;
                report.y = *y;
                report.z = *z;

                // the rest of the data in the old struct size (after z element) starts from offset 16 and has 16 bytes till end
                memcpy( &report.customValue1, tmp_buffer + 16, 16 );
            }
            else
            {
                memcpy( &report, tmp_buffer, sizeof( REALSENSE_HID_REPORT ) );
            }

            sensor_data data{};
            data.sensor = { _id_to_sensor[report.reportId] };

            hid_data hid{};
            hid.x = report.x;
            hid.y = report.y;
            hid.z = report.z;

            data.fo.pixels = &(hid.x);
            data.fo.metadata = &(report.timeStamp);
            data.fo.frame_size = sizeof(REALSENSE_HID_REPORT);
            data.fo.metadata_size = sizeof(report.timeStamp);

            _callback(data);
#else
            if(_queue.dequeue(&report, 10))
            {
                if(std::find_if(_configured_profiles.begin(), _configured_profiles.end(), [&](hid_profile& p)
                {
                    return _id_to_sensor[report.reportId].compare(p.sensor_name) == 0;

                }) != _configured_profiles.end())
                {
                    sensor_data data{};
                    data.sensor = { _id_to_sensor[report.reportId] };

                    hid_data hid{};
                    hid.x = report.x;
                    hid.y = report.y;
                    hid.z = report.z;

                    data.fo.pixels = &(hid.x);
                    data.fo.metadata = &(report.timeStamp);
                    data.fo.frame_size = sizeof(REALSENSE_HID_REPORT);
                    data.fo.metadata_size = sizeof(report.timeStamp);

                    _callback(data);
                }
            }
#endif
        }

        usb_status rs_hid_device::set_feature_report( unsigned char power, int report_id, int fps, double sensitivity)
        {
            uint32_t transferred;


            int value = (HID_REPORT_TYPE_FEATURE << 8) + report_id;

            FEATURE_REPORT featureReport;
            auto hid_interface = get_hid_interface()->get_number();

            auto dev = _usb_device->open(hid_interface);

            if (!dev)
                return RS2_USB_STATUS_NO_DEVICE;

            auto res = dev->control_transfer(USB_REQUEST_CODE_GET,
                HID_REQUEST_GET_REPORT,
                value,
                hid_interface,
                (uint8_t*) &featureReport,
                sizeof(featureReport),
                transferred,
                1000);

            if(res != RS2_USB_STATUS_SUCCESS)
            {
                LOG_WARNING("control_transfer of USB_REQUEST_CODE_GET failed return value is: " << res);
                return res;
            }

            featureReport.power = power;

            if(fps > 0)
                featureReport.report = (1000 / fps);

            //we want to change the sensitivity values only in gyro, for FW version >= 5.16
            if( featureReport.reportId == REPORT_ID_GYROMETER_3D
                && _realsense_hid_report_actual_size == sizeof( REALSENSE_HID_REPORT ) )
                featureReport.sensitivity = static_cast<unsigned short>(sensitivity);


            res = dev->control_transfer(USB_REQUEST_CODE_SET,
                HID_REQUEST_SET_REPORT,
                value,
                hid_interface,
                (uint8_t*) &featureReport,
                sizeof(featureReport),
                transferred,
                1000);

            if(res != RS2_USB_STATUS_SUCCESS)
            {
                LOG_WARNING("control_transfer of USB_REQUEST_CODE_SET failed return value is: " << res);
                return res;
            }

            res = dev->control_transfer(USB_REQUEST_CODE_GET,
                HID_REQUEST_GET_REPORT,
                value,
                hid_interface,
                (uint8_t*) &featureReport,
                sizeof(featureReport),
                transferred,
                1000);

            if(res != RS2_USB_STATUS_SUCCESS)
            {
                LOG_WARNING("control_transfer of USB_REQUEST_CODE_GET failed return value is: " << res);
                return res;
            }

            if(featureReport.power != power)
            {
                LOG_WARNING("faild to set power: " << power);
                return RS2_USB_STATUS_OTHER;
            }
            return res;
        }

#ifdef __APPLE__
        int rs_hid_device::hidapi_PowerDevice(unsigned char reportId)
        {
            if (_hidapi_device == NULL)
                return -1;

            REALSENSE_FEATURE_REPORT featureReport;
            int ret;

            memset(&featureReport,0, sizeof(featureReport));
            featureReport.reportId = reportId;
            // Reading feature report.
            ret = hid_get_feature_report(_hidapi_device, (unsigned char *)
                                         &featureReport ,sizeof(featureReport) );

            if (ret == -1) {
                LOG_ERROR("fail to read feature report from device");
                return ret;
            }

            // change report to power the device to D0

            featureReport.power = DEVICE_POWER_D0;

            // Write feature report back.
            ret = hid_send_feature_report(_hidapi_device, (unsigned char *) &featureReport, sizeof(featureReport));

            if (ret == -1) {
                LOG_ERROR("fail to write feature report from device");
                return ret;
            }

            ret = hid_get_feature_report(_hidapi_device, (unsigned char *) &featureReport ,sizeof(featureReport) );

            if (ret == -1) {
                LOG_ERROR("fail to read feature report from device");
                return ret;
            }

            if (featureReport.power == DEVICE_POWER_D0) {
                LOG_INFO("Device is powered up");
            } else {
                LOG_INFO("Device is powered off");
                return -1;
            }

            return 0;
        }
#endif

        rs_usb_interface rs_hid_device::get_hid_interface()
        {
            auto intfs = _usb_device->get_interfaces();

            auto it = std::find_if(intfs.begin(), intfs.end(),
                [](const rs_usb_interface& i) { return i->get_class() == RS2_USB_CLASS_HID; });

            if (it == intfs.end())
                throw std::runtime_error("can't find HID interface of device: " + _usb_device->get_info().id);

            return *it;
        }

        rs_usb_endpoint rs_hid_device::get_hid_endpoint()
        {
            auto hid_interface = get_hid_interface();

            auto ep = hid_interface->first_endpoint(RS2_USB_ENDPOINT_DIRECTION_READ, RS2_USB_ENDPOINT_INTERRUPT);
            if(!ep)
                throw std::runtime_error("can't find HID endpoint of device: " + _usb_device->get_info().id);

            return ep;
        }

        void rs_hid_device::set_gyro_scale_factor(double scale_factor) 
        {
            _gyro_scale_factor = scale_factor;
            // for FW >=5.16 the scale factor changes to 10000.0 since FW sends 32bit
            if( scale_factor == 10000.0 )
                _realsense_hid_report_actual_size = 38;
        }
    }
}
