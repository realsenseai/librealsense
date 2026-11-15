// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#if (_MSC_FULL_VER < 180031101)
#error At least Visual Studio 2013 Update 4 is required to compile this backend
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "../types.h"
#include "mf-hid.h"
#include "hid-startup.h"
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
#include <comutil.h>
#include <rsutils/string/from.h>


#pragma comment(lib, "Sensorsapi.lib")
#pragma comment(lib, "PortableDeviceGuids.lib")

// Windows Filetime is represented in 64 - bit number of 100 - nanosecond intervals since midnight Jan 1, 1601
// To convert to the Unix epoch, subtract 116444736000000000LL to reach Jan 1, 1970.
constexpr uint64_t WIN_FILETIME_2_UNIX_SYSTIME = 116444736000000000LL;

namespace librealsense
{
    namespace platform
    {
        class sensor_events : public ISensorEvents
        {
        public:
            virtual ~sensor_events() = default;

            explicit sensor_events( const std::vector<std::string> & names, uint64_t started_at,
                                    hid_callback callback, double gyro_scale_factor, double accel_scale_factor )
                : m_cRef(0), _callback(callback), _gyro_scale_factor(gyro_scale_factor),
                  _accel_scale_factor(accel_scale_factor), _startup(names, started_at) {}

            void wait_for_first_samples()
            {
                // SetEventSink success does not prove that power-on succeeded.
                _startup.wait_for( std::chrono::seconds(5) );
            }

            STDMETHODIMP QueryInterface(REFIID iid, void** ppv)
            {
                if (ppv == NULL)
                {
                    return E_POINTER;
                }
                if (iid == __uuidof(IUnknown))
                {
                    *ppv = static_cast<IUnknown*>(this);
                }
                else if (iid == __uuidof(ISensorEvents))
                {
                    *ppv = static_cast<ISensorEvents*>(this);
                }
                else
                {
                    *ppv = NULL;
                    return E_NOINTERFACE;
                }
                AddRef();
                return S_OK;
            }

            STDMETHODIMP_(ULONG) AddRef()
            {
                return InterlockedIncrement(&m_cRef);
            }

            STDMETHODIMP_(ULONG) Release()
            {
                ULONG count = InterlockedDecrement(&m_cRef);
                if (count == 0)
                {
                    delete this;
                    return 0;
                }
                return count;
            }

            //
            // ISensorEvents methods.
            //

            STDMETHODIMP OnEvent(
                ISensor *pSensor,
                REFGUID eventID,
                IPortableDeviceValues *pEventData)
            {
                HRESULT hr = S_OK;

                // Handle custom events here.

                return hr;
            }

            STDMETHODIMP OnDataUpdated(ISensor *pSensor, ISensorDataReport *report)
            {
                if (NULL == report ||
                    NULL == pSensor)
                {
                    return E_INVALIDARG;
                }

                BSTR        fName{};
                SYSTEMTIME  sys_time;
                FILETIME    file_time;
                const HRESULT timestamp_status = report->GetTimestamp(&sys_time);
                if( FAILED(timestamp_status) ) return timestamp_status;

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

                    // Windows Sensor API reports accel in G (unit exponent already applied);
                    // reconstruct FW counts: 1000.0 for legacy milli-g FW, 100000.0 for 10 micro-g FW
                    // (selected per FW version via set_accel_scale_factor, like the gyro)
                    rawX *= _accel_scale_factor;
                    rawY *= _accel_scale_factor;
                    rawZ *= _accel_scale_factor;
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
                meta_data.header.timestamp = customTimestampLow | (customTimestampHigh < 32);
                // Payload:
                meta_data.report_type.imu_report.header.md_type_id = md_type::META_DATA_HID_IMU_REPORT_ID;
                meta_data.report_type.imu_report.header.md_size = metadata_imu_report_size;
                meta_data.report_type.imu_report.flags = static_cast<uint8_t>( md_hid_imu_attributes::custom_timestamp_attirbute |
                                                                                md_hid_imu_attributes::imu_counter_attribute |
                                                                                md_hid_imu_attributes::usb_counter_attribute);
                meta_data.report_type.imu_report.custom_timestamp = customTimestampLow | (customTimestampHigh < 32);
                meta_data.report_type.imu_report.imu_counter = imu_count;
                meta_data.report_type.imu_report.usb_counter = usb_count;

                data.x = static_cast<int32_t>(rawX);
                data.y = static_cast<int32_t>(rawY);
                data.z = static_cast<int32_t>(rawZ);
                data.ts_low = customTimestampLow;
                data.ts_high = customTimestampHigh;

                pSensor->GetFriendlyName(&fName);
                d.sensor.name = CW2A(fName);
                SysFreeString(fName); // free string after it was copied to sensor data

                d.fo.pixels = &data;
                d.fo.metadata = &meta_data;
                d.fo.metadata_size = metadata_hid_raw_size;
                d.fo.frame_size = sizeof(data);
                d.fo.backend_time = 0;
                if( ! SystemTimeToFileTime(&sys_time, &file_time) )
                    return HRESULT_FROM_WIN32( GetLastError() );
                const uint64_t report_time = ( uint64_t( file_time.dwHighDateTime ) << 32 ) | file_time.dwLowDateTime;
                d.fo.backend_time = ( report_time - WIN_FILETIME_2_UNIX_SYSTIME ) * 0.0001;

                // The caller holds its configure lock until start returns.
                // Forwarding a warm-up callback could block the other sensor.
                if( ! _startup.on_report( d.sensor.name, report_time ) )
                    return S_OK;
                _callback(d);

                return S_OK;
            }

