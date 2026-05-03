// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#include "mf-backend.h"
#include "mf-uvc.h"
#include "mf-hid.h"
#include <src/core/time-service.h>
#include <src/platform/device-watcher.h>
#include <src/platform/command-transfer.h>
#include "usb/usb-device.h"
#include "usb/usb-enumerator.h"
#include "../types.h"
#include <mfapi.h>
#include <chrono>
#include <Windows.h>
#include <dbt.h>
#include <cctype> // std::tolower
#include <rsutils/time/timer.h>
#include <rsutils/string/windows.h>

namespace {

void debug_dev_broadcast( DEV_BROADCAST_HDR const * p_hdr, char const * context )
{
    switch( p_hdr->dbch_devicetype )
    {
    case DBT_DEVTYP_DEVICEINTERFACE: {
        auto p_actual = reinterpret_cast< DEV_BROADCAST_DEVICEINTERFACE const * >( p_hdr );
        std::string name = p_actual->dbcc_name ? rsutils::string::windows::win_to_utf( p_actual->dbcc_name ) : std::string();
        LOG_DEBUG( "device change event: " << context << ": DEVICEINTERFACE: \""
                                           << name << "\"" );
        break;
    }
    case DBT_DEVTYP_HANDLE: {
        auto p_actual = reinterpret_cast< DEV_BROADCAST_HANDLE const * >( p_hdr );
        LOG_DEBUG( "device change event: " << context << ": HANDLE: file system handle 0x"
                                           << std::hex << p_actual->dbch_handle );
        break;
    }
    case DBT_DEVTYP_OEM: {
        auto p_actual = reinterpret_cast< DEV_BROADCAST_OEM const * >( p_hdr );
        LOG_DEBUG( "device change event: " << context << ": OEM: identifier 0x" << std::hex
                                           << p_actual->dbco_identifier );
        break;
    }
    case DBT_DEVTYP_PORT: {
        auto p_actual = reinterpret_cast< DEV_BROADCAST_PORT const * >( p_hdr );
        std::string name = p_actual->dbcp_name ? rsutils::string::windows::win_to_utf( p_actual->dbcp_name ) : std::string();
        LOG_DEBUG( "device change event: " << context << ": PORT: \"" << name
                                           << "\"" );
        break;
    }
    case DBT_DEVTYP_VOLUME: {
        auto p_actual = reinterpret_cast< DEV_BROADCAST_VOLUME const * >( p_hdr );
        LOG_DEBUG( "device change event: " << context << ": VOLUME" );
        break;
    }
    default:
        LOG_DEBUG( "device change event: " << context << ": UNKNOWN (dbch_devicetype= "
                                           << p_hdr->dbch_devicetype << ")" );
        break;
    }
}

}

namespace librealsense
{
    namespace platform
    {
        wmf_backend::wmf_backend()
        {
            // In applications that have COM initializations on other threads using
            // COINIT_APARTMENTTHREADED (like the Qt framework, for example), using
            // COINIT_MULTITHREADED can lead to a deadlock inside COM functions.
#ifdef COM_MULTITHREADED
            CoInitializeEx(nullptr, COINIT_MULTITHREADED); // when using COINIT_APARTMENTTHREADED, calling _pISensor->SetEventSink(NULL) to stop sensor can take several seconds
#else
            CoInitializeEx( nullptr, COINIT_APARTMENTTHREADED ); // Apartment model
#endif

            MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
        }

        wmf_backend::~wmf_backend()
        {
            try {
                MFShutdown();
                CoUninitialize();
            }
            catch(...)
            {
                // TODO: Write to log
            }
        }

        std::shared_ptr<uvc_device> wmf_backend::create_uvc_device(uvc_device_info info) const
        {
            return std::make_shared<retry_controls_work_around>(
                            std::make_shared<wmf_uvc_device>(info, shared_from_this()));
        }

        std::shared_ptr<backend> create_backend()
        {
            return std::make_shared<wmf_backend>();
        }

