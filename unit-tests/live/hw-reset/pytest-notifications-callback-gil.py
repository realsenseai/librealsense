# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Python wrapper deadlock regression test. See LRS-1040
# The test loops hardware_reset while a background thread injects FW errors, and on each
# iteration re-registers set_notifications_callback while a notification is in flight.
# Runs in a subprocess so a GIL deadlock can be killed instead of hanging pytest.

import os
import sys
import subprocess

import pytest
import pyrealsense2 as rs

from rspy import devices
from rspy.pytest.device_helpers import is_jetson_platform

# How long the parent waits for the child to complete. 20 iterations typically
# take a few minutes; this is generous slack for slow USB reconnects. A real
# GIL deadlock makes no progress at all, so any value beyond expected runtime
# works -- it just sets the upper bound on how long CI waits before declaring
# failure.
CHILD_TIMEOUT_S = 300


pytestmark = [
    pytest.mark.device_each( "D455" ), # The HWM injection is a D400 FW mechanism. Since this is a SW bug it's enough testing the flow on one device type only.
    pytest.mark.skipif( is_jetson_platform(), reason="No D455 connected to Jetson in CI" ),
    pytest.mark.context( "weekly" ),
    # Override the conftest default (200s) so our proc.wait(CHILD_TIMEOUT_S)
    # fires first on a hang. pytest-timeout's "thread" method on Windows kills
    # the whole pytest process via os._exit(1), bypassing our cleanup and
    # leaving no FAILED report -- so its timeout must be > CHILD_TIMEOUT_S
    # plus a buffer for terminate/kill of the child.
    pytest.mark.timeout( CHILD_TIMEOUT_S + 20 ),
]


# ============================================================================
# Test (parent process)
# ============================================================================

def test_hw_reset_with_notifications_callback_no_gil_deadlock( test_device ):
    dev, _ctx = test_device
    target_sn = dev.get_info( rs.camera_info.serial_number )

    # The pytest infra (conftest.py) locates the freshly-built pyrealsense2.pyd
    # via repo.find_pyrs_dir() and adds its directory to sys.path -- but the
    # spawned child gets only what we hand it via env. Build PYTHONPATH from
    # the .pyd directory plus the parent's sys.path so the child can resolve
    # both `import pyrealsense2` and `from rspy import ...`.
    env = os.environ.copy()
    env[ "PYTHONPATH" ] = os.pathsep.join(
        [ os.path.dirname( rs.__file__ ) ] + [ p for p in sys.path if p ]
    )

    proc = subprocess.Popen(
        [ sys.executable, __file__, "--child", target_sn ],
        env = env,
    )
    try:
        rc = proc.wait( timeout = CHILD_TIMEOUT_S )
    except subprocess.TimeoutExpired:
        # Child is hung -- almost certainly the GIL deadlock has regressed.
        proc.terminate()
        try:
            proc.wait( timeout = 5 )
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
        pytest.fail(
            f"GIL deadlock regression: child did not complete within "
            f"{CHILD_TIMEOUT_S} [sec]"
        )

    assert rc == 0, f"child process exited with code {rc} (see child stderr above)"


# ============================================================================
# Child process body.
# Reached only when this file is run as a script by the parent test above
# (subprocess.Popen([..., __file__, "--child", serial])). Pytest collection
# imports the module but never runs anything below the if __name__ guard.
# ============================================================================

