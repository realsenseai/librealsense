# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

"""
Brainstem Acroname Hub

See documentation for brainstem here:
https://acroname.com/reference/api/python/index.html
"""

from rspy import log, device_hub, signals
import time
import platform, re

# Minimum elapsed time (seconds) required between issuing a port disable and a
# subsequent port enable on the Acroname USBHub3p (firmware 2.12.2). Below this
# threshold, sustained disable/enable traffic on high-current ports (observed
# with D585S, ~1 A idle) can leave the BrainStem control endpoint unresponsive.
_MIN_PORT_RECYCLE_GAP_S = 2.0

if __name__ == '__main__':
    import os, sys, getopt
    def usage():
        ourname = os.path.basename( sys.argv[0] )
        print( 'Syntax: acroname [options]' )
        print( '        Control the acroname USB hub' )
        print( 'Options:' )
        print( '        --enable       Enable all ports' )
        print( '        --disable      Disable all ports' )
        print( '        --recycle      Recycle all ports' )
        print( '        --reset        Reset the acroname' )
        sys.exit(2)
    try:
        opts,args = getopt.getopt( sys.argv[1:], '',
            longopts = [ 'help', 'recycle', 'enable', 'disable', 'reset' ])
    except getopt.GetoptError as err:
        print( '-F-', err )   # something like "option -a not recognized"
        usage()
    if args or not opts:
        usage()
    # See the end of the file for all the option handling


try:
    import brainstem
    log.d( f'brainstem {brainstem.version.get_version_string().split()[-1]} loaded from {brainstem.__file__}' )
except ModuleNotFoundError:
    log.d( 'no brainstem library is available' )
    raise

class NoneFoundError( RuntimeError ):
    def __init__( self, message = None ):
        super().__init__( self, message  or  'no Acroname module found' )

