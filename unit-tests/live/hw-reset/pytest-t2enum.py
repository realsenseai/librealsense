# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
from rspy import devices
from rspy.timer import Timer
from rspy.stopwatch import Stopwatch
import time
import logging
log = logging.getLogger(__name__)

# Verify reasonable enumeration time for the device

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
    pytest.mark.context("nightly"),
]

dev = None
target_sn = None   # cached once before hardware_reset() - the removed dev handle cannot be queried safely
device_removed = False
device_added = False
MAX_ENUM_TIME_D400 = 7 # [sec] tight per-D400 KPI to catch enumeration regressions

def device_changed( info ):
    global dev, device_removed, device_added
    if dev and info.was_removed( dev ):
        log.info( "Device removal detected at: %s", time.perf_counter() )
        device_removed = True
    for new_dev in info.get_new_devices():
        if new_dev.get_info( rs.camera_info.serial_number ) == target_sn:
            log.info( "Device addition detected at: %s", time.perf_counter() )
            device_added = True

def get_max_enum_time_by_device( dev ):
    product_line = dev.get_info( rs.camera_info.product_line )
    if product_line == "D400":
        return MAX_ENUM_TIME_D400
    elif product_line == "D500":
        return devices.MAX_ENUMERATION_TIME  # covers both USB/GMSL and DDS variants
    return 0


def test_hw_reset_to_enumeration_time( test_device ):
    global dev, target_sn, device_removed, device_added
    device_removed = False
    device_added = False

    # get max enumeration time per device
    dev, ctx = test_device
    target_sn = dev.get_info( rs.camera_info.serial_number )
    ctx.set_devices_changed_callback( device_changed )

    max_dev_enum_time = get_max_enum_time_by_device( dev )
    time.sleep(1)
    log.info( "Sending HW-reset command" )
    enumeration_sw = Stopwatch() # we know we add the device removal time, but it shouldn't take long
    dev.hardware_reset()

    log.info( "Pending for device removal" )
    t = Timer( 10 )
    t.start()
    while not t.has_expired():
        if ( device_removed ):
            break
        time.sleep( 0.1 )

    assert device_removed and not t.has_expired() # verifying we are not timed out

    log.info( "Pending for device addition" )
    buffer = 5 # we add 5 seconds so if the test pass the criteria by a short amount of time we can print it
    t = Timer( max_dev_enum_time + buffer )
    r_2_e_time = 0 # reset to enumeration time
    while not t.has_expired():
        if ( device_added ):
            r_2_e_time = enumeration_sw.get_elapsed()
            break
        time.sleep(0.1)

    if r_2_e_time:
        log.debug( "Enumeration time took %s [sec]", r_2_e_time )
    else:
        log.error( "Enumeration did not occur in %s [sec]", max_dev_enum_time + buffer )

    assert device_added
    assert r_2_e_time < max_dev_enum_time