            STDMETHODIMP OnLeave(
                REFSENSOR_ID sensorID)
            {
                HRESULT hr = S_OK;

                // Perform any housekeeping tasks for the sensor that is leaving.
                // For example, if you have maintained a reference to the sensor,
                // release it now and set the pointer to NULL.

                return hr;
            }

            STDMETHODIMP OnStateChanged(
                ISensor* pSensor,
                SensorState state)
            {
                HRESULT hr = S_OK;

                if (NULL == pSensor)
                {
                    return E_INVALIDARG;
                }


                if (state == SENSOR_STATE_READY)
                {
                    wprintf_s(L"\nTime sensor is now ready.");
                }
                else if (state == SENSOR_STATE_ACCESS_DENIED)
                {
                    wprintf_s(L"\nNo permission for the time sensor.\n");
                    wprintf_s(L"Enable the sensor in the control panel.\n");
                }


                return hr;
            }

        private:
            long m_cRef;
            hid_callback _callback;
            double _gyro_scale_factor = 10.0;
            double _accel_scale_factor = 1000.0;
            hid_startup _startup;
        };

        // SetProperties uses S_FALSE for per-property failures, which CHECK_HR
        // alone does not reject. Both supported calls set exactly one property.
        static void set_sensor_property( ISensor * sensor, IPortableDeviceValues * values, REFPROPERTYKEY key )
        {
            CComPtr<IPortableDeviceValues> results;
            HRESULT hr = sensor->SetProperties( values, &results );
            CHECK_HR(hr);
            if( hr == S_FALSE )
            {
                HRESULT property_error;
                CHECK_HR(results->GetErrorValue( key, &property_error ));
                CHECK_HR(property_error);
            }
        }

        void wmf_hid_device::open(const std::vector<hid_profile>&iio_profiles)
        {
            try
            {
                for( auto const & profile : iio_profiles )
                {
                    for( auto const & sensor : _connected_sensors )
                    {
                        if( profile.sensor_name != sensor->get_sensor_name() )
                            continue;
                        CComPtr<IPortableDeviceValues> interval;
                        CHECK_HR(interval.CoCreateInstance( CLSID_PortableDeviceValues ));
                        CHECK_HR(interval->SetUnsignedIntegerValue( SENSOR_PROPERTY_CURRENT_REPORT_INTERVAL,
                                                                  profile.frequency ));
                        set_sensor_property( sensor->get_sensor(), interval, SENSOR_PROPERTY_CURRENT_REPORT_INTERVAL );

                        const bool is_gyro = profile.sensor_name == "HID Sensor Class Device: Gyroscope";
                        const bool is_accel = profile.apply_sensitivity_to_accel
                            && profile.sensor_name == "HID Sensor Class Device: Accelerometer";
                        if( is_gyro || is_accel )
                        {
                            CComPtr<IPortableDeviceValues> axes, properties;
                            CHECK_HR(axes.CoCreateInstance( CLSID_PortableDeviceValues ));
                            CHECK_HR(properties.CoCreateInstance( CLSID_PortableDeviceValues ));
                            PROPVARIANT value{};
                            value.vt = VT_R8;
                            value.dblVal = profile.sensitivity;
                            CHECK_HR(axes->SetValue( is_gyro ? SENSOR_DATA_TYPE_ANGULAR_VELOCITY_X_DEGREES_PER_SECOND
                                                           : SENSOR_DATA_TYPE_ACCELERATION_X_G, &value ));
                            CHECK_HR(axes->SetValue( is_gyro ? SENSOR_DATA_TYPE_ANGULAR_VELOCITY_Y_DEGREES_PER_SECOND
                                                           : SENSOR_DATA_TYPE_ACCELERATION_Y_G, &value ));
                            CHECK_HR(axes->SetValue( is_gyro ? SENSOR_DATA_TYPE_ANGULAR_VELOCITY_Z_DEGREES_PER_SECOND
                                                           : SENSOR_DATA_TYPE_ACCELERATION_Z_G, &value ));
                            CHECK_HR(properties->SetIPortableDeviceValuesValue( SENSOR_PROPERTY_CHANGE_SENSITIVITY, axes ));
                            set_sensor_property( sensor->get_sensor(), properties, SENSOR_PROPERTY_CHANGE_SENSITIVITY );
                        }
                        _opened_sensors.push_back(sensor);
                    }
                }
            }
            catch (...)
            {
                _opened_sensors.clear();
                throw;
            }
        }

