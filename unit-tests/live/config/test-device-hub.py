# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense Inc. All Rights Reserved.

# test:donotrun:!nightly
# test:device each(D400*)
# test:device each(D500*)
# wait_for_device can block. Timeout early in case of error.
# test:timeout 60

import time
import pyrealsense2 as rs
from rspy import log, test

MAX_TRIES = 3
DISCONNECT_TIMEOUT_SEC = 2


dev, ctx = test.find_first_device_or_exit()
hub = rs.device_hub(ctx)

# -------- Basic functionality --------
test.start("device_hub: wait_for_device & is_connected")
try:
    dev_from_hub = hub.wait_for_device()
    test.check(dev_from_hub is not None, "wait_for_device() did not return a device")
    test.check(hub.is_connected(dev_from_hub), "Device should be reported as connected")
except:
    test.unexpected_exception()
test.finish()

# -------- Disconnect detection via hardware reset --------
test.start("device_hub: detect disconnect after hardware_reset")
caught_once = False
attempt = 1

while not caught_once and attempt <= MAX_TRIES:
    try:
        log.i(f"Attempt {attempt}/{MAX_TRIES}: issuing hardware_reset()")
        dev.hardware_reset()
        attempt += 1

        # Wait until hub reports this handle as disconnected
        t = time.time()
        while time.time() - t < DISCONNECT_TIMEOUT_SEC:
            if not hub.is_connected(dev):
                caught_once = True
                break
            time.sleep(0.1)

    except:
        test.unexpected_exception()

test.check(caught_once, f"Failed to observe a disconnect in {MAX_TRIES} hardware reset attempt(s)")
test.finish()

test.start("device_hub: wait for reconnection")
# Exiting the test will cause hub to disconnect port. We saw problems on CI when disconnecting like this when device is powering down/up so we wait for it to finish reset.
# (Unifi Hub stuck disconnecting port, D555 recovery OS -> domain set to 0)
new_dev = hub.wait_for_device()
test.check(new_dev is not None)
test.finish()

test.print_results_and_exit()
