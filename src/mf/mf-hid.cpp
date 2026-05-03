// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "../types.h"
#include "mf-hid.h"
#include "win/win-helpers.h"
#include "metadata.h"

#include <PortableDeviceTypes.h>
#include <PortableDevice.h>
#include <Windows.h>
#include <Sensorsapi.h>
#include <sensors.h>
#include <SensAPI.h>
#include <initguid.h>
#include <propkeydef.h>
#include <wrl/implements.h>
#include <rsutils/string/from.h>
#include <rsutils/string/windows.h>

#pragma comment(lib, "Sensorsapi.lib")
#pragma comment(lib, "PortableDeviceGuids.lib")

// Windows Filetime is represented in 64 - bit number of 100 - nanosecond intervals since midnight Jan 1, 1601
// To convert to the Unix epoch, subtract 116444736000000000LL to reach Jan 1, 1970.
constexpr uint64_t WIN_FILETIME_2_UNIX_SYSTIME = 116444736000000000LL;

namespace librealsense
{
    namespace platform
    {
        class sensor_events : public Microsoft::WRL::RuntimeClass<
            Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
            ISensorEvents>
        {
        public:
            ~sensor_events() override = default;

            explicit sensor_events(hid_callback callback, double gyro_scale_factor = 10.0)
                : _callback(callback), _gyro_scale_factor(gyro_scale_factor) {}

            //
            // ISensorEvents methods.
            //

            IFACEMETHODIMP OnEvent(
                ISensor * /*pSensor*/,
                REFGUID /*eventID*/,
                IPortableDeviceValues * /*pEventData*/) override
            {
                return S_OK;
            }

            IFACEMETHODIMP OnDataUpdated(ISensor *pSensor, ISensorDataReport *report) override
            {
                if (NULL == report ||
                    NULL == pSensor)
                {
                    return E_INVALIDARG;
                }

                BSTR        fName{};
                SYSTEMTIME  sys_time;
                FILETIME    file_time;
                report->GetTimestamp(&sys_time);

                PROPVARIANT var = {};
                // Custom timestamp low
                auto hr = (report->GetSensorValue(SENSOR_DATA_TYPE_CUSTOM_VALUE1, &var));
                if (FAILED(hr)) return S_OK;
                auto customTimestampLow = var.ulVal;

                // Custom timestamp high
                CHECK_HR(report->GetSensorValue(SENSOR_DATA_TYPE_CUSTOM_VALUE2, &var));
                auto customTimestampHigh = var.ulVal;

                // Parse additional custom fields
                CHECK_HR(report->GetSensorValue(SENSOR_DATA_TYPE_CUSTOM_VALUE6, &var));
                uint8_t imu_count = var.bVal;
                CHECK_HR(report->GetSensorValue(SENSOR_DATA_TYPE_CUSTOM_VALUE7, &var));
                uint8_t usb_count = var.bVal;

                /* Retrieve sensor type - Sensor types are more specific groupings than sensor categories. Sensor type IDs are GUIDs that are defined in Sensors.h */

                SENSOR_TYPE_ID type{};

                CHECK_HR(pSensor->GetType(&type));

                double rawX, rawY, rawZ;


                if (type == SENSOR_TYPE_ACCELEROMETER_3D)
                {
                    CHECK_HR(report->GetSensorValue(SENSOR_DATA_TYPE_ACCELERATION_X_G, &var));
                    rawX = var.dblVal;

                    CHECK_HR(report->GetSensorValue(SENSOR_DATA_TYPE_ACCELERATION_Y_G, &var));
                    rawY = var.dblVal;

                    CHECK_HR(report->GetSensorValue(SENSOR_DATA_TYPE_ACCELERATION_Z_G, &var));
                    rawZ = var.dblVal;

                    static constexpr double accelerator_transform_factor = 1000.0;

                    rawX *= accelerator_transform_factor;
                    rawY *= accelerator_transform_factor;
                    rawZ *= accelerator_transform_factor;
                }
                else if (type == SENSOR_TYPE_GYROMETER_3D)
                {
                    // Raw X
                    CHECK_HR(report->GetSensorValue(SENSOR_DATA_TYPE_ANGULAR_VELOCITY_X_DEGREES_PER_SECOND, &var));
                    rawX = var.dblVal;

                    // Raw Y
                    CHECK_HR(report->GetSensorValue(SENSOR_DATA_TYPE_ANGULAR_VELOCITY_Y_DEGREES_PER_SECOND, &var));
                    rawY = var.dblVal;

                    // Raw Z
                    CHECK_HR(report->GetSensorValue(SENSOR_DATA_TYPE_ANGULAR_VELOCITY_Z_DEGREES_PER_SECOND, &var));
                    rawZ = var.dblVal;

                    rawX *= _gyro_scale_factor;
                    rawY *= _gyro_scale_factor;
                    rawZ *= _gyro_scale_factor;
                }
                else
                {
                    /* Unsupported sensor */
                    return S_FALSE;
                }

                PropVariantClear(&var);

                sensor_data d{};
                hid_sensor_data data{};
                // Populate HID IMU data - Header
                metadata_hid_raw meta_data{};
                meta_data.header.report_type = md_hid_report_type::hid_report_imu;
                meta_data.header.length = hid_header_size + metadata_imu_report_size;
                meta_data.header.timestamp = customTimestampLow | (uint64_t(customTimestampHigh) << 32);
                // Payload:
                meta_data.report_type.imu_report.header.md_type_id = md_type::META_DATA_HID_IMU_REPORT_ID;
                meta_data.report_type.imu_report.header.md_size = metadata_imu_report_size;
                meta_data.report_type.imu_report.flags = static_cast<uint8_t>( md_hid_imu_attributes::custom_timestamp_attirbute |
                                                                                md_hid_imu_attributes::imu_counter_attribute |
                                                                                md_hid_imu_attributes::usb_counter_attribute);
                meta_data.report_type.imu_report.custom_timestamp = customTimestampLow | (uint64_t(customTimestampHigh) << 32);
                meta_data.report_type.imu_report.imu_counter = imu_count;
                meta_data.report_type.imu_report.usb_counter = usb_count;

                data.x = static_cast<int32_t>(rawX);
                data.y = static_cast<int32_t>(rawY);
                data.z = static_cast<int32_t>(rawZ);
                data.ts_low = customTimestampLow;
                data.ts_high = customTimestampHigh;

                if (SUCCEEDED(pSensor->GetFriendlyName(&fName)) && fName)
                {
                    d.sensor.name = rsutils::string::windows::win_to_utf(fName);
                    SysFreeString(fName); // free string after it was copied to sensor data
                }

                d.fo.pixels = &data;
                d.fo.metadata = &meta_data;
                d.fo.metadata_size = metadata_hid_raw_size;
                d.fo.frame_size = sizeof(data);
                d.fo.backend_time = 0;
                if (SystemTimeToFileTime(&sys_time, &file_time))
                {
                    auto ll_now = (LONGLONG)file_time.dwLowDateTime + ((LONGLONG)(file_time.dwHighDateTime) << 32LL) - WIN_FILETIME_2_UNIX_SYSTIME;
                    d.fo.backend_time = ll_now * 0.0001; //100 nano-sec to millisec
                }

                _callback(d);

                return S_OK;
            }

            IFACEMETHODIMP OnLeave(REFSENSOR_ID /*sensorID*/) override
            {
                return S_OK;
            }

            IFACEMETHODIMP OnStateChanged(ISensor* pSensor, SensorState state) override
            {
                if (nullptr == pSensor)
                    return E_INVALIDARG;

                if (state == SENSOR_STATE_READY)
                    LOG_DEBUG("HID sensor is now ready");
                else if (state == SENSOR_STATE_ACCESS_DENIED)
                    LOG_WARNING("No permission for the HID sensor; enable it in the control panel");

                return S_OK;
            }

        private:
            hid_callback _callback;
            double _gyro_scale_factor = 10.0;
        };

