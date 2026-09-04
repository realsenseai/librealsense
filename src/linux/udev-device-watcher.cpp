// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

#include "udev-device-watcher.h"

#include <dirent.h>
#include <poll.h>

#include <chrono>
#include <fstream>
#include <set>
#include <string>
#include <exception>

using std::string;
using std::runtime_error;


namespace {

    // Devices are keyed by "busnum-devpath-devnum" (see backend-v4l2.cpp), and sysfs
    // names the same device "busnum-devpath" - so drop the trailing devnum.
    std::string usb_sysfs_dir( std::string const & unique_id )
    {
        auto last = unique_id.rfind( '-' );
        if( last == std::string::npos )
            return {};
        return "/sys/bus/usb/devices/" + unique_id.substr( 0, last );
    }


    std::string read_first_line( std::string const & path )
    {
        std::ifstream f( path );
        std::string line;
        std::getline( f, line );
        return line;
    }


    // Names of the entries directly under a directory, or empty if it can't be read.
    std::vector< std::string > list_dir( std::string const & path )
    {
        std::vector< std::string > names;
        if( DIR * dir = opendir( path.c_str() ) )
        {
            while( struct dirent * e = readdir( dir ) )
            {
                std::string name = e->d_name;
                if( name != "." && name != ".." )
                    names.push_back( std::move( name ) );
            }
            closedir( dir );
        }
        return names;
    }


    // Returns the unique_ids of USB devices still mid-enumeration, so an arrival can
    // wait rather than publish a device that comes up missing sensors. A device's sysfs
    // interfaces come from its USB configuration descriptor, which makes them the
    // authoritative set to expect: every video interface should have surfaced a
    // /dev/video node the backend can see, and every HID interface should have surfaced
    // through the HID backend.
    std::set< std::string > incomplete_devices( librealsense::platform::backend_device_group const & curr )
    {
        std::set< std::string > uvc_nodes;    // /dev/videoN the backend enumerated
        std::set< std::string > uids;
        for( auto && uvc : curr.uvc_devices )
        {
            uvc_nodes.insert( uvc.id );
            uids.insert( uvc.unique_id );
        }
        std::set< std::string > hid_uids;
        for( auto && hid : curr.hid_devices )
            hid_uids.insert( hid.unique_id );

        std::set< std::string > incomplete;
        for( auto && uid : uids )
        {
            std::string const dir = usb_sysfs_dir( uid );
            if( dir.empty() )
                continue;
            std::string const bus_dev = dir.substr( dir.rfind( '/' ) + 1 );
            for( auto && entry : list_dir( dir ) )
            {
                // Interface directories are named "<busnum-devpath>:<config>.<iface>"
                if( entry.compare( 0, bus_dev.size(), bus_dev ) != 0 || entry.find( ':' ) == std::string::npos )
                    continue;
                std::string const iface = dir + "/" + entry;
                std::string const cls = read_first_line( iface + "/bInterfaceClass" );
                if( cls == "0e" )   // video
                {
                    bool surfaced = false;
                    for( auto && node : list_dir( iface + "/video4linux" ) )
                        if( uvc_nodes.count( "/dev/" + node ) )
                            surfaced = true;
                    if( ! surfaced )
                    {
                        incomplete.insert( uid );
                        break;
                    }
                }
                else if( cls == "03" )   // HID - the IMU on an IMU-bearing camera
                {
                    if( ! hid_uids.count( uid ) )
                    {
                        incomplete.insert( uid );
                        break;
                    }
                }
            }
        }
        return incomplete;
    }


    void foreach_device_prop( struct udev_device * udev_dev,
                              std::function< void( std::string const & param, std::string const & value ) > callback )
    {
        struct udev_list_entry * prop_entry;
        udev_list_entry_foreach( prop_entry, udev_device_get_properties_list_entry( udev_dev ) )
        {
            const string prop_name = udev_list_entry_get_name( prop_entry );
            const string prop_value = udev_list_entry_get_value( prop_entry );

            callback( prop_name, prop_value );
        }
    }


