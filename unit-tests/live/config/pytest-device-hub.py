# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense Inc. All Rights Reserved.

# wait_for_device can block. Timeout early in case of error.

import time
import pytest
import pyrealsense2 as rs
from pytest_check import check
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.context("nightly"),
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
    pytest.mark.timeout(60),
]

MAX_TRIES = 3
DISCONNECT_TIMEOUT_SEC = 2

_module_state = {}


def test_wait_for_device_and_is_connected(test_device):
    dev, ctx = test_device
    hub = rs.device_hub(ctx)
    dev_from_hub = hub.wait_for_device()
    check.is_true(dev_from_hub is not None, "wait_for_device() did not return a device")
    check.is_true(hub.is_connected(dev_from_hub), "Device should be reported as connected")
    _module_state['basic_ok'] = True


def test_detect_disconnect_after_hardware_reset(test_device):
    if not _module_state.get('basic_ok'):
        pytest.skip("prerequisite test_wait_for_device_and_is_connected failed")

    dev, ctx = test_device
    hub = rs.device_hub(ctx)
    caught_once = False
    attempt = 1

    while not caught_once and attempt <= MAX_TRIES:
        log.info(f"Attempt {attempt}/{MAX_TRIES}: issuing hardware_reset()")
        dev.hardware_reset()
        attempt += 1

        # Wait until hub reports this handle as disconnected
        t = time.time()
        while time.time() - t < DISCONNECT_TIMEOUT_SEC:
            if not hub.is_connected(dev):
                caught_once = True
                break
            time.sleep(0.1)

    check.is_true(caught_once, f"Failed to observe a disconnect in {MAX_TRIES} hardware reset attempt(s)")
    _module_state['disconnect_ok'] = True

    # Exiting the tests directly after the disconnection will cause hub to disconnect port.
    # We had issues on CI disconnecting when device is powering down/up so we wait for it to finish reset.
    # (Unifi Hub stuck disconnecting port, D555 recovery OS -> domain set to 0)
    new_dev = hub.wait_for_device()
    check.is_true(new_dev is not None)
