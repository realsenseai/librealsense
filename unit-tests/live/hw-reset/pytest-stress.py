# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Fails on D585S and D555

import pytest
import pyrealsense2 as rs
from rspy import devices
from rspy.timer import Timer
from rspy.stopwatch import Stopwatch
import time
import logging
from rspy.snippets import is_dds_dev
log = logging.getLogger(__name__)

# HW-reset stress test: repeatedly reset the device and verify it reconnects each time.
# Iterations depend on context:
#   weekly:  USB/GMSL = 100 (STRESS_ITERATIONS),        DDS =  50 (STRESS_ITERATIONS_DDS)
#   nightly: USB/GMSL =  10 (STRESS_ITERATIONS_NIGHTLY), DDS =   5 (STRESS_ITERATIONS_NIGHTLY_DDS)

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
    pytest.mark.device_exclude("D585S"),
    pytest.mark.device_exclude("D555"),
    pytest.mark.context("nightly"),
    pytest.mark.timeout(3600),
]

STRESS_ITERATIONS              = 100
STRESS_ITERATIONS_DDS          =  50
STRESS_ITERATIONS_NIGHTLY      =  10
STRESS_ITERATIONS_NIGHTLY_DDS  =   5
REMOVAL_TIMEOUT        = 10   # [sec] max wait for any device event after reset

dev             = None   # current live handle - used for hardware_reset() and serial-number matching
device_removed  = False
device_added    = False
target_sn       = None   # serial number of the device under test - set once, never changes


def device_changed( info ):
    global dev, device_removed, device_added
    if dev and info.was_removed( dev ):
        device_removed = True
    for candidate in info.get_new_devices():
        try:
            if candidate.get_info( rs.camera_info.serial_number ) == target_sn:
                # We replace the device handle after each reset, to be sure we're always referring to the current live instance.
                dev          = candidate
                device_added = True
        except RuntimeError:
            continue


def test_hw_reset_stress( test_device, test_context_var ):
    global dev, target_sn, device_removed, device_added

    dev, ctx = test_device
    target_sn = dev.get_info( rs.camera_info.serial_number )
    ctx.set_devices_changed_callback( device_changed )

    is_dds    = is_dds_dev(dev)
    is_weekly = 'weekly' in test_context_var
    if is_weekly:
        iterations = STRESS_ITERATIONS_DDS         if is_dds else STRESS_ITERATIONS
    else:
        iterations = STRESS_ITERATIONS_NIGHTLY_DDS if is_dds else STRESS_ITERATIONS_NIGHTLY
    max_enum    = devices.MAX_ENUMERATION_TIME
    conn_type   = "DDS" if is_dds else "USB/GMSL"

    log.info( f"Running {iterations} HW-reset iterations on {conn_type} device "
              f"({'weekly' if is_weekly else 'nightly'} context, max reconnect time: {max_enum} [sec])" )

    time.sleep( 1 )  # let the device settle before the first reset

    failed_removal   = []
    failed_reconnect = []
    skipped_removal  = 0

    for i in range( 1, iterations + 1 ):
        device_removed   = False
        device_added     = False

        log.debug( f"[{i}/{iterations}] Sending HW-reset" )
        sw = Stopwatch()
        dev.hardware_reset()

        # --- wait for removal OR addition (whichever comes first) ---
        # Use the sum of both timeouts: in the OS-race case (no removal event), the addition
        # can arrive after the full remove + reconnect cycle.
        first_timeout = REMOVAL_TIMEOUT + max_enum
        t = Timer( first_timeout )
        t.start()
        while not t.has_expired():
            if device_removed or device_added:
                break
            time.sleep( 0.05 )

        if not device_removed and not device_added:
            log.error( f"[{i}/{iterations}] No device event within {first_timeout} [sec]" )
            failed_removal.append( i )
            break

        if device_removed:
            log.debug( f"[{i}/{iterations}] removed in {sw.get_elapsed():.2f} [sec]" )
        else:
            skipped_removal += 1
            log.warning( f"[{i}/{iterations}] device reappeared without removal event (OS race) at {sw.get_elapsed():.2f} [sec]" )

        # --- wait for reconnect (may already be set) ---
        if not device_added:
            t = Timer( max_enum )
            t.start()
            while not t.has_expired():
                if device_added:
                    break
                time.sleep( 0.05 )

        added_time = sw.get_elapsed()

        if not device_added:
            log.error( f"[{i}/{iterations}] Device did not reconnect within {max_enum} [sec]" )
            failed_reconnect.append( i )
            break

        log.debug( f"[{i}/{iterations}] added in {added_time:.2f} [sec] - OK" )

    log.info( f"Completed {i} of {iterations} iterations" )
    if skipped_removal:
        log.warning( f"{skipped_removal} iteration(s) had no removal event (OS race - device reconnected too fast)" )

    if failed_removal:
        log.error( "Iterations with no events: %s", failed_removal )
    if failed_reconnect:
        log.error( "Iterations with missing reconnect: %s", failed_reconnect )

    assert len( failed_removal )   == 0, f"{len(failed_removal)} iteration(s) had no device events at all"
    assert len( failed_reconnect ) == 0, f"{len(failed_reconnect)} iteration(s) failed on reconnect"
    assert i == iterations,              f"Stress run aborted early at iteration {i}/{iterations}"