        void wmf_hid_device::open(const std::vector<hid_profile>&iio_profiles)
        {
            try
            {
                for (auto& profile_to_open : iio_profiles)
                {
                    for (auto& connected_sensor : _connected_sensors)
                    {
                        if (profile_to_open.sensor_name == connected_sensor->get_sensor_name())
                        {
                            /* Set SENSOR_PROPERTY_CURRENT_REPORT_INTERVAL sensor property to profile */
                            Microsoft::WRL::ComPtr<IPortableDeviceValues> pPropsToSet;
                            Microsoft::WRL::ComPtr<IPortableDeviceValues> pPropsReturn;

                            /* Create the input object */
                            CHECK_HR(CoCreateInstance(__uuidof(PortableDeviceValues), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pPropsToSet)));

                            /* Add the current report interval property */
                            HRESULT hr = pPropsToSet->SetUnsignedIntegerValue(SENSOR_PROPERTY_CURRENT_REPORT_INTERVAL, profile_to_open.frequency);
                            if (SUCCEEDED(hr))
                            {
                                // Setting a single property
                                hr = connected_sensor->get_sensor()->SetProperties(pPropsToSet.Get(), &pPropsReturn);
                                if (SUCCEEDED(hr))
                                {
                                    _opened_sensors.push_back(connected_sensor);
                                }
                            }

                            //currently implemented only for Gyro sensitivity
                            if( profile_to_open.sensor_name == "HID Sensor Class Device: Gyroscope" )
                            {
                                // creating IPortableDeviceValues container for <Data Field, Sensitivity> tuples
                                Microsoft::WRL::ComPtr<IPortableDeviceValues> pInSensitivityValues;
                                CHECK_HR( CoCreateInstance( CLSID_PortableDeviceValues,
                                                         nullptr,
                                                         CLSCTX_INPROC_SERVER,
                                                         IID_PPV_ARGS( &pInSensitivityValues ) ));

                                PROPVARIANT pv;
                                PropVariantInit( &pv );
                                // COM type for double
                                pv.vt = VT_R8;
                                pv.dblVal = (double)profile_to_open.sensitivity;
                                pInSensitivityValues->SetValue(
                                    SENSOR_DATA_TYPE_ANGULAR_VELOCITY_X_DEGREES_PER_SECOND,
                                    &pv );
                                pInSensitivityValues->SetValue(
                                    SENSOR_DATA_TYPE_ANGULAR_VELOCITY_Y_DEGREES_PER_SECOND,
                                    &pv );
                                pInSensitivityValues->SetValue(
                                    SENSOR_DATA_TYPE_ANGULAR_VELOCITY_Z_DEGREES_PER_SECOND,
                                    &pv );
                                // creating IPortableDeviceValues container holding <SENSOR_PROPERTY_CHANGE_SENSITIVITY,pInSensitivityValues> tuple
                                Microsoft::WRL::ComPtr<IPortableDeviceValues> pInValues;
                                CHECK_HR(CoCreateInstance( CLSID_PortableDeviceValues,
                                                            nullptr,
                                                            CLSCTX_INPROC_SERVER,
                                                            IID_PPV_ARGS( &pInValues ) ));

                                pInValues->SetIPortableDeviceValuesValue( SENSOR_PROPERTY_CHANGE_SENSITIVITY,
                                                                            pInSensitivityValues.Get() );

                                Microsoft::WRL::ComPtr<IPortableDeviceValues> pOutValues;
                                // set sensitivity
                                hr = connected_sensor->get_sensor()->SetProperties( pInValues.Get(), &pOutValues );
                                if( SUCCEEDED( hr ) )
                                    PropVariantClear( &pv );
                            }

                        }
                    }
                }
            }
            catch (...)
            {
                for (auto& connected_sensor : _connected_sensors)
                {
                    connected_sensor.reset();
                }
                _connected_sensors.clear();
                LOG_ERROR("Hid device is busy!");
                throw;
            }
        }