        std::vector<uvc_device_info> wmf_backend::query_uvc_devices() const
        {
            std::vector<uvc_device_info> devices;

            auto action = [&devices, this](const uvc_device_info& info, IMFActivate*)
            {
                uvc_device_info device_info = info;
                device_info.serial = this->get_device_serial(info.vid, info.pid, info.unique_id);
                devices.push_back(device_info);
            };

            wmf_uvc_device::foreach_uvc_device(action);

            return devices;
        }

        std::shared_ptr<command_transfer> wmf_backend::create_usb_device(usb_device_info info) const
        {
            auto dev = usb_enumerator::create_usb_device(info);
            if(dev)
                return std::make_shared<platform::command_transfer_usb>(dev);
            return nullptr;
        }

        std::vector<usb_device_info> wmf_backend::query_usb_devices() const
        {
            auto device_infos = usb_enumerator::query_devices_info();
            return device_infos;
        }

        wmf_hid_device::wmf_hid_device(const hid_device_info& info,
                                       std::shared_ptr<const wmf_backend> backend)
            : _backend(std::move(backend)),
              _cb(nullptr)
        {
            bool found = false;

            wmf_hid_device::foreach_hid_device([&](const hid_device_info& hid_dev_info, Microsoft::WRL::ComPtr<ISensor> sensor) {
                if (hid_dev_info.unique_id == info.unique_id)
                {
                    _connected_sensors.push_back(std::make_shared<wmf_hid_sensor>(hid_dev_info, sensor));
                    found = true;
                }
            });

            if (!found)
            {
                LOG_ERROR("hid device is no longer connected!");
            }
        }

        std::shared_ptr<hid_device> wmf_backend::create_hid_device(hid_device_info info) const
        {
            return std::make_shared<wmf_hid_device>(info, shared_from_this());
        }

        std::vector<hid_device_info> wmf_backend::query_hid_devices() const
        {
            std::vector<hid_device_info> devices;

            auto action = [&devices](const hid_device_info& info, Microsoft::WRL::ComPtr<ISensor>)
            {
                devices.push_back(info);
            };

            wmf_hid_device::foreach_hid_device(action);

            return devices;
        }

        std::vector<mipi_device_info> wmf_backend::query_mipi_devices() const
        {
            return std::vector<mipi_device_info>();
        }

        class win_event_device_watcher : public device_watcher
        {
        public:
            win_event_device_watcher(const backend * backend)
                : _backend( backend )
            {
            }
            ~win_event_device_watcher() { stop(); }

            void start(device_changed_callback callback) override
            {
                std::lock_guard<std::mutex> lock(_m);
                if( ! _data._stopped )
                    throw wrong_api_call_sequence_exception(
                        "Cannot start a running device_watcher" );
                LOG_DEBUG( "starting win_event_device_watcher" );
                _data._stopped = false;
                _callback = std::move(callback);
                _last = backend_device_group( _backend->query_uvc_devices(),
                                              _backend->query_usb_devices(),
                                              _backend->query_hid_devices() );
                _thread = std::thread([this]() { run(); });
            }

            void stop() override
            {
                std::lock_guard<std::mutex> lock(_m);
                if (!_data._stopped)
                {
                    LOG_DEBUG( "stopping win_event_device_watcher" );
                    _data._stopped = true;
                    if (_thread.joinable()) _thread.join();
                }
            }

            bool is_stopped() const override
            {
                return _data._stopped;
            }

        private:
            std::thread _thread;
            std::mutex _m;
            backend_device_group _last;
            device_changed_callback _callback;
            const backend * const _backend;

            struct extra_data {
                rsutils::time::timer _timer{ std::chrono::milliseconds( 100 ) };

                bool _stopped = true;
                bool _changed = false;
                HWND hWnd = nullptr;
                // One handle per registered DEVICEINTERFACE; previously a single
                // hdevnotify_sensor field was reused for both sensor-camera and HID
                // registrations, which leaked the first handle.
                HDEVNOTIFY hdevnotifyHW = nullptr;
                HDEVNOTIFY hdevnotifyUVC = nullptr;
                HDEVNOTIFY hdevnotifySensorCamera = nullptr;
                HDEVNOTIFY hdevnotifyHID = nullptr;
                HDEVNOTIFY hdevnotifyUSB = nullptr;
            } _data;

