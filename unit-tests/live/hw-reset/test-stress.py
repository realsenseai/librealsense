# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# test:device each(D400*)
# test:device each(D500*)
# test:donotrun:!nightly
# test:timeout 360
# test:timeout:weekly 3600

import pyrealsense2 as rs
from rspy import test, log
from rspy.timer import Timer
import time

# HW-reset stress test: repeatedly reset the device and verify it reconnects each time.
# Iterations depend on context:
#   weekly:  USB/GMSL = 100 (STRESS_ITERATIONS),        DDS =  50 (STRESS_ITERATIONS_DDS)
#   nightly: USB/GMSL =  10 (STRESS_ITERATIONS_NIGHTLY), DDS =   5 (STRESS_ITERATIONS_NIGHTLY_DDS)

STRESS_ITERATIONS              = 100
STRESS_ITERATIONS_DDS          =  50
STRESS_ITERATIONS_NIGHTLY      =  10
STRESS_ITERATIONS_NIGHTLY_DDS  =   5
REMOVAL_TIMEOUT        = 10   # [sec] max wait for device to disappear
MAX_ENUM_TIME_D400     = 10   # [sec] increased vs single-shot KPI to allow for slower reconnects after rapid resets
MAX_ENUM_TIME_D500     = 15   # [sec]
MAX_ENUM_TIME_D500_DDS = 18   # [sec] extra time for DDS discovery / initialization

dev             = None   # current live handle — used for both was_removed() matching and hardware_reset()
device_removed  = False
device_added    = False
new_dev_handle  = None   # updated by callback so each iteration gets the fresh handle


def device_changed( info ):
    global dev, device_removed, device_added, new_dev_handle
    if info.was_removed( dev ):
        device_removed = True
    for candidate in info.get_new_devices():
        try:
            added_sn  = candidate.get_info( rs.camera_info.serial_number )
            tested_sn = dev.get_info( rs.camera_info.serial_number )
        except RuntimeError:
            continue
        if added_sn == tested_sn:
            new_dev_handle = candidate
            device_added   = True


def get_max_enum_time( d ):
    pl = d.get_info( rs.camera_info.product_line )
    if pl == "D400":
        return MAX_ENUM_TIME_D400
    if pl == "D500":
        is_dds = ( d.supports( rs.camera_info.connection_type )
                   and d.get_info( rs.camera_info.connection_type ) == "DDS" )
        return MAX_ENUM_TIME_D500_DDS if is_dds else MAX_ENUM_TIME_D500
    return MAX_ENUM_TIME_D400  # safe fallback


################################################################################################
test.start( "HW reset stress test" )

dev, ctx = test.find_first_device_or_exit()
ctx.set_devices_changed_callback( device_changed )

is_dds      = ( dev.supports( rs.camera_info.connection_type )
                and dev.get_info( rs.camera_info.connection_type ) == "DDS" )
is_weekly   = 'weekly' in test.context
if is_weekly:
    iterations = STRESS_ITERATIONS_DDS          if is_dds else STRESS_ITERATIONS
else:
    iterations = STRESS_ITERATIONS_NIGHTLY_DDS  if is_dds else STRESS_ITERATIONS_NIGHTLY
max_enum    = get_max_enum_time( dev )
conn_type   = "DDS" if is_dds else "USB/GMSL"

log.i( f"Running {iterations} HW-reset iterations on {conn_type} device "
       f"({'weekly' if is_weekly else 'nightly'} context, max reconnect time: {max_enum} [sec])" )

time.sleep( 1 )  # let the device settle before the first reset

failed_removal   = []
failed_reconnect = []

for i in range( 1, iterations + 1 ):
    device_removed = False
    device_added   = False
    new_dev_handle = None

    log.d( f"[{i}/{iterations}] Sending HW-reset" )
    dev.hardware_reset()

    # --- wait for removal ---
    t = Timer( REMOVAL_TIMEOUT )
    t.start()
    while not t.has_expired():
        if device_removed:
            break
        time.sleep( 0.05 )

    if not device_removed:
        log.e( f"[{i}/{iterations}] Device did not disconnect within {REMOVAL_TIMEOUT} [sec]" )
        failed_removal.append( i )
        # Cannot safely continue this iteration — abort the whole stress run
        break

    # --- wait for reconnect ---
    t = Timer( max_enum )
    t.start()
    while not t.has_expired():
        if device_added:
            break
        time.sleep( 0.05 )

    if not device_added:
        log.e( f"[{i}/{iterations}] Device did not reconnect within {max_enum} [sec]" )
        failed_reconnect.append( i )
        # Cannot continue — no valid device handle for further resets
        break

    # Update dev to the fresh handle so was_removed() and hardware_reset() stay in sync
    dev = new_dev_handle
    log.d( f"[{i}/{iterations}] OK" )

log.i( f"Completed {i} of {iterations} iterations" )

if failed_removal:
    log.e( "Iterations with missing removal:", failed_removal )
if failed_reconnect:
    log.e( "Iterations with missing reconnect:", failed_reconnect )

test.check( len( failed_removal )   == 0, f"{len(failed_removal)} iteration(s) failed on removal"   )
test.check( len( failed_reconnect ) == 0, f"{len(failed_reconnect)} iteration(s) failed on reconnect" )
test.check( i == iterations,              f"Stress run aborted early at iteration {i}/{iterations}"  )

test.finish()

################################################################################################
test.print_results_and_exit()
