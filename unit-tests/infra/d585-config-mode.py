# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Switch a LibCI D585 between its two sensor configurations:

    3c   dedicated color sensor   (sensors_config_mode = 0)   <- the default lab configuration
    2c   dual RGB                 (sensors_config_mode = 1)

Usage:
    infra/d585-config-mode.py set 2c [--serial SN] [--device-map FILE]
    infra/d585-config-mode.py run 2c [--serial SN] [--device-map FILE] -- <command...>

The D585 (not D585S) is taken from --serial, else from this machine's entry in the device map
(default: the LibCI home's device-map.yaml), else it is the one that enumerates. Like a test session,
the tool powers all hub ports, so a camera that is currently off still comes up.

'run' switches, runs the command, and ALWAYS switches back to 3c, also when the command fails or is
interrupted. Exit code: the command's; 1 if a switch failed; 3 if this machine has no D585 (nothing ran).
"""

import argparse
import os
import subprocess
import sys
import time

sys.path.insert( 1, os.path.join( os.path.dirname( os.path.dirname( os.path.abspath( __file__ ) )), 'py' ))
from rspy import log, devices, device_map

rs = devices.rs

MODES = { '3c': 0, '2c': 1 }
MODE_NAMES = { 0: 'Dedicated Color Sensor (3C)', 1: 'Dual RGB (2C)' }
TIMEOUT = 60       # [sec] to wait for the camera to (re)enumerate
EXIT_NO_D585 = 3


def _is_d585( name ):
    return bool( name ) and 'D585' in name and 'D585S' not in name


def _find( serial = None ):
    """:return: the enumerated D585 (the one with this serial, if given), or None"""
    try:
        for d in rs.context().query_devices():
            if d.supports( rs.camera_info.name ) and _is_d585( d.get_info( rs.camera_info.name )) \
                    and (serial is None or d.get_info( rs.camera_info.serial_number ) == serial):
                return d
    except RuntimeError as e:
        log.d( 'query failed:', e )
    return None


def _wait( serial, timeout, mode = None ):
    """:return: the camera once it enumerates (and is in `mode`, if given), or None on timeout"""
    end = time.time() + timeout
    while time.time() < end:
        d = _find( serial )
        try:
            if d is not None and (mode is None or _mode( d ) == mode):
                return d
        except RuntimeError as e:
            log.d( 'camera not ready:', e )
        time.sleep( 1 )
    return None


def _mode( dev ):
    opt = getattr( rs.option, 'sensors_config_mode', None )
    if opt is None:
        raise RuntimeError( 'this pyrealsense2 has no sensors_config_mode option' )
    return int( dev.first_depth_sensor().get_option( opt ))


def inventory_serial( path ):
    """
    :return: (known, serial) -- known is False when there is no map or no entry for this machine;
             serial is this machine's D585 in the map, or None if it has none
    """
    nodes = device_map.load_map( path )
    node = device_map.resolve_node( nodes )
    if node is None:
        return False, None
    for camera in node.get( 'cameras' ) or []:
        if _is_d585( camera.get( 'product' )):
            return True, camera.get( 'sn' )
    return True, None


def set_mode( serial, key, hub ):
    """Switch to `key` ('3c'/'2c') and check it survives a power cycle. Raises on failure."""
    target = MODES[key]
    if hub:
        hub.enable_ports()   # as the test session does
    dev = _wait( serial, TIMEOUT )
    if dev is None:
        raise RuntimeError( f'D585 {serial or ""} did not enumerate' )
    serial = dev.get_info( rs.camera_info.serial_number )
    cur = _mode( dev )
    log.i( f'{serial}: current = {MODE_NAMES.get( cur, cur )}' )
    if cur == target:
        return serial
    dev.first_depth_sensor().set_option( rs.option.sensors_config_mode, target )
    log.i( f'{serial}: set to {MODE_NAMES[target]}, resetting...' )
    dev.hardware_reset()
    time.sleep( 3 )
    if _wait( serial, TIMEOUT, target ) is None:
        raise RuntimeError( f'{serial} did not come back in {MODE_NAMES[target]}' )
    if hub:  # the test session power-cycles the ports: the setting must survive that
        hub.disable_ports( sleep_on_change = 3 )
        hub.enable_ports()
        if _wait( serial, TIMEOUT, target ) is None:
            raise RuntimeError( f'{serial}: {MODE_NAMES[target]} did not survive a power cycle' )
    log.i( f'{serial}: now in {MODE_NAMES[target]}' )
    return serial


def main():
    ap = argparse.ArgumentParser( description = __doc__, formatter_class = argparse.RawDescriptionHelpFormatter )
    sub = ap.add_subparsers( dest = 'cmd', required = True )
    for name in ('set', 'run'):
        p = sub.add_parser( name )
        p.add_argument( 'mode', choices = MODES )
        p.add_argument( '--serial' )
        p.add_argument( '--device-map' )
        if name == 'run':
            p.add_argument( 'command', nargs = argparse.REMAINDER )
    args = ap.parse_args()

    serial = args.serial
    if not serial:
        known, serial = inventory_serial( args.device_map )
        if known and not serial:
            log.i( 'no D585 on this machine; nothing to do' )
            return EXIT_NO_D585

    devices.init_hub()
    hub = devices.hub
    if hub and not hub.is_connected():
        hub.connect()
    try:
        if args.cmd == 'set':
            set_mode( serial, args.mode, hub )
            return 0
        command = args.command[1:] if args.command[:1] == ['--'] else args.command
        if not command:
            sys.exit( '-F- run: no command given' )
        serial = set_mode( serial, args.mode, hub )
        try:
            return subprocess.call( command )
        finally:
            log.i( f'{serial}: restoring 3C' )
            try:
                set_mode( serial, '3c', hub )
            except Exception as e:
                log.e( f'FAILED to restore {serial} to 3C: {e}' )
                sys.exit( 1 )
    except RuntimeError as e:
        log.e( e )
        return 1
    finally:
        if hub:
            try:
                hub.disconnect()
            except Exception:
                pass


if __name__ == '__main__':
    sys.exit( main() )
