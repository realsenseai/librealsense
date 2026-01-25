# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

import sys, os, re, platform
from time import perf_counter as timestamp


def usage():
    ourname = os.path.basename( sys.argv[0] )
    print( 'Syntax: devices [actions|flags]' )
    print( '        Control the LibRS devices connected' )
    print( 'Actions (only one)' )
    print( '        --list         Enumerate devices (default action)' )
    print( '        --recycle      Recycle all' )
    print( 'Flags:' )
    print( '        --all          Enable all port [requires hub]' )
    print( '        --port <#>     Enable only this port [requires hub]' )
    print( '        --ports        Show physical port for each device (rather than the RS string)' )
    sys.exit(2)

try:
    from rspy import log
except ModuleNotFoundError:
    if __name__ != '__main__':
        raise
    #
    # We need to tell Python where to look for rspy
    rspy_dir = os.path.dirname( os.path.abspath( __file__ ))
    py_dir   = os.path.dirname( rspy_dir )
    sys.path.append( py_dir )
    from rspy import log
#
# And where to look for pyrealsense2
from rspy import repo
pyrs_dir = repo.find_pyrs_dir()
sys.path.insert( 1, pyrs_dir )

# D555 takes ~18 [sec]
MAX_ENUMERATION_TIME = 22  # [sec]

# We need both pyrealsense2 and hub. We can work without hub, but
# without pyrealsense2 no devices at all will be returned.
from rspy import device_hub
try:
    import pyrealsense2 as rs
    log.d( rs )
    hub = device_hub.create() # if there's no hub, this will hold None
    sys.path = sys.path[:-1]  # remove what we added
except ModuleNotFoundError:
    log.w( 'No pyrealsense2 library is available! Running as if no cameras available...' )
    import sys
    log.d( 'sys.path=', sys.path )
    rs = None
    hub = None

import time

_device_by_sn = dict()
_context = None


class Device:
    def __init__( self, sn, dev ):
        self._sn = sn
        self._dev = dev
        self._name = None
        if dev.supports( rs.camera_info.name ):
            self._name = dev.get_info( rs.camera_info.name )
            if self._name.startswith( 'Intel RealSense ' ):
                self._name = self._name[16:]
        self._product_line = None
        if dev.supports( rs.camera_info.product_line ):
            self._product_line = dev.get_info( rs.camera_info.product_line )
        self._physical_port = dev.supports( rs.camera_info.physical_port ) and dev.get_info( rs.camera_info.physical_port ) or None

        if dev.supports(rs.camera_info.connection_type):
            self._connection_type = dev.get_info(rs.camera_info.connection_type)
            self._is_dds = self._connection_type == "DDS"
        else:
            log.w("connection_type is not supported! Assuming not a dds device")
            self._is_dds = False

        self._location = None  # might be either usb location or mac address
        try:
            if self._is_dds:
                self._location = _get_mac_address(dev)
            elif self._connection_type == "USB":
                self._location = _get_usb_location(self._physical_port)
            elif self._connection_type == "GMSL":
                # GMSL device
                log.i("GMSL device detected, no location available")
            else:
                raise Exception(f"Unknown connection type: {self._connection_type}")
        except Exception as e:
            log.e('Failed to get usb location / mac address:', e)

        self._port = None
        if hub:
            try:
                self._port = hub.get_port_by_location(self._location)
            except Exception as e:
                log.e('Failed to get device port:', e)
                log.d('    physical port is', self._physical_port)
                log.d('    location is', self._location)

        self._removed = False

    @property
    def serial_number( self ):
        return self._sn

    @property
    def name( self ):
        return self._name

    @property
    def product_line( self ):
        return self._product_line

    @property
    def connection_type( self ):
        return self._connection_type

    @property
    def physical_port( self ):
        return self._physical_port

    @property
    def usb_location( self ):
        return self._usb_location

    @property
    def port( self ):
        return self._port

    @property
    def handle( self ):
        return self._dev

    @property
    def enabled( self ):
        return self._removed is False

    @property
    def is_dds(self):
        return self._is_dds


def wait_until_all_ports_disabled( timeout = 5 ):
    """
    Waits for all ports to be disabled
    """
    for retry in range( timeout ):
        if len( enabled() ) == 0:
            return True
        time.sleep( 1 )
    log.w( 'Timed out waiting for 0 devices' )
    return False