    std::string udev_string( struct udev_device * dev )
    {
        std::ostringstream os;
        if( ! dev )
            os << "null";
        else
        {
            char const * sysname = udev_device_get_sysname( dev );
            char const * model = udev_device_get_property_value( dev, "ID_MODEL" );
            
            // For bind/unbind events, ID_MODEL and sysname may be null
            if( ! model  ||  strncmp( model, "RealSense", 5 ))
                model = sysname;
            
            if( model )
                os << model;
            else
                os << "<unknown>";
            
            if( sysname && model != sysname )
                os << " [" << sysname << ']';
            //os << udev_device_get_syspath( dev );

            // Enable to get a list of all properties
            // For usb_device:
            //     ACTION=add BUSNUM=002 DEVNAME=/dev/bus/usb/002/125 DEVNUM=125
            //     DEVPATH=/devices/pci0000:00/0000:00:14.0/usb2/2-2/2-2.4/2-2.4.4 DEVTYPE=usb_device DRIVER=usb
            //     ID_BUS=usb ID_MODEL=Intel_R__RealSense_TM__515
            //     ID_MODEL_ENC=Intel\x28R\x29\x20RealSense\x28TM\x29\x20515 ID_MODEL_ID=0b64 ID_REVISION=1058
            //     ID_SERIAL=Intel_R__RealSense_TM__Camera_Intel_R__RealSense_TM__515_00000000F0070132
            //     ID_SERIAL_SHORT=00000000F0070132 ID_USB_INTERFACES=:0e0100:0e0200:ff0000:030000:
            //     ID_VENDOR=Intel_R__RealSense_TM__Camera
            //     ID_VENDOR_ENC=Intel\x28R\x29\x20RealSense\x28TM\x29\x20Camera ID_VENDOR_FROM_DATABASE=Intel Corp.
            //     ID_VENDOR_ID=8086 MAJOR=189 MINOR=252 PRODUCT=8086/b64/1058 SEQNUM=249377 SUBSYSTEM=usb
            //     TYPE=239/2/1 USEC_INITIALIZED=862653458266
            // And, for usb_interface:
            //     .MM_USBIFNUM=01 ACTION=add
            //     DEVPATH=/devices/pci0000:00/0000:00:14.0/usb2/2-2/2-2.4/2-2.4.4/2-2.4.4:1.1 DEVTYPE=usb_interface
            //     DRIVER=uvcvideo ID_USB_CLASS_FROM_DATABASE=Miscellaneous Device
            //     ID_USB_PROTOCOL_FROM_DATABASE=Interface Association ID_VENDOR_FROM_DATABASE=Intel Corp.
            //     INTERFACE=14/2/0 MODALIAS=usb:v8086p0B64d1058dcEFdsc02dp01ic0Eisc02ip00in01 PRODUCT=8086/b64/1058
            //     SEQNUM=249386 SUBSYSTEM=usb TYPE=239/2/1 USEC_INITIALIZED=862653461483
#if 0
            char sep = '[';
            foreach_device_prop( dev, [&]( std::string const& name, std::string const & value )
            {
                os << sep << name << '=' << value;
                sep = ' ';
            } );
            if( sep != '[' )
                os << ']';
#endif
        }
        return os.str();
    }
    }


