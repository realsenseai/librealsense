# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
from rspy import devices
from rspy.timer import Timer
import time
import logging
log = logging.getLogger(__name__)

# hw reset test, we want to make sure the device disconnect & reconnect successfully

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
]

dev = None
target_sn = None   # cached once before hardware_reset() - the removed dev handle cannot be queried safely
device_removed = False
device_added = False
device_removed_time = 0
device_added_time = 0

def device_changed( info ):
    global dev, device_removed, device_added, device_removed_time, device_added_time
    if dev and info.was_removed( dev ):
        device_removed_time = time.perf_counter()
        device_removed = True
    for new_dev in info.get_new_devices():
        if new_dev.get_info( rs.camera_info.serial_number ) == target_sn:
            device_added_time = time.perf_counter()
            device_added = True


def test_hw_reset_sanity( test_device ):
    global dev, target_sn, device_removed, device_added, device_removed_time, device_added_time
    device_removed = False
    device_added = False
    device_removed_time = 0
    device_added_time = 0

    t = Timer( 10 )
    dev, ctx = test_device
    target_sn = dev.get_info( rs.camera_info.serial_number )
    ctx.set_devices_changed_callback( device_changed )
    time.sleep(1)
    log.info( "Sending HW-reset command" )
    dev.hardware_reset()

    log.info( "Pending for device removal" )
    t.start()
    while not t.has_expired():
        if (device_removed):
            break
        time.sleep( 0.1 )

    assert device_removed, "device was not removed after hardware_reset"

    log.info( "Pending for device addition" )
    t = Timer( devices.MAX_ENUMERATION_TIME )
    t.start()
    while not t.has_expired():
        if ( device_added ):
            break
        time.sleep(0.1)

    if device_added_time:
        log.info( "Device reset cycle took %s [sec]", device_added_time - device_removed_time )
    else:
        log.error( "Device not connected back after %s [sec]", t.get_elapsed() )
        log.info( "Querying there are %s devices", len( ctx.query_devices() ) )

    assert device_added, "device did not re-appear within MAX_ENUMERATION_TIME"