def map_unknown_ports():
    """
    Fill in unknown ports in devices by enabling one port at a time, finding out which device
    is there.
    """
    if not hub:
        return
    global _device_by_sn
    devices_with_unknown_ports = [device for device in _device_by_sn.values() if device.port is None]
    if not devices_with_unknown_ports:
        return
    #
    ports = hub.ports()
    known_ports = [device.port for device in _device_by_sn.values() if device.port is not None]
    unknown_ports = [port for port in ports if port not in known_ports]
    try:
        log.d( 'mapping unknown ports', unknown_ports, '...' )
        log.debug_indent()
        #log.d( "active ports:", ports )
        #log.d( "- known ports:", known_ports )
        #log.d( "= unknown ports:", unknown_ports )
        #
        for known_port in known_ports:
            if known_port not in ports:
                log.e( "A device was found on port", known_port, "but the port is not reported as used by the hub!" )
        #
        if len( unknown_ports ) == 1:
            device = devices_with_unknown_ports[0]
            log.d( '... port', unknown_ports[0], 'has', device.handle )
            device._port = unknown_ports[0]
            return
        #
        hub.disable_ports( ports )
        wait_until_all_ports_disabled()
        #
        # Enable one port at a time to try and find what device is connected to it
        n_identified_ports = 0
        for port in unknown_ports:
            #
            log.d( 'enabling port', port )
            hub.enable_ports( [port], disable_other_ports=True )
            sn = None
            for retry in range( MAX_ENUMERATION_TIME ):
                if len( enabled() ) == 1:
                    sn = list( enabled() )[0]
                    break
                time.sleep( 1 )
            if not sn:
                log.d( 'could not recognize device in port', port )
            else:
                device = _device_by_sn.get( sn )
                if device:
                    log.d( '... port', port, 'has', device.handle )
                    device._port = port
                    n_identified_ports += 1
                    if len( devices_with_unknown_ports ) == n_identified_ports:
                        #log.d( 'no more devices; stopping' )
                        break
                else:
                    log.w( "Device with serial number", sn, "was found in port", port,
                            "but was not in context" )
            hub.disable_ports( [port] )
            wait_until_all_ports_disabled()
    finally:
        log.debug_unindent()


def query( monitor_changes=True, hub_reset=False, recycle_ports=True, disable_dds=True ):
    """
    Start a new LRS context, and collect all devices
    :param monitor_changes: If True, devices will update dynamically as they are removed/added
    :param recycle_ports: True to recycle all ports before querying devices; False to leave as-is
    :param hub_reset: Whether we want to reset the hub - this might be a better way to
        recycle the ports in certain cases that leave the ports in a bad state
    :param disable_dds: Whether we want to see dds devices or not
    """
    global rs
    if not rs:
        return
    #
    # Before we can start a context and query devices, we need to enable all the ports
    # on the hub, if any:
    if hub:
        if not hub.is_connected():
            hub.connect(hub_reset)
        if recycle_ports:
            hub.disable_ports( sleep_on_change = 5 )
            print("Sleepint for :", MAX_ENUMERATION_TIME)
            hub.enable_ports( sleep_on_change = MAX_ENUMERATION_TIME )
            print("ports enabled")
    #
    # Get all devices, and store by serial-number
    global _device_by_sn, _context, _port_to_sn
    settings = {'dds' : { 'enabled' : True }}  # explicitly enable dds in case there's an issue with the config file
    if disable_dds:
        settings['dds']['enabled'] = False
    _context = rs.context( settings )
    _device_by_sn = dict()
    try:
        log.debug_indent()
        for retry in range(3):
            try:
                devices = _context.query_devices()
                break
            except RuntimeError as e:
                log.d( 'FAILED to query devices:', e )
                if retry > 1:
                    log.e( 'FAILED to query devices', retry + 1, 'times!' )
                    raise
                else:
                    time.sleep( 1 )
        for dev in devices:
            # The FW update ID is always available, it seems, and is the ASIC serial number
            # whereas the Serial Number is the OPTIC serial number and is only available in
            # non-recovery devices. So we use the former...
            try:
                sn = dev.get_info( rs.camera_info.firmware_update_id )
            except RuntimeError as e:
                log.e( f'Found device with S/N {sn} but trying to get fw-update-id failed: {e}' )
                continue
            device = Device( sn, dev )
            _device_by_sn[sn] = device
            log.d( '... port {}:'.format( device.port is None and '?' or device.port ), sn, dev )
    finally:
        log.debug_unindent()
    #
    if monitor_changes:
        _context.set_devices_changed_callback( _device_change_callback )