class Acroname(device_hub.device_hub):



    def __init__(self):
        super().__init__()
        if discover() is None:  # raise an error if there is no hub connected
            raise NoneFoundError()
        self.hub = None
        self.all_hubs = None
        # Tracks the time we last issued a port-disable command, so that a
        # subsequent enable can wait out the minimum recycle gap (see
        # _MIN_PORT_RECYCLE_GAP_S). None means no disable issued yet.
        self._last_disable_t = None

    def get_name(self):
        """
        :return: name of the hub
        """
        return 'Acroname'

    def connect(self,  reset = False, req_spec = None ):
        """
        Connect to the hub. Raises RuntimeError on failure
        :param reset: When true, the acroname will be reset as part of the connection process
        :param req_spec: Required spec to connect to.
        """
        if not self.hub:
            self.hub = brainstem.stem.USBHub3p()

        if req_spec:
            specs = [req_spec]
        else:
            specs = discover()

        spec = specs[0]
        result = self.hub.connectFromSpec( spec )
        if result != brainstem.result.Result.NO_ERROR:
            raise RuntimeError( "failed to connect to Acroname (result={})".format( result ))
        elif len(specs) > 1:
            log.d( 'connected to', spec )

        if reset:
            log.d("resetting Acroname...")
            result = self.hub.system.reset()
            # According to brainstem support:
            # Result error is expected, so we do not check it
            # * there is also a brainstem internal console print on Linux "error release -4"
            # Disconnection is needed after a reset command
            self.hub.disconnect()

            result = None
            for i in range(10):
                result = self.hub.connectFromSpec(spec)
                if result != brainstem.result.Result.NO_ERROR:
                    time.sleep(1)
                else:
                    log.d('reconnected')
                    return
            raise RuntimeError("failed to reconnect to Acroname (result={})".format(result))


    def is_connected(self):
        return self.hub is not None and self.hub.isConnected()


    def disconnect(self):
        if self.hub:
            self.hub.disconnect()
            del self.hub
            self.hub = None


    def all_ports(self):
        """
        :return: a list of all possible ports, even if currently unoccupied or disabled
        """
        return range(8)


    def ports(self):
        """
        :return: a list of all ports currently occupied (and enabled)
        """
        occupied_ports = []
        for port in self.all_ports():
            if self._port_power(port) > 0.0:
                occupied_ports.append( port )
        return occupied_ports


    def is_port_enabled(self,  port ):
        """
        query if the input port number is enabled through Acroname
        It doesn't mean the device is enumerated as sometimes if the FW crashed we can have a situation where the port is "Disconnected" and not "OK"
        :param port: port number;
        :return: True if Acroname enabled this port, False otherwise
        """
        return ("Disabled" not in self.port_state( port ))


    def port_state(self,  port ):
        if port < 0 or port > 7:
            raise ValueError( "port number must be [0-7]" )
        #
        status = self.hub.usb.getPortState( port )
        #log.d("getPortState for port", port ,"return", port_state_bitmask_to_strings( status.value ))
        return self.port_state_bitmask_to_strings( status.value )


    def port_state_bitmask_to_strings(self,  bitmask ):
        """
        https://acroname.com/reference/devices/usbhub3p/entities/usb.html#usb-port-state
        Bit   |  Port State: Result Bitwise Description
        -----------------------------------------------
        0     |  USB Vbus Enabled - Port is Disabled/Enabled by the Acroname
        1     |  USB2 Data Enabled
        2     |  Reserved
        3     |  USB3 Data Enabled
        4:10  |  Reserved
        11    |  USB2 Device Attached
        12    |  USB3 Device Attached
        13:18 |  Reserved
        19    |  USB Error Flag
        20    |  USB2 Boost Enabled
        21:22 |  Reserved
        23    |  Device Attached
        24:31 |  Reserved
        """
        if bitmask < 0 or bitmask > 0xFFFFFFFF: # 32 bits bitmask
            raise ValueError("bitmask must be within 0 and 0xFFFFFFFF (inclusive)")
        def bit_on(x):
            return bitmask & (1 << x)
        res = ["Enabled" if bit_on(0) else "Disabled"]
        if bit_on(1):
            res.append("USB2 Data Enabled")
        if bit_on(3):
            res.append("USB3 Data Enabled")
        if bit_on(11):
            res.append("USB2 Device Attached")
        if bit_on(12):
            res.append("USB3 Device Attached")
        if bit_on(19):
            res.append("USB Error Flag")
        if bit_on(20):
            res.append("USB2 Boost Enabled")
        if bit_on(23):
            res.append("Device Attached")
        return res



    def enable_ports(self,  ports = None, disable_other_ports = False, sleep_on_change = 0 ):
        """
        Set enable state to provided ports
        :param ports: List of port numbers; if not provided, enable all ports
        :param disable_other_ports: if True, the ports not in the list will be disabled
        :param sleep_on_change: Number of seconds to sleep if any change is made
        :return: True if no errors found, False otherwise
        """
        # Wait out the minimum disable->enable gap if a disable was issued recently.
        # See _MIN_PORT_RECYCLE_GAP_S for rationale.
        if self._last_disable_t is not None:
            settle = _MIN_PORT_RECYCLE_GAP_S - (time.monotonic() - self._last_disable_t)
            if settle > 0:
                log.d(f"waiting {settle:.2f}s for Acroname port-recycle gap")
                time.sleep(settle)
        result = True
        changed = False
        log.d(f"Enabling ports {ports if ports is not None else 'all'} on Acroname"
              f"{', disabling other ports' if disable_other_ports else ''}")
        for port in self.all_ports():
            #
            if ports is None or port in ports:
                # log.d( "enabling port", port)
                action_result = self.hub.usb.setPortEnable( port )
                if action_result != brainstem.result.Result.NO_ERROR:
                    result = False
                    log.e("Failed to enable port", port)
                else:
                    changed = True
            #
            elif disable_other_ports:
                # log.d("disabling port", port)
                action_result = self.hub.usb.setPortDisable( port )
                if action_result != brainstem.result.Result.NO_ERROR:
                    result = False
                else:
                    changed = True
        #
        if changed and sleep_on_change:
            signals.register_signal_handlers()
            time.sleep( sleep_on_change )
        #
        return result


    def disable_ports(self,  ports = None, sleep_on_change = 0 ):
        """
        :param ports: List of port numbers; if not provided, disable all ports
        :param sleep_on_change: Number of seconds to sleep if any change is made
        :return: True if no errors found, False otherwise
        """
        result = True
        changed = False
        log.d(f"Disabling ports {ports if ports is not None else 'all'} on Acroname")
        for port in self.all_ports():
            if ports is None or port in ports:
                # log.d("disabling port", port)
                action_result = self.hub.usb.setPortDisable( port )
                if action_result != brainstem.result.Result.NO_ERROR:
                    result = False
                    log.e("Failed to disable port", port)
                else:
                    changed = True
        if changed:
            # Mark the time so a subsequent enable can wait out the minimum gap.
            self._last_disable_t = time.monotonic()
        if changed and sleep_on_change:
            signals.register_signal_handlers()
            time.sleep( sleep_on_change )
        #
        return result


    def set_ports_usb2(self,  portlist = None, timeout = 100e-3 ):
        """
        Set USB ports to USB2
        """
        if portlist is None:
            portlist = self.ports()
        #
        self.recycle_ports( portlist, timeout = timeout )
        #
        for port in portlist:
            self.hub.usb.setSuperSpeedDataEnable( port )
            self.hub.usb.setHiSpeedDataEnable( port )
            self.hub.usb.setSuperSpeedDataDisable( port )


    def set_ports_usb3(self,  portlist = None, timeout = 100e-3 ):
        """
        Set USB ports to support USB3
        """
        if portlist is None:
            portlist = self.ports()
        #
        self.recycle_ports( portlist, timeout = timeout )
        #
        for port in portlist:
            self.hub.usb.setSuperSpeedDataEnable( port )
            self.hub.usb.setHiSpeedDataEnable( port )
            self.hub.usb.setHiSpeedDataDisable( port )


    def _port_power(self,  port ):
        """
        """
        if port < 0 or port > 7:
            raise ValueError("port number can be only within 0 and 7 (inclusive)")
        #
        micro_volt = self.hub.usb.getPortVoltage( port )
        micro_curr = self.hub.usb.getPortCurrent( port )
        volt = float(micro_volt.value) / 10.0 ** 6
        amps = float(micro_curr.value) / 10.0 ** 6
        #
        return volt * amps

    if 'windows' in platform.system().lower():
        def get_port_by_location(self, usb_location):
            """
            """
            if usb_location:
                #
                # T265 locations look differently...
                match = re.fullmatch(r'Port_#(\d+)\.Hub_#(\d+)', usb_location, re.IGNORECASE)
                if match:
                    # We don't know how to get the port from these yet!
                    return None  # int(match.group(2))
                else:
                    split_location = [int(x) for x in usb_location.split('.')]
                    # lambda helper to return the last 2 non-zero numbers, used when connecting using an additional hub
                    # ex: laptop -> hub -> acroname
                    get_last_two_digits = lambda array: tuple(
                        reversed(list(reversed([i for i in array if i != 0]))[:2]))
                    # only the last two digits are necessary
                    first_index, second_index = get_last_two_digits(split_location)

                    return get_port_from_usb(first_index, second_index)
    else:

        def get_port_by_location(self, usb_location):
            """
            """
            if not self.all_hubs:
                self.all_hubs = set(device_hub.find_all_hubs('24ff'))  # 24ff is Acroname VendorID
            if usb_location:
                #
                # Devices connected through an acroname will be in one of two sub-hubs under the acroname main
                # hub. Each is a 4-port hub with a different port (4 for ports 0-3, 3 for ports 4-7):
                #     /:  Bus 02.Port 1: Dev 1, Class=root_hub, Driver=xhci_hcd/6p, 10000M
                #         |__ Port 2: Dev 2, If 0, Class=Hub, Driver=hub/4p, 5000M                       <--- ACRONAME
                #             |__ Port 3: Dev 3, If 0, Class=Hub, Driver=hub/4p, 5000M
                #                 |__ Port X: Dev, If...
                #                 |__ Port Y: ...
                #             |__ Port 4: Dev 4, If 0, Class=Hub, Driver=hub/4p, 5000M
                #                 |__ Port Z: ...
                # (above is output from 'lsusb -t')
                # For the above acroname at '2-2' (bus 2, port 2), there are at least 3 devices:
                #     2-2.3.X
                #     2-2.3.Y
                #     2-2.4.Z
                # Given the two sub-ports (3.X, 3.Y, 4.Z), we can get the port number.
                # NOTE: some of our devices are hubs themselves! For example, the SR300 will show as '2-2.3.2.1' --
                # we must start a known hub or else the ports we look at are meaningless...
                #
                for port in self.all_hubs:
                    if usb_location.startswith(port + '.'):
                        match = re.search(r'^(\d+)\.(\d+)', usb_location[len(port) + 1:])
                        if match:
                            return get_port_from_usb(int(match.group(1)), int(match.group(2)))