        void wmf_hid_device::close()
        {
            for (auto& open_sensor : _opened_sensors)
            {
                open_sensor.reset();
            }
            _opened_sensors.clear();
        }

        void wmf_hid_device::start_capture(hid_callback callback)
        {
            // Hack, start default profile
            _cb = Microsoft::WRL::Make<sensor_events>(callback, _gyro_scale_factor);

            for (auto& sensor : _opened_sensors)
            {
                CHECK_HR(sensor->start_capture(_cb.Get()));
            }
        }

        void wmf_hid_device::stop_capture()
        {
            for (auto& sensor : _opened_sensors)
            {
                sensor->stop_capture();
            }
            _cb = nullptr;
        }

        std::vector<hid_sensor> wmf_hid_device::get_sensors()
        {
            std::vector<hid_sensor> sensors;

            for (auto& sensor : _hid_profiles)
                sensors.push_back({ sensor.sensor_name });

            return sensors;
        }

        std::vector<uint8_t> wmf_hid_device::get_custom_report_data(const std::string & custom_sensor_name, const std::string & report_name, custom_sensor_report_field report_field)
        {
            return std::vector<uint8_t>();
        }

        void wmf_hid_device::set_gyro_scale_factor(double scale_factor) 
        {
            _gyro_scale_factor = scale_factor;
        }