def _run_child( target_sn ):
    """Re-acquire the device by serial number, register callbacks, and run
    the deadlock-prone hardware_reset loop. Pre-fix this hangs; post-fix it
    returns and the process exits 0. Any AssertionError or other exception
    escapes uncaught -> non-zero exit code, which the parent reports as a
    test failure."""
    import threading
    import time

    from rspy.timer import Timer

    ITERATIONS              = 20
    CALLBACK_SLEEP_S        = 0.3  # GIL-releasing sleep inside notification_cb. Gives main plenty of time to enter the C++ call that previously deadlocked.
    HWM_TRIGGER_INTERVAL_S  = 0.8  # background thread fires HWM at this cadence
    NOTIF_WAIT_TIMEOUT_S    = 5    # max wait per iteration for an in-flight notification

    OPCODE_TRIGGER_ERROR    = 0x4D # HWM opcode that injects a firmware error
    ERROR_ID_MM_FORCE_PAUSE = 9    # error ID -> "Motion Module force pause"

    state = {
        'sn'                  : target_sn,
        'current_dev'         : None,
        'device_added'        : False,
        'callback_in_progress': threading.Event(),
        'stop_trigger'        : threading.Event(),
        'notif_count'         : 0,
    }

    def trigger_mm_error( proto ):
        raw = proto.build_command( OPCODE_TRIGGER_ERROR, ERROR_ID_MM_FORCE_PAUSE )
        reply = list( proto.send_and_receive_raw_data( raw ) )
        return reply[ :4 ] == [ OPCODE_TRIGGER_ERROR, 0, 0, 0 ]

    def notification_cb( notif ):
        state[ 'notif_count' ] += 1
        # Tell main we entered the callback. Worker is now holding _dispatch_mutex while we sleep below.
        # GIL is about to be released by time.sleep().
        state[ 'callback_in_progress' ].set()
        time.sleep( CALLBACK_SLEEP_S )

    def devices_changed_cb( info ):
        for candidate in info.get_new_devices():
            try:
                if candidate.get_info( rs.camera_info.serial_number ) == state[ 'sn' ]:
                    # Reassigning current_dev drops the previous device's Python ref. If this is the last ref, the C++ destructor
                    # cascade fires here -- exercising the destruction-path deadlock.
                    state[ 'current_dev' ] = candidate
                    state[ 'device_added' ] = True
            except RuntimeError:
                continue

    def hwm_trigger_loop():
        """Continuously inject FW errors so a notification is always in flight."""
        while not state[ 'stop_trigger' ].is_set():
            try:
                proto = rs.debug_protocol( state[ 'current_dev' ] )
                trigger_mm_error( proto )
            except Exception:
                # Expected during reset / disconnect windows.
                pass
            time.sleep( HWM_TRIGGER_INTERVAL_S )

    def setup_sensor( device ):
        for s in device.query_sensors():
            if s.supports( rs.option.error_polling_enabled ):
                s.set_notifications_callback( notification_cb )
                s.set_option( rs.option.error_polling_enabled, 1 )
                return s
        return None

    # Acquire the device fresh in this process by serial number.
    ctx = rs.context()
    t = Timer( devices.MAX_ENUMERATION_TIME )
    t.start()
    while not t.has_expired():
        for d in ctx.query_devices():
            try:
                if d.get_info( rs.camera_info.serial_number ) == target_sn:
                    state[ 'current_dev' ] = d
                    break
            except RuntimeError:
                continue
        if state[ 'current_dev' ] is not None:
            break
        time.sleep( 0.1 )
    assert state[ 'current_dev' ] is not None, ( f"could not find device {target_sn} within {devices.MAX_ENUMERATION_TIME} [sec]" )

    ctx.set_devices_changed_callback( devices_changed_cb )
    time.sleep( 1 )  # let the device settle before the first reset

    sensor = setup_sensor( state[ 'current_dev' ] )
    assert sensor is not None, "device has no sensor with error_polling_enabled"

    # Start the background HWM thread first, then wait for any notification to
    # confirm the injection mechanism works on this device / firmware. The
    # thread fires every HWM_TRIGGER_INTERVAL_S; the polling-error handler runs
    # on its own ~1 s cycle, so the first dispatched notification typically
    # arrives within a couple of seconds.
    hwm_thread = threading.Thread( target = hwm_trigger_loop, daemon = True )
    hwm_thread.start()
    assert state[ 'callback_in_progress' ].wait( timeout = NOTIF_WAIT_TIMEOUT_S ), (
        f"no notification within {NOTIF_WAIT_TIMEOUT_S} [sec] of starting HWM "
        f"injection -- HWM 0x4D/9 may not be supported on this device/firmware"
    )
    state[ 'callback_in_progress' ].clear()
    try:
        for i in range( 1, ITERATIONS + 1 ):
            state[ 'callback_in_progress' ].clear()
            state[ 'device_added' ] = False

            state[ 'current_dev' ].hardware_reset()

            t = Timer( devices.MAX_ENUMERATION_TIME )
            t.start()
            while not t.has_expired():
                if state[ 'device_added' ]:
                    break
                time.sleep( 0.05 )
            assert state[ 'device_added' ], (
                f"iter {i}: device did not reconnect within "
                f"{devices.MAX_ENUMERATION_TIME} [sec]"
            )

            # Re-register the callback on the new device. With the HWM thread
            # firing continuously, this is the original LRS-1040 race window:
            # the previous worker may already be in notification_cb's
            # time.sleep() when we reach _notifications_processor::set_callback
            # -> dispatcher::stop().
            sensor = setup_sensor( state[ 'current_dev' ] )
            assert sensor is not None, f"iter {i}: no error-polling sensor on new device"

            # Force the deadlock window deterministically: wait until the
            # worker is mid-time.sleep (callback_in_progress signalled, GIL
            # released, _dispatch_mutex held), then call set_notifications_callback
            # again. Pre-fix this hangs forever on the first iteration that
            # makes it here; post-fix it returns within a callback sleep.
            if state[ 'callback_in_progress' ].wait( timeout = NOTIF_WAIT_TIMEOUT_S ):
                state[ 'callback_in_progress' ].clear()
                sensor.set_notifications_callback( notification_cb )
    finally:
        state[ 'stop_trigger' ].set()
        hwm_thread.join( timeout = 5 )

    assert state[ 'notif_count' ] > 0, (
        "no notifications received -- deadlock window was never exercised"
    )


if __name__ == "__main__":
    if len( sys.argv ) >= 3 and sys.argv[ 1 ] == "--child":
        _run_child( sys.argv[ 2 ] )
        sys.exit( 0 )
    print( f"usage: python {sys.argv[0]} --child <serial>", file = sys.stderr )
    sys.exit( 2 )