specs = None
def discover(retries = 0):
    """
    Return all Acroname module specs in a list. Raise NoneFoundError if one is not found!
    """
    global specs
    # see https://acroname.com/reference/_modules/brainstem/module.html#Module.discoverAndConnect
    if specs is None:
        log.d('discovering Acroname modules ...')
        try:
            log.debug_indent()
            for i in range(retries + 1):
                specs = brainstem.discover.findAllModules(brainstem.link.Spec.USB)
                if not specs:
                    time.sleep(1)
                else:
                    for spec in specs:
                        log.d( '...', spec )
                    break
            if not specs:
                raise NoneFoundError()
        finally:
            log.debug_unindent()

    return specs


def get_port_from_usb(first_usb_index, second_usb_index ):
    """
    Based on last two USB location index, provide the port number
    """
    acroname_port_usb_map = {(4, 4): 0,
                             (4, 3): 1,
                             (4, 2): 2,
                             (4, 1): 3,
                             (3, 4): 4,
                             (3, 3): 5,
                             (3, 2): 6,
                             (3, 1): 7,
                             }
    return acroname_port_usb_map[(first_usb_index, second_usb_index)]



if __name__ == '__main__':
    acroname = Acroname()
    for opt,arg in opts:
        if opt in ('--enable'):
            acroname.connect()
            acroname.enable_ports()   # so ports() will return all
        elif opt in ('--disable'):
            acroname.connect()
            acroname.disable_ports()
        elif opt in ('--recycle'):
            acroname.connect()
            acroname.enable_ports()   # so ports() will return all
            acroname.recycle_ports()
        elif opt in ('--reset'):
            acroname.connect( reset = True )