def _device_change_callback( info ):
    """
    Called when librealsense detects a device change (see query())
    """
    global _device_by_sn
    for device in _device_by_sn.values():
        if device.enabled  and  info.was_removed( device.handle ):
            device._removed = True
            log.d( 'device removed:', device.serial_number )
    for handle in info.get_new_devices():
        sn = handle.get_info( rs.camera_info.firmware_update_id )
        log.d( 'device added:', sn, handle )
        if sn in _device_by_sn:
            device = _device_by_sn[sn]
            device._dev = handle     # Because it has a new handle!
            device._removed = False
        else:
            # shouldn't see new devices...
            log.d( 'new device detected!?' )
            _device_by_sn[sn] = Device( sn, handle )


def all():
    """
    :return: A set of all device serial-numbers at the time of query()
    """
    global _device_by_sn
    return _device_by_sn.keys()


def enabled():
    """
    :return: A set of all device serial-numbers that are currently enabled
    """
    global _device_by_sn
    return { device.serial_number for device in _device_by_sn.values() if device.enabled }


def by_product_line( product_line, ignored_products ):
    """
    :param product_line: The product line we're interested in, as a string ("L500", etc.)
    :param ignored_products: List of products we want to ignore. e.g. ['D455', 'D457', etc.]
    :return: A set of device serial-numbers
    """
    global _device_by_sn
    result = set()
    for device in _device_by_sn.values():
        if device.product_line == product_line:
            for ignored_product in ignored_products:
                if ignored_product in device.name:
                    break
            else:
                result.add(device.serial_number)
    return result


def by_name( name, ignored_products ):
    """
    :param name: Part of the product name to search for ("L515" would match "Intel RealSense L515")
    :param ignored_products: List of products we want to ignore. e.g. ['D455', 'D457', etc.]
    :return: A set of device serial-numbers
    """
    global _device_by_sn
    result = set()
    ignored_list_as_str = " ".join(ignored_products)
    if name not in ignored_list_as_str:
        for device in _device_by_sn.values():
            if device.name and device.name.find( name ) >= 0:
                result.add(device.serial_number)
    return result

def by_spec( spec, ignored_products ):
    """
    Helper function for by_configuration. Yields all serial-numbers matching the given spec
    :param spec: A product name/line (as a string) we want to get serial number of, or an actual s/n
    :param ignored_products: List of products we want to ignore. e.g. ['D455', 'D457', etc.]
    :return: A set of device serial-numbers
    """
    if spec.endswith( '*' ):
        for sn in by_product_line( spec[:-1], ignored_products ):
            yield sn
    elif get( spec ):
        yield spec   # the device serial number
    else:
        for sn in by_name( spec, ignored_products ):
            yield sn


def expand_specs( specs ):
    """
    Given a collection of configuration specs, expand them into actual serial numbers.
    Specs can be loaded from a file: see load_specs_from_file()
    :param specs: a collection of specs
    :return: a set of serial-numbers
    """
    expanded = set()
    for spec in specs:
        sns = {sn for sn in by_spec( spec )}
        if sns:
            expanded.update( sns )
        else:
            # maybe the spec is a specific serial-number?
            if get(spec):
                expanded.add( spec )
            else:
                log.d( 'unknown spec:', spec )
    return expanded


def load_specs_from_file( filename ):
    """
    Loads a set of specs from a file:
        - Comments (#) are removed
        - Each word in the file is a spec
    :param filename: the path to the text file we want to load
    :return: a set of specs that can then be expanded to a set of serial-numbers (see expand_specs())
    """
    from rspy import file
    exceptions = set()
    for line, comment in file.split_comments( filename ):
        specs = line.split()
        if specs:
            log.d( '...', specs, comment and ('  # ' + comment) or '', )
            exceptions.update( specs )
    return exceptions


