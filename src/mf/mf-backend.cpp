// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.
#if (_MSC_FULL_VER < 180031101)
    #error At least Visual Studio 2013 Update 4 is required to compile this backend
#endif

#include "mf-backend.h"
#include "mf-uvc.h"
#include "mf-hid.h"
#include "../win/win-helpers.h"  // cm_node
#include <src/core/time-service.h>
#include <src/platform/device-watcher.h>
#include <src/platform/command-transfer.h>
#include "usb/usb-device.h"
#include "usb/usb-enumerator.h"
#include "../types.h"
#include <mfapi.h>
#include <algorithm>
#include <chrono>
#include <map>
#include <set>
#include <Windows.h>
#include <dbt.h>
#include <devpkey.h>  // DEVPKEY_Device_Class
#include <cctype> // std::tolower
#include <rsutils/time/timer.h>

namespace {

static inline std::string utf8_from_wchar( const wchar_t* w )
{
    if( !w ) return {};
    int size_needed = WideCharToMultiByte( CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL );
    if( size_needed <= 0 ) return {};
    std::string str( size_needed - 1, '\0' );
    WideCharToMultiByte( CP_UTF8, 0, w, -1, &str[0], size_needed, NULL, NULL );
    return str;
}

void debug_dev_broadcast( DEV_BROADCAST_HDR const * p_hdr, char const * context )
{
    switch( p_hdr->dbch_devicetype )
    {
    case DBT_DEVTYP_DEVICEINTERFACE: {
        auto p_actual = reinterpret_cast< DEV_BROADCAST_DEVICEINTERFACE const * >( p_hdr );
        std::string name = utf8_from_wchar( p_actual->dbcc_name );
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
        std::string name = utf8_from_wchar( p_actual->dbcp_name );
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

            wmf_hid_device::foreach_hid_device([&](const hid_device_info& hid_dev_info, CComPtr<ISensor> sensor) {
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

            auto action = [&devices](const hid_device_info& info, CComPtr<ISensor>)
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

        // A device interface path ("\\?\USB#VID_x&PID_y&MI_00#inst#{guid}") and the
        // WM_DEVICECHANGE broadcast for the same interface carry different interface
        // GUIDs, so they only compare equal on the device-instance part. Lowercased
        // because Windows is not consistent about case.
        static std::string instance_id_key( LPCWSTR device_path )
        {
            std::string key = utf8_from_wchar( instance_id_from_device_path( device_path ).c_str() );
            std::transform( key.begin(), key.end(), key.begin(),
                            []( unsigned char c ) { return (char)std::tolower( c ); } );
            return key;
        }

        static std::string instance_id_key( std::string const & device_path )
        {
            std::wstring wide( device_path.begin(), device_path.end() );
            return instance_id_key( wide.c_str() );
        }

        // Drops the composites whose camera interfaces Windows reported as removed,
        // taking their UVC and HID entries with them. Returns true if anything was
        // dropped.
        //
        // Composite granularity on purpose: a camera that goes away always takes its
        // camera interfaces with it, so those are the reliable signal. Acting on a lone
        // HID removal instead would drop just the IMU and republish the very partial
        // device this watcher exists to avoid.
        //
        // USB entries are deliberately left alone here - see the caller, which re-reads
        // them instead of trying to match them by id.
        static bool drop_removed_composites( platform::backend_device_group & group,
                                             std::set< std::string > const & removed_instance_ids )
        {
            std::set< std::string > gone_uids;
            for( auto && uvc : group.uvc_devices )
                if( removed_instance_ids.count( instance_id_key( uvc.device_path ) ) )
                    gone_uids.insert( uvc.unique_id );
            if( gone_uids.empty() )
                return false;

            auto drop_from = [&]( auto & devices )
            {
                devices.erase( std::remove_if( devices.begin(), devices.end(),
                                               [&]( auto const & device )
                                               { return gone_uids.count( device.unique_id ) > 0; } ),
                               devices.end() );
            };
            drop_from( group.uvc_devices );
            drop_from( group.hid_devices );
            return true;
        }

        // Returns the unique_ids of USB composites still mid-enumeration, so an arrival
        // can wait rather than publish a device that comes up missing sensors. The
        // composite's device-tree children come from its USB configuration descriptor,
        // which makes them the authoritative set to expect.
        static std::set< std::string > incomplete_composites( platform::backend_device_group const & curr )
        {
            std::set< std::string > sensor_api_uids;
            for( auto && h : curr.hid_devices )
                sensor_api_uids.insert( h.unique_id );

            // Each USB composite shows up as the PARENT of any of its MI_xx interfaces.
            // We discover composites via the UVC entries and remember which of their
            // interface nodes Media Foundation has already given us.
            struct composite_info
            {
                std::string unique_id;
                std::set< DEVINST > surfaced_uvc_nodes;
            };
            std::map< DEVINST, composite_info > composites;
            for( auto && uvc : curr.uvc_devices )
            {
                std::wstring path( uvc.device_path.begin(), uvc.device_path.end() );
                cm_node iface = cm_node::from_device_path( path.c_str() );
                if( ! iface.valid() )
                    continue;
                cm_node composite = iface.get_parent();
                if( ! composite.valid() )
                    continue;
                composite_info & info = composites[composite.get()];
                info.unique_id = uvc.unique_id;
                info.surfaced_uvc_nodes.insert( iface.get() );
            }

            std::set< std::string > incomplete;
            for( auto const & entry : composites )
            {
                composite_info const & info = entry.second;
                cm_node child = cm_node( entry.first ).get_child();
                while( child.valid() )
                {
                    // DEVPKEY_Device_Class is the human-readable class name assigned by
                    // Windows ("Camera", "HIDClass", "Ports", ...).
                    std::string const device_class = child.get_property( DEVPKEY_Device_Class );
                    if( device_class == "Camera" )
                    {
                        if( ! info.surfaced_uvc_nodes.count( child.get() ) )
                        {
                            incomplete.insert( info.unique_id );
                            break;
                        }
                    }
                    else if( device_class == "HIDClass" )
                    {
                        cm_node hid_instance = child.get_child();
                        if( ! hid_instance.valid() )
                        {
                            // Nothing attached under the HID interface yet - the OS is
                            // still binding a driver to it.
                            incomplete.insert( info.unique_id );
                            break;
                        }
                        // Only a HID Sensor Collection surfaces through the Sensor API,
                        // and Windows gives it its own "Sensor" device class. Other HID
                        // interfaces - vendor-defined controls on unrelated cameras, for
                        // instance - never appear there, so waiting on them would defer
                        // every device on the machine.
                        bool is_sensor_collection = false;
                        for( cm_node node = hid_instance; node.valid(); node = node.get_sibling() )
                        {
                            if( node.get_property( DEVPKEY_Device_Class ) == "Sensor" )
                            {
                                is_sensor_collection = true;
                                break;
                            }
                        }
                        if( is_sensor_collection && ! sensor_api_uids.count( info.unique_id ) )
                        {
                            incomplete.insert( info.unique_id );
                            break;
                        }
                    }
                    child = child.get_sibling();
                }
            }
            return incomplete;
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
                _data._changed = false;
                _data._incomplete_since.clear();
                _data._removed_instance_ids.clear();
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
                // When each still-enumerating composite was first seen that way, so a
                // composite that never finishes binding is waited on once, not forever
                // (see incomplete_composites).
                std::map< std::string, std::chrono::steady_clock::time_point > _incomplete_since;
                // Device interfaces Windows reported removed since the last callback. Only
                // touched from the watcher thread, which is also the one pumping messages.
                std::set< std::string > _removed_instance_ids;

                bool _stopped = true;
                bool _changed = false;
                HWND hWnd;
                HDEVNOTIFY hdevnotifyHW, hdevnotifyUVC, hdevnotify_sensor, hdevnotifyUSB;
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
                            auto changed = []( platform::backend_device_group const & from,
                                               platform::backend_device_group const & to )
                            {
                                return list_changed( from.uvc_devices, to.uvc_devices )
                                    || list_changed( from.usb_devices, to.usb_devices )
                                    || list_changed( from.hid_devices, to.hid_devices );
                            };

                            // Removals first, computed from what we already know: the
                            // notification names the interface and _last says which device
                            // owned it, so no enumeration is needed. That matters - a full
                            // enumeration costs several seconds while a camera is missing,
                            // because the Sensor API stalls on the device that just left, and
                            // the application should not keep a device with dead handles for
                            // that long.
                            //
                            // It is also the only way to notice a camera that reboots - after
                            // a firmware update or a hardware reset it returns on the same
                            // device paths, so comparing snapshots cannot tell it apart from
                            // one that never left.
                            if( ! _data._removed_instance_ids.empty() )
                            {
                                platform::backend_device_group without_removed = _last;
                                if( drop_removed_composites( without_removed, _data._removed_instance_ids ) )
                                {
                                    // The USB list is re-read rather than filtered. A composite
                                    // contributes USB entries under two different ids - its MI_xx
                                    // interfaces share the instance token the UVC entries use,
                                    // while its own entry is parsed from a path with no MI_ part
                                    // and carries an unrelated id - so matching them by id leaves
                                    // some behind, and a leftover would show up as a change on the
                                    // next tick and fire a second, spurious callback. Unlike the
                                    // HID query, this one costs about a millisecond even while a
                                    // camera is missing, so asking the OS is cheaper than guessing.
                                    without_removed.usb_devices = _backend->query_usb_devices();
                                    _callback( _last, without_removed );
                                    _last = without_removed;
                                }
                                _data._removed_instance_ids.clear();
                            }

                            // Arrivals do need a snapshot. Queried in explicit statements,
                            // not as constructor arguments: argument evaluation order is
                            // unspecified and MSVC picks right-to-left, which left the UVC
                            // list - the one carrying device identity - enumerated last.
                            auto uvc_devices = _backend->query_uvc_devices();
                            auto usb_devices = _backend->query_usb_devices();
                            auto hid_devices = _backend->query_hid_devices();
                            platform::backend_device_group curr( uvc_devices, usb_devices, hid_devices );

                            // Arrivals, on the other hand, wait: a composite still growing
                            // camera/HID interfaces is not ready to be published, so re-arm
                            // the debounce and look again on the next tick. Each composite
                            // gets its own budget, so one that advertises an interface it
                            // never binds delays us once instead of blocking every later
                            // event on the machine - and once the budget is spent we publish
                            // whatever exists, which is what lets a genuinely partial device
                            // through.
                            //
                            // 8s covers the widest interface spread measured on a D585 (4.6s
                            // cold plug, 6.5s from a premature publish to the complete one
                            // after a firmware update) without making a real partial device
                            // wait any longer than it has to.
                            static constexpr auto MAX_DEFERRAL = std::chrono::seconds( 8 );
                            auto const now = std::chrono::steady_clock::now();
                            auto const incomplete = incomplete_composites( curr );
                            for( auto it = _data._incomplete_since.begin(); it != _data._incomplete_since.end(); )
                            {
                                if( incomplete.count( it->first ) )
                                    ++it;
                                else
                                    it = _data._incomplete_since.erase( it );
                            }
                            bool may_defer = false;
                            for( auto && unique_id : incomplete )
                            {
                                auto inserted = _data._incomplete_since.emplace( unique_id, now );
                                if( now - inserted.first->second < MAX_DEFERRAL )
                                    may_defer = true;
                            }
                            if( may_defer )
                            {
                                _data._timer.start();
                                // Don't publish yet; fall through to sleep.
                            }
                            else
                            {
                                if( changed( _last, curr ) )
                                {
                                    _callback( _last, curr );
                                    _last = curr;
                                }
                                _data._changed = false;
                            }
                        }
                        // Yield CPU resources, as this is required for connect/disconnect events only
                        std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
                    }
                }

                UnregisterDeviceNotification(_data.hdevnotifyHW);
                UnregisterDeviceNotification(_data.hdevnotifyUVC);
                UnregisterDeviceNotification(_data.hdevnotify_sensor);
                DestroyWindow(_data.hWnd);
            }

            static LRESULT CALLBACK on_win_event(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
            {
                LRESULT lRet = 1;

                switch (message)
                {
                case WM_CREATE:
                    SetWindowLongPtr(hWnd, GWLP_USERDATA, LONG_PTR(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams));
                    if (!DoRegisterDeviceInterfaceToHwnd(hWnd))
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
                        auto p_iface = reinterpret_cast< DEV_BROADCAST_DEVICEINTERFACE const * >( lParam );
                        auto instance_id = instance_id_key( p_iface->dbcc_name );
                        if( ! instance_id.empty() )
                            data->_removed_instance_ids.insert( std::move( instance_id ) );
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

                //===========================register HWmonitor events==============================
                const GUID classGuid = { 0x175695cd, 0x30d9, 0x4f87, 0x8b, 0xe3, 0x5a, 0x82, 0x70, 0xf4, 0x9a, 0x31 };
                DEV_BROADCAST_DEVICEINTERFACE devBroadcastDeviceInterface;
                devBroadcastDeviceInterface.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
                devBroadcastDeviceInterface.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
                devBroadcastDeviceInterface.dbcc_classguid = classGuid;
                devBroadcastDeviceInterface.dbcc_reserved = 0;

                data->hdevnotifyHW = RegisterDeviceNotification(hWnd,
                    &devBroadcastDeviceInterface,
                    DEVICE_NOTIFY_WINDOW_HANDLE);
                if (data->hdevnotifyHW == NULL)
                {
                    LOG_WARNING("Register HW events Failed!\n");
                    return FALSE;
                }

                ////===========================register UVC events==============================
                DEV_BROADCAST_DEVICEINTERFACE di = { 0 };
                di.dbcc_size = sizeof(di);
                di.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
                di.dbcc_classguid = KSCATEGORY_CAPTURE;

                data->hdevnotifyUVC = RegisterDeviceNotification(hWnd,
                    &di,
                    DEVICE_NOTIFY_WINDOW_HANDLE);
                if (data->hdevnotifyUVC == nullptr)
                {
                    UnregisterDeviceNotification(data->hdevnotifyHW);
                    LOG_WARNING("Register UVC events Failed!\n");
                    return FALSE;
                }

                ////===========================register UVC sensor camera events==============================
                DEV_BROADCAST_DEVICEINTERFACE di_sensor = { 0 };
                di_sensor.dbcc_size = sizeof(di_sensor);
                di_sensor.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
                di_sensor.dbcc_classguid = KSCATEGORY_SENSOR_CAMERA;

                data->hdevnotify_sensor = RegisterDeviceNotification(hWnd,
                    &di_sensor,
                    DEVICE_NOTIFY_WINDOW_HANDLE);
                if (data->hdevnotify_sensor == nullptr)
                {
                    UnregisterDeviceNotification(data->hdevnotify_sensor);
                    LOG_WARNING("Register UVC events Failed!\n");
                    return FALSE;
                }

                ////===========================register HID sensor camera events==============================
                static const GUID GUID_DEVINTERFACE_HID =
                { 0x4d1e55b2,0xf16f,0x11cf,{0x88,0xcb,0x00,0x11,0x11,0x00,0x00,0x30} };

                DEV_BROADCAST_DEVICEINTERFACE hid_sensor = { 0 };
                hid_sensor.dbcc_size = sizeof(hid_sensor);
                hid_sensor.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
                hid_sensor.dbcc_classguid = GUID_DEVINTERFACE_HID;

                data->hdevnotify_sensor = RegisterDeviceNotification(hWnd,
                    &hid_sensor,
                    DEVICE_NOTIFY_WINDOW_HANDLE);
                if (data->hdevnotify_sensor == nullptr)
                {
                    UnregisterDeviceNotification(data->hdevnotify_sensor);
                    LOG_WARNING("Register UVC events Failed!\n");
                    return FALSE;
                }

                //===========================register FW Update device events==============================
                const GUID usbClassGuid = { 0xa5dcbf10, 0x6530, 0x11d2, 0x90, 0x1f, 0x00, 0xc0, 0x4f, 0xb9, 0x51, 0xed };
                DEV_BROADCAST_DEVICEINTERFACE usvDevBroadcastDeviceInterface;
                usvDevBroadcastDeviceInterface.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
                usvDevBroadcastDeviceInterface.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
                usvDevBroadcastDeviceInterface.dbcc_classguid = usbClassGuid;
                usvDevBroadcastDeviceInterface.dbcc_reserved = 0;

                data->hdevnotifyUSB = RegisterDeviceNotification(hWnd,
                    &usvDevBroadcastDeviceInterface,
                    DEVICE_NOTIFY_WINDOW_HANDLE);
                if (data->hdevnotifyUSB == NULL)
                {
                    LOG_WARNING("Register HW events Failed!\n");
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
