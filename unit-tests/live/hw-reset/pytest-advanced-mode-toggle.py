# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
from rspy.timer import Timer
import time
import logging
log = logging.getLogger(__name__)

# Verify that toggling advanced mode ON/OFF causes the device to reconnect
# and that the state is correctly applied and reversible.

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.context("nightly"),
]

TOGGLE_WAIT_TIME  = 30  # [sec] max wait for device to reconnect after advanced mode toggle
TOGGLE_ITERATIONS =  3  # number of ON→OFF→ON cycles

dev = None
target_sn = None   # cached before toggle — the removed dev handle cannot be queried safely
device_added = False


def device_changed( info ):
    global dev, device_added
    for candidate in info.get_new_devices():
        try:
            if candidate.get_info( rs.camera_info.serial_number ) == target_sn:
                dev = candidate   # update handle to the newly enumerated instance
                device_added = True
        except RuntimeError:
            continue


def _wait_for_reconnect( timeout ):
    """Wait up to *timeout* seconds for the device to reappear. Returns True if it did."""
    t = Timer( timeout )
    t.start()
    while not t.has_expired():
        if device_added:
            return True
        time.sleep( 0.1 )
    return False


def test_advanced_mode_toggle( test_device ):
    global dev, target_sn, device_added
    device_added = False

    dev, ctx = test_device
    target_sn = dev.get_info( rs.camera_info.serial_number )
    name = dev.get_info( rs.camera_info.name )

    try:
        am_dev = rs.rs400_advanced_mode( dev )
    except Exception as e:
        pytest.skip( f"Advanced mode not supported on {name}: {e}" )

    ctx.set_devices_changed_callback( device_changed )

    initial_state = am_dev.is_enabled()
    toggled_state = not initial_state
    log.info( "Device: %s | Initial advanced mode: %s | Iterations: %d",
              name, "ON" if initial_state else "OFF", TOGGLE_ITERATIONS )

    try:
        for i in range( 1, TOGGLE_ITERATIONS + 1 ):
            log.info( "[%d/%d] Toggling advanced mode to %s",
                      i, TOGGLE_ITERATIONS, "ON" if toggled_state else "OFF" )
            device_added = False
            am_dev.toggle_advanced_mode( toggled_state )

            log.info( "[%d/%d] Waiting up to %d sec for device to reconnect...",
                      i, TOGGLE_ITERATIONS, TOGGLE_WAIT_TIME )
            assert _wait_for_reconnect( TOGGLE_WAIT_TIME ), \
                f"[{i}/{TOGGLE_ITERATIONS}] Device did not reconnect within {TOGGLE_WAIT_TIME} sec after toggling advanced mode"

            toggled_enabled = rs.rs400_advanced_mode( dev ).is_enabled()
            assert toggled_enabled == toggled_state, \
                f"[{i}/{TOGGLE_ITERATIONS}] Expected advanced mode {'ON' if toggled_state else 'OFF'} after toggle but got {'ON' if toggled_enabled else 'OFF'}"
            log.info( "[%d/%d] Device reconnected; advanced mode is %s",
                      i, TOGGLE_ITERATIONS, "ON" if toggled_state else "OFF" )

            log.info( "[%d/%d] Toggling advanced mode back to %s",
                      i, TOGGLE_ITERATIONS, "ON" if initial_state else "OFF" )
            device_added = False
            rs.rs400_advanced_mode( dev ).toggle_advanced_mode( initial_state )

            log.info( "[%d/%d] Waiting up to %d sec for device to reconnect after restore...",
                      i, TOGGLE_ITERATIONS, TOGGLE_WAIT_TIME )
            assert _wait_for_reconnect( TOGGLE_WAIT_TIME ), \
                f"[{i}/{TOGGLE_ITERATIONS}] Device did not reconnect within {TOGGLE_WAIT_TIME} sec after restoring advanced mode"

            am_dev = rs.rs400_advanced_mode( dev )
            restored_enabled = am_dev.is_enabled()
            assert restored_enabled == initial_state, \
                f"[{i}/{TOGGLE_ITERATIONS}] Expected advanced mode {'ON' if initial_state else 'OFF'} after restore but got {'ON' if restored_enabled else 'OFF'}"
            log.info( "[%d/%d] Advanced mode restored to %s", i, TOGGLE_ITERATIONS, "ON" if initial_state else "OFF" )

    finally:
        # Best-effort restore initial state so later tests in the session are not affected
        try:
            if dev and rs.rs400_advanced_mode( dev ).is_enabled() != initial_state:
                log.info( "Restoring advanced mode to %s", "ON" if initial_state else "OFF" )
                device_added = False
                rs.rs400_advanced_mode( dev ).toggle_advanced_mode( initial_state )

                log.info( "Waiting up to %d sec for device to reconnect after restore...", TOGGLE_WAIT_TIME )
                if not _wait_for_reconnect( TOGGLE_WAIT_TIME ):
                    log.warning( "Device did not reconnect within %d sec after restoring advanced mode", TOGGLE_WAIT_TIME )
                else:
                    restored_enabled = rs.rs400_advanced_mode( dev ).is_enabled()
                    if restored_enabled != initial_state:
                        log.warning( "Advanced mode restore: expected %s but got %s",
                                     "ON" if initial_state else "OFF", "ON" if restored_enabled else "OFF" )
                    else:
                        log.info( "Advanced mode restored to %s", "ON" if initial_state else "OFF" )
        except Exception as e:
            log.warning( "Best-effort advanced mode restore failed for %s: %s", name, e )
