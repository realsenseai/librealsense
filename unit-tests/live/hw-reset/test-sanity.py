# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

# test:device each(D400*) !D457  # D457 device is known for HW reset issues..
# test:device each(D500*)

import pyrealsense2 as rs
from rspy import test, log, devices
from rspy.timer import Timer
import time

# hw reset test, we want to make sure the device disconnect & reconnect successfully

dev = None
device_removed = False
device_added = False
device_removed_time = 0
device_added_time = 0

def device_changed( info ):
    global dev, device_removed, device_added, device_removed_time, device_added_time
    if info.was_removed(dev):
        device_removed_time = time.perf_counter()
        device_removed = True
    for new_dev in info.get_new_devices():
        added_sn = new_dev.get_info(rs.camera_info.serial_number)
        tested_dev_sn = dev.get_info(rs.camera_info.serial_number)
        if added_sn == tested_dev_sn:
            device_added_time = time.perf_counter()
            device_added = True


################################################################################################
test.start( "HW reset - verify disconnect & reconnect" )

t = Timer( 10 )
dev, ctx = test.find_first_device_or_exit()
ctx.set_devices_changed_callback( device_changed )
time.sleep(1)
log.i( "Sending HW-reset command" )
dev.hardware_reset()

log.i( "Pending for device removal" )
t.start()
while not t.has_expired():
    if (device_removed):
        break
    time.sleep( 0.1 )

test.check(device_removed)

log.i("Pending for device addition")
t = Timer( devices.MAX_ENUMERATION_TIME )
t.start()
while not t.has_expired():
    if ( device_added ):
        break
    time.sleep(0.1)

test.check( device_added )
if device_added_time:
    log.i( "Device reset cycle took", device_added_time - device_removed_time, "[sec]" )
else:
    log.e( "Device not connected back after ", t.get_elapsed() , "[sec]" )
    log.i( "Querying there are", len( ctx.query_devices() ), "devices" )
test.finish()

################################################################################################
test.print_results_and_exit()