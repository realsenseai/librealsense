# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

from rspy import log
from rspy import signals
from abc import ABC, abstractmethod


class NoneFoundError( RuntimeError ):
    def __init__( self, message = None ):
        super().__init__( self, message  or  'no hub found' )


class device_hub(ABC):
    def __getattribute__(self, name):
        attr = super().__getattribute__(name)

        # some hubs override / clear signals, this is used to re-register them, only for methods for now
        if callable(attr) and not name.startswith('__'):
            def wrapper(*args, **kwargs):
                result = attr(*args, **kwargs)
                signals.register_signal_handlers()
                return result

            return wrapper

        return attr  # Return non-methods or special methods as-is

    @abstractmethod
    def get_name(self):
        """
        :return: name of the hub
        """
        pass

    @abstractmethod
    def connect(self, reset = False):
        """
        Connect to a hub device
        :param reset: When true, the hub will be reset as part of the connection process
        """
        pass

    @abstractmethod
    def is_connected(self):
        """
        :return: True if the hub is connected, False otherwise
        """
        pass

    @abstractmethod
    def disconnect(self):
        """
        Disconnect from the hub
        """
        pass

    @abstractmethod
    def all_ports(self):
        """
        :return: a list of all possible ports, even if currently unoccupied or disabled
        """
        pass

    @abstractmethod
    def ports(self):
        """
        :return: a list of all ports currently enabled
        """
        pass

    @abstractmethod
    def is_port_enabled( self, port ):
        """
        query if the input port number is enabled through the hub
        :param port: port number;
        :return: True if the hub enabled this port, False otherwise
        """
        pass

    @abstractmethod
    def port_state( self, port ):
        pass

    @abstractmethod
    def enable_ports( self,  ports = None, disable_other_ports = False, sleep_on_change = 0 ):
        """
        Set enable state to provided ports
        :param ports: List of port numbers; if not provided, enable all ports
        :param disable_other_ports: if True, the ports not in the list will be disabled
        :param sleep_on_change: Number of seconds to sleep if any change is made
        :return: True if no errors found, False otherwise
        """
        pass

    @abstractmethod
    def disable_ports( self, ports = None, sleep_on_change = 0 ):
        """
        :param ports: List of port numbers; if not provided, disable all ports
        :param sleep_on_change: Number of seconds to sleep if any change is made
        :return: True if no errors found, False otherwise
        """
        pass

    @abstractmethod
    def get_port_by_location(self, usb_location):
        """
        """
        pass

    def recycle_ports(self, portlist = None, timeout = 2 ):
        """
        Disable and enable a port
        :param timeout: how long to wait before re-enabling
        :return: True if everything OK, False otherwise
        """
        if portlist is None:
            portlist = self.ports()
        #
        result = self.disable_ports( portlist )
        #
        import time
        time.sleep( timeout )
        #
        result = self.enable_ports( portlist ) and result
        #
        return result


#################################

def find_all_hubs(vid):
    """
    Yields all hub port numbers
    """
    # example for Acroname hub:
    # 24ff:8013 =
    #   iManufacturer           Acroname Inc.
    #   iProduct                USBHub3p-3[A]
    from rspy import lsusb
    hubs = set( lsusb.devices_by_vendor( vid ))
    ports = set()
    # go thru the tree and find only the top-level ones (which we should encounter first)
    for dev,dev_port in lsusb.tree():
        if dev not in hubs:
            continue
        for port in ports:
            # ignore everything inside existing hubs - we only want the top-level
            if dev_port.startswith( port + '.' ):
                break
        else:
            ports.add( dev_port )
            yield dev_port


def create():
    return _find_active_hub()


def _find_active_hub():
    """
    Function finds an available hub to connect to and returns it
    """
    active_hubs = []
    acroname_hub = _create_acroname()
    if acroname_hub:
        active_hubs.append(acroname_hub)
        pass

    ykush_hub = _create_ykush()
    if ykush_hub:
        active_hubs.append(ykush_hub)

    unifi_hub = _create_unifi()
    if unifi_hub:
        active_hubs.append(unifi_hub)

    if len(active_hubs) > 1:
        return _create_combined_hubs(active_hubs)
    if len(active_hubs) == 1:
        return active_hubs[0]

    import sys
    log.d('sys.path=', sys.path)
    return None


def _create_acroname():
    try:
        from rspy import acroname
    except ModuleNotFoundError:
        return None  # package not installed
    except Exception as e:
        log.w(f"acroname import failed: {e}")
        return None
    try:
        return acroname.Acroname()
    except acroname.NoneFoundError:
        return None  # no hub connected
    except Exception as e:
        log.w(f"acroname hub init failed: {e}")
        return None


def _create_ykush():
    try:
        from rspy import ykush
    except ModuleNotFoundError:
        return None  # package not installed
    except Exception as e:
        log.w(f"ykush import failed: {e}")
        return None
    try:
        return ykush.Ykush()
    except ykush.NoneFoundError:
        return None  # no hub connected
    except Exception as e:
        log.w(f"ykush hub init failed: {e}")
        return None

def _create_unifi():
    try:
        from rspy import unifi
    except ModuleNotFoundError:
        return None  # package not installed
    except Exception as e:
        log.w(f"unifi import failed: {e}")
        return None
    try:
        return unifi.UniFiSwitch()
    except unifi.NoneFoundError:
        return None  # no hub connected
    except Exception as e:
        log.w(f"unifi hub init failed: {e}")
        return None

def _create_combined_hubs(hub_list):
    """
    Function creates a combined hub from the list of hubs
    :param hub_list: list of hubs
    :return: combined hub
    """
    from rspy import combined_hub
    return combined_hub.CombinedHub(hub_list)

