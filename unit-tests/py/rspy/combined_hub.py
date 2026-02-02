# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

from rspy import device_hub, log
import time


class CombinedHub(device_hub.device_hub):
    """
    Combine multiple device_hub instances into a single unified port interface.
    """
    def __init__(self, hubs):
        """
        :param hubs: list of device_hub instances
        """
        # Use each hub's own name for clarity
        self.hubs = {hub.get_name(): hub for hub in hubs}  # assuming no duplicate hubs
        self.virtual_port_to_hub_port = {}  # virtual port -> (hub_name, real port)
        self.hub_port_to_virtual_port = {}  # (hub_name, real port) -> virtual port
        self._assign_ports()

    def _assign_ports(self):
        # Since we hold multiple hubs, which might have overlapping port numbers,
        # we need to assign our own port numbers to each hub's ports
        port_number = 0
        for hub_name, hub in self.hubs.items():
            hub_ports = hub.all_ports()
            for port in hub_ports:
                name_and_port = (hub_name, port)
                self.virtual_port_to_hub_port[port_number] = name_and_port
                self.hub_port_to_virtual_port[name_and_port] = port_number
                # log.d("Assigning virtual port", port_number, "to hub", hub_name, "port", port)
                port_number += 1

    def get_name(self):
        """Return the name of this virtual hub."""
        return "CombinedHub"

    def connect(self, reset=False):
        """Connect all underlying hubs."""
        for hub in self.hubs.values():
            hub.connect(reset)

    def is_connected(self):
        """:return: True if every hub is connected"""
        return all(hub.is_connected() for hub in self.hubs.values())

    def disconnect(self):
        """Disconnect all underlying hubs."""
        for hub in self.hubs.values():
            hub.disconnect()

    def all_ports(self):
        """:return: list of all virtual port numbers"""
        return list(self.virtual_port_to_hub_port)

    def ports(self):
        """:return: list of currently enabled virtual ports"""
        enabled = []
        for name, hub in self.hubs.items():
            hub_ports = hub.ports()  # this returns the real port numbers
            for port in hub_ports:
                virtual_port = self.hub_port_to_virtual_port[(name, port)]
                enabled.append(virtual_port)
        return enabled

    def is_port_enabled(self, port):
        """
        :param port: virtual port number
        :return: True if the corresponding hub port is enabled
        """
        if port not in self.virtual_port_to_hub_port:
            raise ValueError(f"Unknown port number: {port}")
        hub_name, hub_port = self.virtual_port_to_hub_port[port]
        return self.hubs[hub_name].is_port_enabled(hub_port)

    def port_state(self, port):
        """
        :param port: virtual port number
        :return: the state string from the underlying hub
        """
        if port not in self.virtual_port_to_hub_port:
            raise ValueError(f"Unknown port number: {port}")
        hub_name, hub_port = self.virtual_port_to_hub_port[port]
        return self.hubs[hub_name].port_state(hub_port)

    def enable_ports(self, ports=None, disable_other_ports=False, sleep_on_change=0):
        """
        Enable the specified virtual ports.
        :param ports: list of virtual ports to enable; None to enable all
        :param disable_other_ports: if True, disable any non-requested ports on each hub
        :param sleep_on_change: seconds to sleep if any change was made
        :return: True if all hubs succeed
        """
        # Build per-hub targets
        if ports is None:
            targets = {name: None for name in self.hubs}  # all ports will be enabled
        else:
            targets = {name: [] for name in self.hubs}
            for p in ports:
                if p not in self.virtual_port_to_hub_port:
                    raise ValueError(f"Unknown port number: {p}")
                hub_name, hub_port = self.virtual_port_to_hub_port[p]
                targets[hub_name].append(hub_port)

        success = True
        for name, hub in self.hubs.items():
            ok = hub.enable_ports(targets[name], disable_other_ports)
            success = success and ok

        # We wait for the maximum time allowed for enumeration changes.
        if sleep_on_change and success:
            time.sleep(sleep_on_change)
        return success

    def disable_ports(self, ports=None, sleep_on_change=0):
        """
        Disable the specified virtual ports.
        :param ports: list of virtual ports to disable; None to disable all
        :param sleep_on_change: seconds to sleep if any change was made
        :return: True if all hubs succeed
        """
        if ports is None:
            targets = {name: None for name in self.hubs}
        else:
            targets = {name: [] for name in self.hubs}
            for p in ports:
                if p not in self.virtual_port_to_hub_port:
                    raise ValueError(f"Unknown port number: {p}")
                hub_name, hub_port = self.virtual_port_to_hub_port[p]
                targets[hub_name].append(hub_port)

        success = True
        for name, hub in self.hubs.items():
            ok = hub.disable_ports(targets[name])
            success = success and ok

        # We wait for the maximum time allowed for enumeration changes.
        if sleep_on_change and success:
            time.sleep(sleep_on_change)

        return success

    def get_port_by_location(self, location):
        """
        :param location: location string, usb location or mac address
        :return: virtual port number or None if not found
        """
        for name, hub in self.hubs.items():
            try:
                port = hub.get_port_by_location(location)  # this hold the REAL port value
            except Exception as e:
                # not an actual error, this will likely happen when multiple hubs are used
                # log.d(location, "not found in hub", name, ":", e)
                continue

            if port is not None:
                return self.hub_port_to_virtual_port[(name, int(port))]
        return None