namespace librealsense {


udev_device_watcher::udev_device_watcher( const platform::backend * backend )
    : _backend( backend )
    , _active_object( [this]( dispatcher::cancellable_timer timer ) {
        struct pollfd fds;
        fds.fd = _udev_monitor_fd;
        fds.events = POLLIN;

        // Polling will block for a time but we cannot block for too long, as we want destruction to happen in
        // reasonable time. So we use a short-enough period:
        int const POLLING_PERIOD_MS = 100;
        if( poll( &fds, 1, POLLING_PERIOD_MS ) > 0 )
        {
            if( timer.was_stopped() || ! _udev_monitor || POLLIN != fds.revents )
                return;
            
            auto udev_dev = udev_monitor_receive_device( _udev_monitor );
            if( ! udev_dev )
            {
                LOG_DEBUG( "failed to read data from udev monitor socket" );
                return;
            }

            const string udev_action = udev_device_get_action( udev_dev );
            if( udev_action == "add" || udev_action == "remove" || udev_action == "bind" || udev_action == "unbind" )
            {
                LOG_DEBUG( "[udev] " << udev_action << " " << udev_string( udev_dev ) );
                // On remove events, we get all the device interfaces first, and lastly we get the device.
                // On add events, we get the device first and only then the device interfaces.
                // Hardware reset triggers unbind/bind events instead of remove/add.
                // In any case, we get lots of events for each device. And we only want to do one enumeration --
                // so we wait for things to calm down and just remember that enumeration is needed...
                _changed = true;
            }

            udev_device_unref( udev_dev );
        }
        else if( _changed )
        {
            // Something's changed but nothing's happened in the last polling period -- let's enumerate!
            LOG_DEBUG( "[udev] checking ..." );
            platform::backend_device_group curr( _backend->query_uvc_devices(),
                                                 _backend->query_usb_devices(),
                                                 _backend->query_hid_devices() );

            // A device whose interfaces are still appearing is held back rather than
            // published missing sensors - it simply isn't in the group yet, so removals
            // of other devices are still reported immediately. Each device gets its own
            // budget: once that is spent we publish whatever it has, which is what lets
            // a genuinely partial device through.
            static constexpr auto MAX_WAIT = std::chrono::seconds( 8 );
            auto const now = std::chrono::steady_clock::now();
            auto incomplete = incomplete_devices( curr );
            for( auto it = _incomplete_since.begin(); it != _incomplete_since.end(); )
            {
                if( incomplete.count( it->first ) )
                    ++it;
                else
                    it = _incomplete_since.erase( it );
            }
            bool waiting = false;
            for( auto && uid : incomplete )
            {
                auto inserted = _incomplete_since.emplace( uid, now );
                if( now - inserted.first->second >= MAX_WAIT )
                    continue;   // waited long enough; let it through as-is
                LOG_DEBUG( "[udev] " << uid << " still enumerating; holding it back" );
                auto held = [&uid]( auto const & device ) { return device.unique_id == uid; };
                curr.uvc_devices.erase( std::remove_if( curr.uvc_devices.begin(), curr.uvc_devices.end(), held ),
                                        curr.uvc_devices.end() );
                curr.hid_devices.erase( std::remove_if( curr.hid_devices.begin(), curr.hid_devices.end(), held ),
                                        curr.hid_devices.end() );
                waiting = true;
            }

            if( list_changed( _devices_data.uvc_devices, curr.uvc_devices )
                || list_changed( _devices_data.usb_devices, curr.usb_devices )
                || list_changed( _devices_data.hid_devices, curr.hid_devices ) )
            {
                LOG_DEBUG( "[udev] changed!" );
                callback_invocation_holder callback = { _callback_inflight.allocate(), &_callback_inflight };
                if( callback )
                    _callback( _devices_data, curr );
                _devices_data = curr;
            }
            // Keep checking while something is being held back, or its arrival would
            // never be reported.
            _changed = waiting;
        }
    }, "udev-device-watcher" )
{
    _udev_ctx = udev_new();
    if( ! _udev_ctx )
        throw runtime_error( "could not initialize udev" );

    _udev_monitor = udev_monitor_new_from_netlink( _udev_ctx, "udev" );
    if( ! _udev_monitor )
    {
        udev_unref( _udev_ctx );
        _udev_ctx = nullptr;
        throw runtime_error( "could not initialize udev monitor" );
    }

    _udev_monitor_fd = udev_monitor_get_fd( _udev_monitor );

    if( udev_monitor_filter_add_match_subsystem_devtype( _udev_monitor, "usb", 0 ) )
    {
        udev_monitor_unref( _udev_monitor );
        _udev_monitor = nullptr;
        _udev_monitor_fd = -1;
        udev_unref( _udev_ctx );
        _udev_ctx = nullptr;
        throw runtime_error( "could not initialize udev monitor filter for \"usb\" subsystem" );
    }

    if( udev_monitor_enable_receiving( _udev_monitor ) )
    {
        udev_monitor_unref( _udev_monitor );
        _udev_monitor = nullptr;
        _udev_monitor_fd = -1;
        udev_unref( _udev_ctx );
        _udev_ctx = nullptr;
        throw runtime_error( "failed to enable the udev monitor" );
    }

    _devices_data = { _backend->query_uvc_devices(), _backend->query_usb_devices(), _backend->query_hid_devices() };
}


udev_device_watcher::~udev_device_watcher()
{
    try
    {
        stop();
    }
    catch(...)
    {
        LOG_DEBUG( "Error while stopping udev device watcher" );
    }
    /* Release the udev monitor */
    if( _udev_monitor )
        udev_monitor_unref( _udev_monitor );
    _udev_monitor = nullptr;
    _udev_monitor_fd = -1;

    /* Clean up the udev context */
    if( _udev_ctx )
        udev_unref( _udev_ctx );
    _udev_ctx = nullptr;
}


// Scan devices using udev
void udev_device_watcher::foreach_device( std::function< void( struct udev_device * udev_dev ) > callback )
{
    auto enumerator = udev_enumerate_new( _udev_ctx );
    if( ! enumerator )
        throw runtime_error( "error creating udev enumerator" );

    udev_enumerate_add_match_subsystem( enumerator, "usb" );
    udev_enumerate_scan_devices( enumerator );
    
    auto devices = udev_enumerate_get_list_entry( enumerator );
    struct udev_list_entry * device_entry;
    udev_list_entry_foreach( device_entry, devices )
    {
        const char * path = udev_list_entry_get_name( device_entry );
        struct udev_device * udev_dev = udev_device_new_from_syspath( _udev_ctx, path );

        callback( udev_dev );

        udev_device_unref( udev_dev );
    }

    udev_enumerate_unref( enumerator );
}


}  // namespace librealsense
