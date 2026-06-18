# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Hub-recycle stress test: power-cycles the USB port the device under test is
# connected to (true cold plug-in via the OS, not an in-band hardware_reset),
# then verifies that the re-enumerated device exposes its full sensor set,
# in particular the Motion Module on IMU-bearing devices.
#
# This catches the partial-enumeration race where, on Windows, the device-
# watcher's debounce fires a "device added" callback before the HID Sensor
# Collection has bound at T0+~1s, surfacing a UVC-only device with no IMU and
# triggering "No HID info provided, IMU is disabled" / "HID Motion Sensor
# Failure! bad optional access" in the log.
#
# Uses the existing rspy.devices.enable_only(recycle=True) flow so the hub
# singleton and per-device port resolution are reused from the test fixtures.

import logging

import pytest
import pyrealsense2 as rs
from rspy import devices

log = logging.getLogger(__name__)


pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
    pytest.mark.device_type("USB"),   # hub-recycle is meaningful only on USB
    pytest.mark.context("nightly"),
    pytest.mark.timeout(300),
]


ITERATIONS_NIGHTLY = 5
ITERATIONS_WEEKLY  = 20


def _sensor_names( dev ):
    return [ s.get_info( rs.camera_info.name ) for s in dev.query_sensors() ]


def _find_device_by_sn( ctx, sn ):
    for d in ctx.query_devices():
        if d.supports( rs.camera_info.serial_number ) and d.get_info( rs.camera_info.serial_number ) == sn:
            return d
    return None


def test_hub_recycle_imu_presence( test_device, test_context_var ):
    dev, ctx = test_device
    sn = dev.get_info( rs.camera_info.serial_number )

    # The device_type("USB") marker filters out non-USB cameras at collection
    # time. The Motion-Module check below is what we can't express as a marker
    # today: skip cleanly for D4xx/D5xx products that don't carry an IMU.
    if not any( "Motion" in n for n in _sensor_names( dev ) ):
        pytest.skip( f"{dev.get_info( rs.camera_info.name )} has no Motion Module - test does not apply" )

    if devices.hub is None:
        pytest.skip( "no hub configured - hub-recycle requires a real port power-cycle; "
                     "enable_only(recycle=True) would fall back to an in-band hw_reset, "
                     "which defeats the purpose of this test" )
    if devices.get( sn ).port is None:
        pytest.fail( f"Hub is present but could not resolve a port for serial {sn} - "
                     "refusing to recycle all ports to avoid disturbing other devices on the hub." )

    iterations = ITERATIONS_WEEKLY if 'weekly' in test_context_var else ITERATIONS_NIGHTLY
    log.info( f"Hub-recycle IMU-presence test: {iterations} iterations on serial {sn}, "
              f"hub port {devices.get( sn ).port}" )

    missing_imu = []
    no_reappear = []

    for i in range( 1, iterations + 1 ):
        log.debug( f"[{i}/{iterations}] recycling port" )
        try:
            # Recycles just this device's port (disable -> wait_until_removed
            # -> enable -> wait_for re-enumeration), so other devices on the
            # same hub are not disturbed.
            devices.enable_only( [sn], recycle=True, timeout=devices.MAX_ENUMERATION_TIME )
        except Exception as e:
            log.error( f"[{i}/{iterations}] enable_only failed: {e}" )
            no_reappear.append( i )
            continue

        new_dev = _find_device_by_sn( ctx, sn )
        if new_dev is None:
            log.error( f"[{i}/{iterations}] device with serial {sn} not in context after recycle" )
            no_reappear.append( i )
            continue

        try:
            names = _sensor_names( new_dev )
        except RuntimeError as e:
            log.error( f"[{i}/{iterations}] query_sensors raised: {e}" )
            missing_imu.append( i )
            continue

        if any( "Motion" in n for n in names ):
            log.debug( f"[{i}/{iterations}] OK: {names}" )
        else:
            log.error( f"[{i}/{iterations}] Motion Module MISSING: {names}" )
            missing_imu.append( i )

    if no_reappear:
        log.error( f"Iterations that did not re-enumerate: {no_reappear}" )
    if missing_imu:
        log.error( f"Iterations with missing Motion Module: {missing_imu}" )

    assert not no_reappear, f"{len(no_reappear)}/{iterations} iterations: device did not re-enumerate"
    assert not missing_imu, f"{len(missing_imu)}/{iterations} iterations: device re-enumerated without Motion Module"
