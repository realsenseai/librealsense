# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
from rspy import devices
from rspy.timer import Timer
import platform
import time
import logging
log = logging.getLogger(__name__)

# hw reset test, we want to make sure the device disconnect & reconnect successfully

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
]

MAX_ENUM_TIME_D400 = 7  # [sec] tight per-D400 KPI to catch enumeration regressions
ENUM_WAIT_BUFFER = 5    # [sec] keep waiting past the limit so a near miss still logs its time


def sensor_names( device ):
    return sorted( s.get_info( rs.camera_info.name ) for s in device.query_sensors() )


def wait_for( condition, timeout ):
    t = Timer( timeout )
    t.start()
    while not condition() and not t.has_expired():
        time.sleep( 0.1 )


@pytest.fixture( scope="module" )
def reset_cycle( test_device ):
    """One hardware_reset per camera; records what the tests below assert on."""
    dev, ctx = test_device
    r = dict( sn=dev.get_info( rs.camera_info.serial_number ),  # cached: the removed handle cannot be queried safely
              product_line=dev.get_info( rs.camera_info.product_line ),
              connection_type=dev.get_info( rs.camera_info.connection_type ) if dev.supports( rs.camera_info.connection_type ) else None,
              expected_sensors=sensor_names( dev ), removed=None, added=None, added_sensors=None )
    log.info( "Sensors before reset: %s", r['expected_sensors'] )

    def device_changed( info ):
        if r['removed'] is None and info.was_removed( dev ):
            r['removed'] = time.perf_counter()
        for new_dev in info.get_new_devices():
            if r['added'] is None and new_dev.get_info( rs.camera_info.serial_number ) == r['sn']:
                r['added'] = time.perf_counter()
                # Sampled here, not after the test wakes up: a device published before all
                # of its interfaces have enumerated is repaired by a later event.
                r['added_sensors'] = sensor_names( new_dev )

    ctx.set_devices_changed_callback( device_changed )
    time.sleep( 1 )
    log.info( "Sending HW-reset command" )
    r['reset'] = time.perf_counter()
    dev.hardware_reset()
    wait_for( lambda: r['removed'], 10 )
    if r['removed']:
        wait_for( lambda: r['added'], devices.MAX_ENUMERATION_TIME + ENUM_WAIT_BUFFER )
    if not r['added']:
        log.info( "Querying there are %s devices", len( ctx.query_devices() ) )
    yield r
    ctx.set_devices_changed_callback( lambda info: None )  # drop the closure's dev, or ctx teardown hangs at exit


def test_device_removed( reset_cycle ):
    r = reset_cycle
    latency = r['removed'] and r['removed'] - r['reset']
    if r['removed']:
        log.info( "Removal latency: %.2f [sec]", latency )
    assert r['removed'], "device was not removed within 10s of hardware_reset"
    # Regression guard for the Windows UVC+HID watcher gate that used to defer removal up to 15s.
    # DDS devices don't go through this gate and have their own reset-to-enumeration timing.
    if platform.system() == "Windows" and r['connection_type'] != "DDS":
        assert latency < 2.5, f"removal took {latency:.2f}s — expected under 2.5s on Windows USB"


def test_device_reenumerated( reset_cycle ):
    r = reset_cycle
    if not r['removed']:
        pytest.skip( "device was not removed" )
    limit = MAX_ENUM_TIME_D400 if r['product_line'] == "D400" else devices.MAX_ENUMERATION_TIME
    enum_time = r['added'] and r['added'] - r['reset']
    if r['added']:
        log.info( "Device reset cycle took %.2f [sec] (limit %s [sec])", enum_time, limit )
    assert r['added'], f"device did not re-appear within {devices.MAX_ENUMERATION_TIME + ENUM_WAIT_BUFFER}s of removal"
    assert enum_time < limit, f"re-enumerated {enum_time:.2f}s after reset, expected under {limit}s"


def test_sensors_after_reset( reset_cycle ):
    """The device must come back whole; a missing sensor is typically the Motion Module (no IMU)."""
    r = reset_cycle
    if not r['added']:
        pytest.skip( "device did not re-enumerate" )
    log.info( "Sensors after reset: %s", r['added_sensors'] )
    assert r['added_sensors'] == r['expected_sensors'], \
        f"device re-enumerated with {r['added_sensors']}, expected {r['expected_sensors']}"