        void wmf_hid_device::foreach_hid_device(std::function<void(hid_device_info, Microsoft::WRL::ComPtr<ISensor>)> action)
        {
            /* Enumerate all HID devices and run action function on each device */
            try
            {
                Microsoft::WRL::ComPtr<ISensorManager> pSensorManager;
                Microsoft::WRL::ComPtr<ISensorCollection> pSensorCollection;
                ULONG sensorCount = 0;
                HRESULT res{};

                CHECK_HR(CoCreateInstance(CLSID_SensorManager, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pSensorManager)));

                /* Retrieves a collection containing all sensors associated with category SENSOR_CATEGORY_ALL */
                res=pSensorManager->GetSensorsByCategory(SENSOR_CATEGORY_ALL, &pSensorCollection);
                if (SUCCEEDED(res))
                {
                    /* Retrieves the count of sensors in the collection */
                    CHECK_HR(pSensorCollection->GetCount(&sensorCount));

                    for (ULONG i = 0; i < sensorCount; i++)
                    {
                        Microsoft::WRL::ComPtr<ISensor> pSensor;
                        /* Retrieves the sensor at the specified index in the collection */
                        if (SUCCEEDED(pSensorCollection->GetAt(i, &pSensor)))
                        {
                            /* Retrieve SENSOR_PROPERTY_FRIENDLY_NAME which is the sensor name that is intended to be seen by the user */
                            std::string sensor_id;
                            {
                                BSTR fName;
                                LOG_HR( res = pSensor->GetFriendlyName( &fName ) );
                                if( FAILED( res ) )
                                    sensor_id = "Unidentified HID sensor";
                                else
                                {
                                    sensor_id = rsutils::string::windows::win_to_utf( fName );
                                    SysFreeString( fName );
                                }
                            }

                            /* Retrieve SENSOR_PROPERTY_PERSISTENT_UNIQUE_ID which is a GUID that uniquely identifies the sensor on the current computer */
                            SENSOR_ID id{};
                            CHECK_HR(pSensor->GetID(&id));

                            /* Retrieve sensor type - Sensor types are more specific groupings than sensor categories. Sensor type IDs are GUIDs that are defined in Sensors.h */
                            SENSOR_TYPE_ID type{};
                            CHECK_HR(pSensor->GetType(&type));

                            Microsoft::WRL::ComPtr<IPortableDeviceValues> pValues = nullptr;  // Output
                            hid_device_info info{};

                            /* Retrieves multiple sensor properties */
                            auto hr = pSensor->GetProperties(nullptr, &pValues);
                            if (SUCCEEDED(hr))
                            {
                                /* Get the number of property returned */
                                DWORD propertyCount = 0;
                                hr = pValues->GetCount(&propertyCount);
                                if (SUCCEEDED(hr))
                                {
                                    PROPERTYKEY propertyKey;
                                    PROPVARIANT propertyValue = {};

                                    /* Loop through the properties */
                                    for (DWORD properyIndex = 0; properyIndex < propertyCount; properyIndex++)
                                    {
                                        // Get the value at the current index.
                                        hr = pValues->GetAt(properyIndex, &propertyKey, &propertyValue);
                                        if (SUCCEEDED(hr))
                                        {
                                            if (IsEqualPropertyKey(propertyKey, SENSOR_PROPERTY_DEVICE_PATH))
                                            {
                                                info.device_path = rsutils::string::windows::win_to_utf( propertyValue.pwszVal );
                                                info.id = sensor_id;

                                                uint16_t vid, pid, mi;
                                                std::string uid, guid;
                                                if (parse_usb_path_multiple_interface(vid, pid, mi, uid, info.device_path, guid))
                                                {
                                                    auto node = cm_node::from_device_path( propertyValue.pwszVal );
                                                    if( node.valid() )
                                                    {
                                                        // We take the "unique id" (really, the composite ID used to associate all the devices belonging to
                                                        // a single composite device) of the PARENT of the HID device:
                                                        //     17 USB\VID_8086&PID_0B4D\012345678901 "USB Composite Device"
                                                        //         18 USB\VID_8086&PID_0B4D&MI_00\6&CB1C340&0&0000 "Intel(R) RealSense(TM) Depth Camera 465  Depth"
                                                        //         19 USB\VID_8086&PID_0B4D&MI_03\6&CB1C340&0&0003 "Intel(R) RealSense(TM) Depth Camera 465  RGB"
                                                        //         20 USB\VID_8086&PID_0B4D&MI_05\6&CB1C340&0&0005 "USB Input Device"
                                                        //             21 HID\VID_8086&PID_0B4D&MI_05\7&24FD3503&0&0000 "HID Sensor Collection V2"
                                                        //         22 USB\VID_8086&PID_0B4D&MI_06\6&CB1C340&0&0006 "Intel(R) RealSense(TM) Depth Camera 465 "
                                                        // (the first number is the CM DEVINST handle for each node)
                                                        // Note that all the USB devices have the same "CB1C340" ID, while the HID device is "24FD3503".
                                                        // Because the HID devices are "inside" a USB device parent in the OS's CM tree, we can try to get the
                                                        // parent UID:
                                                        info.unique_id = node.get_parent().get_uid();
                                                    }
                                                    else
                                                    {
                                                        LOG_WARNING( "Parent for HID device not available: " << info.device_path );
                                                        // Leave it empty: it won't be matched against anything
                                                    }

                                                    info.pid = rsutils::string::from() << std::hex << pid;
                                                    info.vid = rsutils::string::from() << std::hex << vid;
                                                }
                                            }
                                            if (IsEqualPropertyKey(propertyKey, SENSOR_PROPERTY_SERIAL_NUMBER))
                                            {
                                                auto str = rsutils::string::windows::win_to_utf( propertyValue.pwszVal );
                                                std::transform(begin(str), end(str), begin(str), ::tolower);
                                                info.serial_number = str;
                                            }
                                        }

                                        PropVariantClear(&propertyValue);
                                    }
                                }
                            }

                            action(info, pSensor);
                        }
                    }
                }
                // ERROR_NOT_FOUND is normal if no sensors are available
                else if( res != HRESULT_FROM_WIN32( ERROR_NOT_FOUND ) )
                    LOG_HR_STR( "pSensorManager->GetSensorsByCategory(SENSOR_CATEGORY_ALL)", res );
            }
            catch (...)
            {
                LOG_INFO("Could not enumerate HID devices!");
            }
        }
    }
}
