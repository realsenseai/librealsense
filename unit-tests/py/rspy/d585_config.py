# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
D585 sensor configurations: 3C (dedicated color sensor, the lab default) and 2C (dual RGB).

A device_each() pattern naming the 2C product ("... Dual RGB") also selects a 3C camera that can switch to it, as
the parameter "<serial>-2c". conftest.module_device_setup switches the camera to the mode each module asked for.
"""

from rspy import log

MODES = { '3c': 0, '2c': 1 }
SUFFIX = '-2c'

# product name in 3C -> the name the same camera has in 2C
TWO_C_NAMES = { 'D585 Prototype': 'D585 Proto Dual RGB', 'D585': 'D585 Dual RGB' }


def split( serial ):
    """'541-2c' -> ('541', '2c'); '541' -> ('541', '3c')"""
    return ( serial[:-len( SUFFIX )], '2c' ) if serial.endswith( SUFFIX ) else ( serial, '3c' )


def switchable_to( pattern ):
    """:return: serials of the 3C cameras whose 2C name matches a "Dual RGB" pattern"""
    from rspy import devices
    p = pattern.upper()
    return [ sn for sn in devices.all()
             if 'DUAL RGB' in p and p in TWO_C_NAMES.get( devices.get( sn ).name, '' ).upper() ]


def _option():
    import pyrealsense2 as rs
    return getattr( rs.option, 'sensors_config_mode', None )


def _supports_switch( device ):
    opt = _option()
    return opt is not None and device.handle.first_depth_sensor().supports( opt )


def can_toggle( device ):
    """True if this rspy.devices.Device is a D585 that can switch between 3C and 2C"""
    return device.name in TWO_C_NAMES and _supports_switch( device )


def adopt_3c_names():
    """A camera left in 2C (e.g. by an aborted run) is listed as its 3C self; its first setup switches it back.
    Call right after devices.query(), while the cameras are still powered"""
    from rspy import devices
    three_c_names = { v: k for k, v in TWO_C_NAMES.items() }
    for sn in devices.all():
        device = devices.get( sn )
        if device.name in three_c_names and _supports_switch( device ):
            log.d( f'{sn}: found in 2C, treating as {three_c_names[device.name]}' )
            device._name = three_c_names[device.name]


def current( handle ):
    """:return: '3c' or '2c' -- the configuration the camera is in now"""
    value = int( handle.first_depth_sensor().get_option( _option() ))
    return { v: k for k, v in MODES.items() }[value]


def ensure( serial, key ):
    """
    Put the camera with this serial into configuration `key`, resetting it if it is not there already.
    :return: True if a switch was needed
    """
    from rspy import devices
    handle = devices.get( serial ).handle
    if current( handle ) == key:
        return False
    log.d( f'{serial}: switching to {key.upper()}' )
    handle.first_depth_sensor().set_option( _option(), MODES[key] )
    if not devices.hw_reset( [serial] ):
        raise RuntimeError( f'{serial} did not come back after switching to {key.upper()}' )
    if current( devices.get( serial ).handle ) != key:
        raise RuntimeError( f'{serial} is not in {key.upper()} after the switch' )
    return True
