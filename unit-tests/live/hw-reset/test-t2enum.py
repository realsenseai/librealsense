# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

# test:device each(D400*) !D457  # D457 device is known for HW reset issues..
# test:device each(D500*)
# test:donotrun:!nightly

import pyrealsense2 as rs
from rspy import test, log
from rspy.timer import Timer
from rspy.stopwatch import Stopwatch
import time

# Verify reasonable enumeration time for the device

dev = None
device_removed = False
device_added = False
MAX_ENUM_TIME_D400 = 5 # [sec]
MAX_ENUM_TIME_D500 = 15 # [sec]

def device_changed( info ):
    global dev, device_removed, device_added
    if info.was_removed(dev):
        log.out( "Device removal detected at: ", time.perf_counter())
        device_removed = True
    for new_dev in info.get_new_devices():
        added_sn = new_dev.get_info(rs.camera_info.serial_number)
        tested_dev_sn = dev.get_info(rs.camera_info.serial_number)
        if added_sn == tested_dev_sn:
            log.out( "Device addition detected at: ", time.perf_counter())
            device_added = True

def get_max_enum_time_by_device( dev ):
    if dev.get_info( rs.camera_info.product_line ) == "D400":
        return MAX_ENUM_TIME_D400
    elif dev.get_info( rs.camera_info.product_line ) == "D500":
        if dev.get_info( rs.camera_info.connection_type) == "DDS":
            return MAX_ENUM_TIME_D500 + 3  # some extra time for discovery and initialization for DDS
        return MAX_ENUM_TIME_D500
    return 0

################################################################################################
test.start( "HW reset to enumeration time" )

# get max enumeration time per device
dev, ctx = test.find_first_device_or_exit()
ctx.set_devices_changed_callback( device_changed )

max_dev_enum_time = get_max_enum_time_by_device( dev )
time.sleep(1)
log.out( "Sending HW-reset command" )
enumeration_sw = Stopwatch() # we know we add the device removal time, but it shouldn't take long
dev.hardware_reset()

log.out( "Pending for device removal" )
t = Timer( 10 )
t.start()
while not t.has_expired():
    if ( device_removed ):
        break
    time.sleep( 0.1 )

test.check( device_removed and not t.has_expired() ) # verifying we are not timed out

log.out( "Pending for device addition" )
buffer = 5 # we add 5 seconds so if the test pass the creteria by a short amount of time we can print it
t = Timer( max_dev_enum_time + buffer )
r_2_e_time = 0 # reset to enumeration time
while not t.has_expired():
    if ( device_added ):
        r_2_e_time = enumeration_sw.get_elapsed()
        break
    time.sleep(0.1)

test.check( device_added )
test.check( r_2_e_time < max_dev_enum_time )

if r_2_e_time:
  log.d( "Enumeration time took", r_2_e_time, "[sec]" )
else:
  log.e( "Enumeration did not occur in ", max_dev_enum_time + buffer, "[sec]" )

test.finish()

################################################################################################
test.print_results_and_exit()