# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
D585 sensor configurations: dedicated color sensor (the lab default) and dual RGB.

Every device-marked test that runs on a dedicated sensor camera able to switch also runs on it in dual RGB, as the parameter
"<serial>-dual-rgb" (unless it device_exclude()s the dual RGB name, e.g. "Dual RGB"); a "Dual RGB" pattern selects only that run.
The dual RGB runs come last in the session, see conftest. conftest.module_device_setup switches the camera to the mode
each module asked for.
"""

from rspy import log

MODES = { 'dedicated_sensor': 0, 'dual_rgb': 1 }
SUFFIX = '-dual-rgb'

# product name in dedicated sensor -> the name the same camera has in dual RGB
DUAL_RGB_NAMES = { 'D585 Prototype': 'D585 Proto Dual RGB', 'D585': 'D585 Dual RGB' }


def split( serial ):
    """'541-dual-rgb' -> ('541', 'dual_rgb'); '541' -> ('541', 'dedicated_sensor')"""
    return ( serial[:-len( SUFFIX )], 'dual_rgb' ) if serial.endswith( SUFFIX ) else ( serial, 'dedicated_sensor' )


def switchable_to( pattern ):
    """:return: serials of the dedicated sensor cameras whose dual RGB name matches a "Dual RGB" pattern"""
    from rspy import devices
    p = pattern.upper()
    return [ sn for sn in devices.all()
             if 'DUAL RGB' in p and p in DUAL_RGB_NAMES.get( devices.get( sn ).name, '' ).upper() and can_toggle( devices.get( sn ) ) ]


def _option():
    import pyrealsense2 as rs
    return getattr( rs.option, 'sensors_config_mode', None )


def _supports_switch( device ):
    opt = _option()
    return opt is not None and device.handle.first_depth_sensor().supports( opt )


def dual_rgb_name( device ):
    """:return: the product name of this camera in dual RGB, '' if it has none"""
    return DUAL_RGB_NAMES.get( device.name, '' )


def can_toggle( device ):
    """True if this rspy.devices.Device is a D585 that can switch between dedicated sensor and dual RGB"""
    return device.name in DUAL_RGB_NAMES and _supports_switch( device )


def adopt_dedicated_sensor_names():
    """A camera left in dual RGB (e.g. by an aborted run) is listed as its dedicated sensor self; its first setup switches it back.
    Call right after devices.query(), while the cameras are still powered"""
    from rspy import devices
    dedicated_sensor_names = { v: k for k, v in DUAL_RGB_NAMES.items() }
    for sn in devices.all():
        device = devices.get( sn )
        if device.name in dedicated_sensor_names and _supports_switch( device ):
            log.d( f'{sn}: found in dual RGB, treating as {dedicated_sensor_names[device.name]}' )
            device._name = dedicated_sensor_names[device.name]


def current( handle ):
    """:return: 'dedicated_sensor' or 'dual_rgb' -- the configuration the camera is in now"""
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
