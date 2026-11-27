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

_switched = set()   # cameras switched away from the dedicated sensor this session, restored by restore_all()
_last_mode = {}     # camera serial -> the configuration it was last put in; a camera not listed is in the dedicated sensor

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


def real_serial( serial ):
    """the camera serial of a test instance value (a list or any non-twin value is returned as is)"""
    return split( serial )[0] if isinstance( serial, str ) else serial


def extra_instances( serial, exclude_patterns, exclude_types=() ):
    """:return: the extra test instances of a camera -- one in dual RGB, unless an exclude pattern (device_exclude marker or
    --exclude-device) covers its dual RGB name, or the camera's connection type is in exclude_types"""
    from rspy import devices
    device = devices.get( serial )
    if not device or not can_toggle( device ) or ( device.connection_type or '' ).upper() in exclude_types:
        return []
    name = dual_rgb_name( device ).upper()
    return [] if any( p.strip( '*' ).upper() in name for p in exclude_patterns ) else [serial + SUFFIX]


def test_id( serial ):
    """the readable part of a test instance id, e.g. 'D585 Proto Dual RGB-<serial>-dual-rgb'"""
    from rspy import devices
    real, mode = split( serial )
    device = devices.get( real )
    return f'{dual_rgb_name( device ) if mode == "dual_rgb" else device.name}-{serial}' if device else serial


def runs_last( serial ):
    """True for the test instances that run at the end of the session, after every dedicated sensor one"""
    return isinstance( serial, str ) and split( serial )[1] == 'dual_rgb'


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


def mode_changes( serial ):
    """True if setting up this test instance switches its camera, so its handle still describes the previous configuration"""
    if not isinstance( serial, str ):
        return False
    real, mode = split( serial )
    return mode != _last_mode.get( real, 'dedicated_sensor' )


def setup( serial ):
    """Put the camera of this test instance into its configuration (the camera must be enabled)"""
    real, mode = split( serial ) if isinstance( serial, str ) else ( serial, 'dedicated_sensor' )
    from rspy import devices
    device = devices.get( real ) if isinstance( real, str ) else None
    if device and can_toggle( device ):
        if mode == 'dual_rgb':
            _switched.add( real )   # before the switch: a failed one still gets restored
        ensure( real, mode )
        _last_mode[real] = mode
    elif mode == 'dual_rgb':
        raise RuntimeError( 'sensors_config_mode is not supported' )


def restore_all():
    """Put every camera switched this session back to the dedicated sensor, so the next job finds it as usual.
    Call once at session end"""
    from rspy import devices
    for sn in sorted( _switched ):
        try:
            devices.enable_only( [sn] )
            ensure( sn, 'dedicated_sensor' )
        except Exception as e:
            log.warning( f'{sn}: failed to restore the dedicated sensor configuration: {e}' )
    _switched.clear()
    _last_mode.clear()


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