def by_configuration( config, exceptions=None, inclusions=None ):
    """
    Yields the serial numbers fitting the given configuration. If configuration includes an 'each' directive
    will yield all fitting serial numbers one at a time. Otherwise yields one set of serial numbers fitting the configuration

    :param config: A test:device line collection of arguments (e.g., [L515 D400*]) or serial numbers
    :param exceptions: A collection of serial-numbers that serve as exceptions that will never get matched
    :param inclusions: A collection of serial-numbers from which to match - nothing else will get matched

    If no device matches the configuration devices specified, a RuntimeError will be raised unless
    'inclusions' is provided and the configuration is simple, and an empty set yielded to signify.
    """
    exceptions = exceptions or set()
    # split the current config to two lists:
    #     1) new_config (the wanted products)
    #     2) ignored_products (strings starting with !)
    # For example: "each(D400*) !D457" ---> new_config = ['each(D400*)'], ignored_products = ['D457']
    new_config = []
    ignored_products = []
    for p in config:
        if p[0] == '!':
            ignored_products.append(p[1:])  # remove the '!'
        else:
            new_config.append(p)

    nothing_matched = True
    if len( new_config ) > 0 and re.fullmatch( r'each\(.+\)', new_config[0], re.IGNORECASE ):
        spec = new_config[0][5:-1]
        for sn in by_spec( spec, ignored_products ):
            if sn in exceptions:
                continue
            if inclusions and sn not in inclusions:
                continue
            nothing_matched = False
            yield { sn }
    else:
        sns = set()
        for spec in new_config:
            old_len = len(sns)
            for sn in by_spec( spec, ignored_products ):
                if sn in exceptions:
                    continue
                if inclusions and sn not in inclusions:
                    continue
                if sn not in sns:
                    sns.add( sn )
                    break
            new_len = len(sns)
            if new_len == old_len:
                # No new device matches the spec:
                #   - if no inclusions were specified, this is always an error
                #   - with inclusions, it's not an error only if it's the only spec
                if not inclusions or len(new_config) > 1:
                    error = 'no device matches configuration "' + spec + '"'
                    if old_len:
                        error += ' (after already matching ' + str(sns) + ')'
                    if ignored_products:
                        error += ' (!' + str(ignored_products) + ')'
                    if exceptions:
                        error += ' (-' + str(exceptions) + ')'
                    if inclusions:
                        error += ' (+' + str(inclusions) + ')'
                    raise RuntimeError( error )
        if sns:
            nothing_matched = False
            yield sns
    if nothing_matched and inclusions:
        yield set()  # let the caller decide how to deal with it


def get_first( sns ):
    """
    Throws if no serial-numbers are available!
    :param sns: An iterable list of serial-numbers. If None, defaults to all enabled() devices
    :return: The first Device in the given serial-numbers
    """
    return get( next( iter( sns or enabled() )))


def get( sn ):
    """
    :param sn: The serial-number of the requested device
    :return: The Device object with the given SN, or None
    """
    global _device_by_sn
    return _device_by_sn.get(sn)


def get_by_port( port ):
    """
    Return the Device at the given port number. Note that the device may not be enabled!
    :param sn: The port of the requested device
    :return: The Device object, or None
    """
    global _device_by_sn
    for sn in all():
        device = get( sn )
        if device.port == port:
            return device
    return None


def recovery():
    """
    :return: A set of all device serial-numbers that are in recovery mode
    """
    global _device_by_sn
    return { device.serial_number for device in _device_by_sn.values() if device.handle.is_in_recovery_mode() }


def enable_only( serial_numbers, recycle = False, timeout = MAX_ENUMERATION_TIME ):
    """
    Enable only the devices corresponding to the given serial-numbers. This can work either
    with or without a hub: without, the devices will simply be HW-reset, but other devices
    will still be present.

    NOTE: will raise an exception if any SN is unknown!

    :param serial_numbers: A collection of serial-numbers to enable - all others' ports are
                           disabled and will no longer be usable!
    :param recycle: If False, the devices will not be reset if they were already enabled. If
                    True, the devices will be recycled by disabling the port, waiting, then
                    re-enabling
    :param timeout: The maximum seconds to wait to make sure the devices are indeed online
    """
    if hub:
        #
        ports = [ get( sn ).port for sn in serial_numbers ]
        #
        if recycle:
            #
            log.d( 'recycling ports via hub:', ports )
            #
            enabled_devices = enabled()
            hub.disable_ports( )
            _wait_until_removed( enabled_devices, timeout = timeout )
            #
            hub.enable_ports( ports )
            #
        else:
            #
            hub.enable_ports( ports, disable_other_ports = True )
        #
        _wait_for( serial_numbers, timeout = timeout )
        #
    elif recycle:
        #
        hw_reset( serial_numbers )
        #
    else:
        log.d( 'no hub; ports left as-is' )
        # even without reset, enable_only should wait for the devices to be available again
        _wait_for(serial_numbers, timeout=timeout)