            void run()
            {
                WNDCLASS windowClass = {};
                LPCWSTR SzWndClass = TEXT("MINWINAPP");
                windowClass.lpfnWndProc = &on_win_event;
                windowClass.lpszClassName = SzWndClass;
                UnregisterClass(SzWndClass, nullptr);

                if (!RegisterClass(&windowClass))
                    LOG_WARNING("RegisterClass failed.");

                _data.hWnd = CreateWindow(SzWndClass, nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, &_data);
                if (!_data.hWnd)
                    throw winapi_error("CreateWindow failed");

                MSG msg;

                while (!_data._stopped)
                {
                    if (PeekMessage(&msg, _data.hWnd, 0, 0, PM_REMOVE))
                    {
                        TranslateMessage( &msg );
                        DispatchMessage( &msg );
                    }
                    else
                    {
                        if( _data._changed && _data._timer.has_expired() )
                        {
                            platform::backend_device_group curr( _backend->query_uvc_devices(),
                                                                 _backend->query_usb_devices(),
                                                                 _backend->query_hid_devices() );
                            if( list_changed( _last.uvc_devices, curr.uvc_devices )
                                || list_changed( _last.usb_devices, curr.usb_devices )
                                || list_changed( _last.hid_devices, curr.hid_devices ) )
                            {
                                _callback( _last, curr );
                                _last = curr;
                            }
                            _data._changed = false;
                        }
                        // Yield CPU resources, as this is required for connect/disconnect events only
                        std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
                    }
                }

                if (_data.hdevnotifyHW)            UnregisterDeviceNotification(_data.hdevnotifyHW);
                if (_data.hdevnotifyUVC)           UnregisterDeviceNotification(_data.hdevnotifyUVC);
                if (_data.hdevnotifySensorCamera)  UnregisterDeviceNotification(_data.hdevnotifySensorCamera);
                if (_data.hdevnotifyHID)           UnregisterDeviceNotification(_data.hdevnotifyHID);
                if (_data.hdevnotifyUSB)           UnregisterDeviceNotification(_data.hdevnotifyUSB);
                DestroyWindow(_data.hWnd);
            }

