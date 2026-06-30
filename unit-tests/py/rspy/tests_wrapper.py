# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

# Some future models might need to wrap the tests in setup and teardown steps.
# Don't remove this file even if current implementation is empty...

import time
import pyrealsense2 as rs
from rspy import log
from rspy.stopwatch import Stopwatch


# The D585S FW finishes loading its safety config a few seconds after USB enumeration (~4s
# observed). A safety_mode change issued before that fails with "Failed to set the option to
# value 2", so retry until it takes or we time out.
def set_safety_mode( safety_sensor, mode, timeout = 10, interval = 0.5 ):
    sw = Stopwatch()
    last_exc = None
    attempt = 0
    while True:
        attempt += 1
        try:
            safety_sensor.set_option( rs.option.safety_mode, mode )
            if safety_sensor.get_option( rs.option.safety_mode ) == float( mode ):
                log.i( f"safety_mode set to {mode} after {attempt} attempt(s), {sw.get_elapsed():.1f}s" )
                return
        except Exception as e:
            last_exc = e
        if sw.get_elapsed() >= timeout:
            if last_exc:
                raise last_exc
            raise RuntimeError( f"failed to set safety_mode to {mode} within {timeout}s" )
        time.sleep( interval )


# Many operations, such as setting options, can take place only in safety service mode
def start_wrapper( dev = None ):
    if "D585S" in dev.get_info(rs.camera_info.name):
        safety_sensor = dev.first_safety_sensor()
        set_safety_mode( safety_sensor, rs.safety_mode.service )

def stop_wrapper( dev = None ):
    if "D585S" in dev.get_info(rs.camera_info.name):
        try:
            safety_sensor = dev.first_safety_sensor()
            safety_sensor.set_option(rs.option.safety_mode, rs.safety_mode.run)
        except Exception as e:
            log.e(f"Cleanup failed: could not set safety_mode back to run: {e}")