def enable_all():
    """
    Enables all ports on the hub -- without a hub, this does nothing!
    """
    hub.enable_ports()


def _wait_until_removed( serial_numbers, timeout = 5 ):
    """
    Wait until the given serial numbers are all offline

    :param serial_numbers: A collection of serial-numbers to wait until removed
    :param timeout: Number of seconds of maximum wait time
    :return: True if all have come offline; False if timeout was reached
    """
    while True:
        have_devices = False
        enabled_sns = enabled()
        for sn in serial_numbers:
            if sn in enabled_sns:
                have_devices = True
                break
        if not have_devices:
            return True
        #
        if timeout <= 0:
            log.e( "timed out waiting for devices to be removed" )
            return False
        timeout -= 1
        time.sleep( 1 )


def _wait_for( serial_numbers, timeout = MAX_ENUMERATION_TIME ):
    """
    Wait until the given serial numbers are all online

    :param serial_numbers: A collection of serial-numbers to wait for
    :param timeout: Number of seconds of maximum wait time
    :return: True if all have come online; False if timeout was reached
    """
    did_some_waiting = False

    while True:
        #
        have_all_devices = True
        enabled_sns = enabled()
        for sn in serial_numbers:
            if sn not in enabled_sns:
                have_all_devices = False
                break
        #
        if have_all_devices:
            if did_some_waiting:
                # Wait an extra second, just in case -- let the devices properly power up
                #log.d( 'all devices powered up' )
                time.sleep( 1 )
            return True
        #
        if timeout <= 0:
            if did_some_waiting:
                log.d( 'timed out waiting for a device connection' )
            return False
        timeout -= 1
        time.sleep( 1 )
        did_some_waiting = True

def hw_reset( serial_numbers, timeout = MAX_ENUMERATION_TIME ):
    """
    Recycles the given devices manually, using a hardware-reset (rather than any hub port
    reset). The devices are sent a HW-reset command and then we'll wait until they come back
    online.

    NOTE: will raise an exception if any SN is unknown!

    :param serial_numbers: A collection of serial-numbers to reset
    :param timeout: Maximum # of seconds to wait for the devices to come back online
    :return: True if all devices have come back online before timeout
    """
    # we can wait for usb and dds devices to be removed, but not for mipi devices
    removable_devs_sns = {sn for sn in serial_numbers if
                          _device_by_sn[sn].port is not None or _device_by_sn[sn].is_dds}

    _wait_for(serial_numbers, timeout=timeout) # make sure devices are added before doing hw reset
    for sn in serial_numbers:
        dev = get( sn ).handle
        dev.hardware_reset()
    #

    if removable_devs_sns:
        _wait_until_removed( removable_devs_sns )
        # if relevant, we need to handle case where we have both removable and non-removable devices
    else:
        # normally we will get here with a mipi device,
        # we want to allow some time for the device to reinitialize as it was not disconnected
        time.sleep(3)
    #
    return _wait_for( serial_numbers, timeout = timeout )



###############################################################################################
if 'windows' in platform.system().lower():
    def _get_usb_location( physical_port ):
        """
        Helper method to get Windows USB location from registry
        """
        if not physical_port:
            return None
        # physical port example:
        #   \\?\usb#vid_8086&pid_0b07&mi_00#6&8bfcab3&0&0000#{e5323777-f976-4f5b-9b55-b94699c46e44}\global
        #
        re_result = re.match( r'.*\\(.*)#vid_(.*)&pid_(.*)(?:&mi_(.*))?#(.*)#', physical_port, flags = re.IGNORECASE )
        if not re_result:
            return None
        dev_type = re_result.group(1)
        vid = re_result.group(2)
        pid = re_result.group(3)
        mi = re_result.group(4)
        unique_identifier = re_result.group(5)
        #
        import winreg
        if mi:
            registry_path = r"SYSTEM\CurrentControlSet\Enum\{}\VID_{}&PID_{}&MI_{}\{}".format(
                dev_type, vid, pid, mi, unique_identifier
                )
        else:
            registry_path = r"SYSTEM\CurrentControlSet\Enum\{}\VID_{}&PID_{}\{}".format(
                dev_type, vid, pid, unique_identifier
                )
        try:
            reg_key = winreg.OpenKey( winreg.HKEY_LOCAL_MACHINE, registry_path )
        except FileNotFoundError:
            log.e( 'Could not find registry key for port:', registry_path )
            log.e( '    usb location:', physical_port )
            return None
        result = winreg.QueryValueEx( reg_key, "LocationInformation" )
        # location example: 0000.0014.0000.016.003.004.003.000.000
        # and, for T265: Port_#0002.Hub_#0006
        return result[0]
