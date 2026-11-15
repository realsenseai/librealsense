# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
D585 sensor configurations: 3C (dedicated color sensor, the lab default) and 2C (dual RGB).

A D585 that supports the switch is also exposed by rspy.devices as a "2C variant": a second device entry that
shares the physical camera (port, handle) but carries the 2C product name, so that tests selecting
``device_each("Dual RGB")`` run on it. The pytest harness switches the camera to 2C when such a variant is set
up and back to 3C when it is torn down, see conftest.module_device_setup.
"""

from rspy import log

MODES = { '3c': 0, '2c': 1 }
SUFFIX = '-2c'

# product name in 3C -> the name the same camera has in 2C
TWO_C_NAMES = { 'D585 Prototype': 'D585 Proto Dual RGB', 'D585': 'D585 Dual RGB' }


def _option():
    import pyrealsense2 as rs
    return getattr( rs.option, 'sensors_config_mode', None )


def can_toggle( device ):
    """:param device: an rspy.devices.Device (3C name) -- True if the camera can switch to 2C"""
    try:
        opt = _option()
        return device.name in TWO_C_NAMES and opt is not None and device.handle.first_depth_sensor().supports( opt )
    except Exception as e:
        log.d( f'cannot tell if {device.serial_number} can toggle: {e}' )
        return False


class Variant:
    """The 2C view of a toggleable D585; everything but the identity is the underlying camera's"""

    def __init__( self, base ):
        self.base = base
        self.name = TWO_C_NAMES[base.name]
        self.serial_number = base.serial_number + SUFFIX
        self.config = '2c'

    def __getattr__( self, attr ):   # port, handle, enabled, product_line, connection_type, ...
        return getattr( self.base, attr )

    @property
    def real_serial( self ):
        return self.base.serial_number


def name_matches( pattern, variant ):
    """Variants answer only to patterns that name them ("Dual RGB"), never to "D585" or "D500*" -- those mean the camera as such"""
    p = pattern.upper()
    return 'DUAL RGB' in p and p in variant.name.upper()


def current( device ):
    """:return: '3c' or '2c' -- the configuration the camera is in now"""
    value = int( device.first_depth_sensor().get_option( _option() ))
    return next( k for k, v in MODES.items() if v == value )


def ensure( serial, key ):
    """
    Put the camera with this (real) serial into configuration `key`, resetting it if it is not there already.
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
