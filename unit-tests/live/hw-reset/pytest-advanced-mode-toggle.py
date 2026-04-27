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
    pytest.mark.device_each("D500*"),
]

TOGGLE_WAIT_TIME = 30  # [sec] max wait for device to reconnect after advanced mode toggle

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


def _wait_for_reconnect():
    """Wait up to TOGGLE_WAIT_TIME seconds for the device to reappear. Returns True if it did."""
    global device_added
    device_added = False
    t = Timer( TOGGLE_WAIT_TIME )
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
    log.info( "Device: %s | Initial advanced mode: %s", name, "ON" if initial_state else "OFF" )

    # --- Toggle to opposite state ---
    toggled_state = not initial_state
    log.info( "Toggling advanced mode to %s", "ON" if toggled_state else "OFF" )
    am_dev.toggle_advanced_mode( toggled_state )

    log.info( "Waiting up to %d sec for device to reconnect after toggle...", TOGGLE_WAIT_TIME )
    assert _wait_for_reconnect(), \
        f"Device did not reconnect within {TOGGLE_WAIT_TIME} sec after toggling advanced mode"

    toggled_enabled = rs.rs400_advanced_mode( dev ).is_enabled()
    assert toggled_enabled == toggled_state, \
        f"Expected advanced mode {'ON' if toggled_state else 'OFF'} after toggle but got {'ON' if toggled_enabled else 'OFF'}"
    log.info( "Device reconnected; advanced mode is %s", "ON" if toggled_state else "OFF" )

    # --- Toggle back to original state ---
    log.info( "Toggling advanced mode back to %s", "ON" if initial_state else "OFF" )
    rs.rs400_advanced_mode( dev ).toggle_advanced_mode( initial_state )

    log.info( "Waiting up to %d sec for device to reconnect after restore...", TOGGLE_WAIT_TIME )
    assert _wait_for_reconnect(), \
        f"Device did not reconnect within {TOGGLE_WAIT_TIME} sec after restoring advanced mode"

    restored_enabled = rs.rs400_advanced_mode( dev ).is_enabled()
    assert restored_enabled == initial_state, \
        f"Expected advanced mode {'ON' if initial_state else 'OFF'} after restore but got {'ON' if restored_enabled else 'OFF'}"
    log.info( "Advanced mode restored to %s; test passed", "ON" if initial_state else "OFF" )