            static LRESULT CALLBACK on_win_event(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
            {
                LRESULT lRet = 1;

                switch (message)
                {
                case WM_CREATE:
                {
                    SetWindowLongPtr(hWnd, GWLP_USERDATA, LONG_PTR(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams));
                    if (!DoRegisterDeviceInterfaceToHwnd(hWnd))
                    {
                        auto data = reinterpret_cast<extra_data*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                        data->_stopped = true;
                    }
                    break;
                }
                case WM_QUIT:
                {
                    auto data = reinterpret_cast<extra_data*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                    data->_stopped = true;
                    break;
                }
                case WM_DEVICECHANGE:
                {
                    //PDEV_BROADCAST_DEVICEINTERFACE b = (PDEV_BROADCAST_DEVICEINTERFACE)lParam;
                    // Output some messages to the window.
                    switch (wParam)
                    {
                    case DBT_DEVICEARRIVAL: {
                        // The system broadcasts the DBT_DEVICEARRIVAL device event when a device or
                        // piece of media has been inserted and becomes available.
                        auto p_hdr = reinterpret_cast< DEV_BROADCAST_HDR const * >( lParam );
                        debug_dev_broadcast( p_hdr, "arrival" );
                        if( p_hdr->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE )
                            break;
                        auto data = reinterpret_cast< extra_data * >(
                            GetWindowLongPtr( hWnd, GWLP_USERDATA ) );
                        data->_changed = true;
                        data->_timer.start();
                        break;
                    }
                    case DBT_DEVICEREMOVECOMPLETE: {
                        // A device or piece of media has been physically removed
                        auto p_hdr = reinterpret_cast< DEV_BROADCAST_HDR const * >( lParam );
                        debug_dev_broadcast( p_hdr, "remove complete" );
                        if( p_hdr->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE )
                            break;
                        auto data = reinterpret_cast<extra_data*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                        data->_changed = true;
                        data->_timer.start();
                    }
                        break;
                    }
                    break;
                }

                default:
                    // Send all other messages on to the default windows handler.
                    lRet = DefWindowProc(hWnd, message, wParam, lParam);
                    break;
                }

                return lRet;
            }

            static BOOL DoRegisterDeviceInterfaceToHwnd(HWND hWnd)
            {
                auto data = reinterpret_cast<extra_data*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

                auto register_interface = [hWnd]( REFGUID classGuid ) -> HDEVNOTIFY
                {
                    DEV_BROADCAST_DEVICEINTERFACE filter = {};
                    filter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
                    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
                    filter.dbcc_classguid = classGuid;
                    return RegisterDeviceNotification( hWnd, &filter, DEVICE_NOTIFY_WINDOW_HANDLE );
                };

                auto unregister_succeeded = [data]()
                {
                    if (data->hdevnotifyHW)            { UnregisterDeviceNotification(data->hdevnotifyHW);            data->hdevnotifyHW = nullptr; }
                    if (data->hdevnotifyUVC)           { UnregisterDeviceNotification(data->hdevnotifyUVC);           data->hdevnotifyUVC = nullptr; }
                    if (data->hdevnotifySensorCamera)  { UnregisterDeviceNotification(data->hdevnotifySensorCamera);  data->hdevnotifySensorCamera = nullptr; }
                    if (data->hdevnotifyHID)           { UnregisterDeviceNotification(data->hdevnotifyHID);           data->hdevnotifyHID = nullptr; }
                };

                // HW monitor (private RealSense interface)
                static const GUID hwMonitorGuid = { 0x175695cd, 0x30d9, 0x4f87, { 0x8b, 0xe3, 0x5a, 0x82, 0x70, 0xf4, 0x9a, 0x31 } };
                data->hdevnotifyHW = register_interface( hwMonitorGuid );
                if (!data->hdevnotifyHW)
                {
                    LOG_WARNING("Register HW events failed");
                    return FALSE;
                }

                // UVC video capture
                data->hdevnotifyUVC = register_interface( KSCATEGORY_CAPTURE );
                if (!data->hdevnotifyUVC)
                {
                    LOG_WARNING("Register UVC events failed");
                    unregister_succeeded();
                    return FALSE;
                }

                // UVC sensor-camera (Win10+ depth/IR)
                data->hdevnotifySensorCamera = register_interface( KSCATEGORY_SENSOR_CAMERA );
                if (!data->hdevnotifySensorCamera)
                {
                    LOG_WARNING("Register sensor-camera events failed");
                    unregister_succeeded();
                    return FALSE;
                }

                // HID sensors (IMU)
                static const GUID GUID_DEVINTERFACE_HID = { 0x4d1e55b2, 0xf16f, 0x11cf, { 0x88, 0xcb, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 } };
                data->hdevnotifyHID = register_interface( GUID_DEVINTERFACE_HID );
                if (!data->hdevnotifyHID)
                {
                    LOG_WARNING("Register HID events failed");
                    unregister_succeeded();
                    return FALSE;
                }

                // FW Update device (USB device class)
                static const GUID usbClassGuid = { 0xa5dcbf10, 0x6530, 0x11d2, { 0x90, 0x1f, 0x00, 0xc0, 0x4f, 0xb9, 0x51, 0xed } };
                data->hdevnotifyUSB = register_interface( usbClassGuid );
                if (!data->hdevnotifyUSB)
                {
                    LOG_WARNING("Register USB events failed");
                    unregister_succeeded();
                    return FALSE;
                }

                return TRUE;
            }
        };

        std::shared_ptr<device_watcher> wmf_backend::create_device_watcher() const
        {
            return std::make_shared<win_event_device_watcher>(this);
        }

        std::string wmf_backend::get_device_serial(uint16_t device_vid, uint16_t device_pid, const std::string& device_uid) const
        {
            std::string device_serial = "";
            std::string location = "";
            usb_spec spec = usb_undefined;

            platform::get_usb_descriptors(device_vid, device_pid, device_uid, location, spec, device_serial);

            return device_serial;
        }
    }
}