else:
    def _get_usb_location( physical_port ):
        """
        """
        if not physical_port:
            return None
        # physical port example:
        # u'/sys/devices/pci0000:00/0000:00:14.0/usb2/2-3/2-3.3/2-3.3.1/2-3.3.1:1.0/video4linux/video0'
        #
        split_location = physical_port.split( '/' )
        if len(split_location) > 4:
            port_location = split_location[-4]
        elif len(split_location) == 1:
            # Recovery devices may have only the relevant port: e.g., L515 Recovery has "2-2.4.4-84"
            port_location = physical_port
        else:
            raise RuntimeError( f"invalid physical port '{physical_port}'" )
        # location example: 2-3.3.1
        return port_location


def _get_mac_address(dev):
    GET_ETH_CONFIG_OPCODE = 187
    raw_command = rs.debug_protocol(dev).build_command(GET_ETH_CONFIG_OPCODE,1)
    raw_result = rs.debug_protocol(dev).send_and_receive_raw_data(raw_command)
    if raw_result[0] == GET_ETH_CONFIG_OPCODE: # success
        return ":".join([f"{num:02x}" for num in raw_result[52:58]]) # bytes for the MAC address

    return None

###############################################################################################
if __name__ == '__main__':
    import os, sys, getopt

    try:
        opts,args = getopt.getopt( sys.argv[1:], '',
            longopts = [ 'help', 'recycle', 'all', 'none', 'list', 'port=', 'PORT=', 'ports' ])
    except getopt.GetoptError as err:
        print( '-F-', err )   # something like "option -a not recognized"
        usage()
    if args:
        usage()
    try:
        if hub:
            if not hub.is_connected():
                hub.connect()

        action = 'list'
        def get_handle(dev):
            return dev.handle
        def get_phys_port(dev):
            return dev.physical_port or "???"
        printer = get_handle
        for opt,arg in opts:
            if opt in ('--list'):
                action = 'list'
            elif opt in ('--port','--PORT'):
                if not hub:
                    log.f( 'No hub available' )
                all_ports = hub.all_ports()
                str_ports = arg.split(',')
                ports = [int(port) for port in str_ports if port.isnumeric() and int(port) in all_ports]
                if len(ports) != len(str_ports):
                    log.f( 'Invalid ports', str_ports )
                # With --port, leave other ports alone
                # With --PORT, disable other ports
                #       This would otherwise require --none, wait, --port)
                #       Note that it does not recycle the port if it was already enabled
                hub.enable_ports( ports, disable_other_ports=(opt == '--PORT') )
                action = 'none'
            elif opt in ('--ports'):
                printer = get_phys_port
            elif opt in ('--all'):
                if not hub:
                    log.f( 'No hub available' )
                hub.enable_ports()
                action = 'none'
            elif opt in ('--none'):
                if not hub:
                    log.f( 'No hub available' )
                hub.disable_ports()
                action = 'none'
            elif opt in ('--recycle'):
                action = 'recycle'
            else:
                usage()
        #
        if action == 'list':
            query( monitor_changes=False, recycle_ports=False, hub_reset=False, disable_dds=False )
            map_unknown_ports()
            for sn in all():
                device = get( sn )
                print( '{port} {name:30} {sn:20} {handle}'.format(
                    sn = sn,
                    name = device.name,
                    port = device.port is None and '?' or device.port,
                    handle = printer(device)
                    ))
        elif action == 'recycle':
            hub.recycle_ports()
    finally:
        # Disconnect from the hub -- if we don't it might crash on Linux...
        hub.disconnect()