        void wmf_hid_device::close()
        {
            // Also owns cleanup if a previous start failed partway through.
            stop_capture();
            _opened_sensors.clear();
        }

        void wmf_hid_device::start_capture(hid_callback callback)
        {
            if( ! _streaming_sensors.empty() )
                throw std::runtime_error( "Previous Windows HID capture is still active" );
            std::vector<std::string> names;
            for( auto const & sensor : _opened_sensors )
            {
                auto const & info = sensor->get_device_info();
                if( hid_startup::is_required( info.vid, info.pid ) )
                    names.push_back( sensor->get_sensor_name() );
            }
            FILETIME now{};
            if( ! names.empty() )
                GetSystemTimePreciseAsFileTime( &now );
            const uint64_t started_at = ( uint64_t(now.dwHighDateTime) << 32 ) | now.dwLowDateTime;
            CComPtr<sensor_events> events = new sensor_events( names, started_at, callback,
                                                             _gyro_scale_factor, _accel_scale_factor );
            _cb = events.p;
            try
            {
                for( auto const & sensor : _opened_sensors )
                {
                    const HRESULT hr = sensor->start_capture(events);
                    CHECK_HR(hr);
                    _streaming_sensors.push_back(sensor);
                }
                events->wait_for_first_samples();
            }
            catch (...)
            {
                stop_capture();
                throw;
            }
        }

        void wmf_hid_device::stop_capture()
        {
            while( ! _streaming_sensors.empty() )
            {
                const HRESULT hr = _streaming_sensors.back()->stop_capture();
                CHECK_HR(hr);
                _streaming_sensors.pop_back();
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

        void wmf_hid_device::set_accel_scale_factor(double scale_factor)
        {
            _accel_scale_factor = scale_factor;
        }

        void wmf_hid_device::foreach_hid_device(std::function<void(hid_device_info, CComPtr<ISensor>)> action)
        {
            /* Enumerate all HID devices and run action function on each device */
            try
            {
                CComPtr<ISensorManager> pSensorManager = nullptr;
                CComPtr<ISensorCollection> pSensorCollection = nullptr;
                CComPtr<ISensor> pSensor = nullptr;
                ULONG sensorCount = 0;
                HRESULT res{};

                CHECK_HR(CoCreateInstance(CLSID_SensorManager, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pSensorManager)));

                /* Retrieves a collection containing all sensors associated with category SENSOR_CATEGORY_ALL */
                res=pSensorManager->GetSensorsByCategory(SENSOR_CATEGORY_ALL, &pSensorCollection);
                if (SUCCEEDED(res))
                {
                    /* Retrieves the count of sensors in the collection */
                    CHECK_HR(pSensorCollection->GetCount(&sensorCount));

                    for (ULONG i = 0; i < sensorCount; i++)
                    {
                        /* Retrieves the sensor at the specified index in the collection */
                        if (SUCCEEDED(pSensorCollection->GetAt(i, &pSensor.p)))
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

                            CComPtr<IPortableDeviceValues> pValues = nullptr;  // Output
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
                        //Releasing resources
                        safe_release(pSensor);
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
