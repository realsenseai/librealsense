# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

# This test checks the locking mechanism on UVC devices (MIPI classes extend UVC).
# HWMC locks the device and is also "invoke_power"ed, so we use many commands
# with several threads trying to send simultaneously.
# We set visual preset as it internally issues many commands - PU, XU and HWM.

import pytest
import pyrealsense2 as rs
import threading
import time
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.context("weekly"),
]


def change_presets(dev, index, delay, errors):
    try:
        depth_sensor = dev.first_depth_sensor()
        if not depth_sensor.supports(rs.option.visual_preset):
            errors.append(f"Thread {index}: visual_preset not supported")
            return
    except Exception as e:
        errors.append(f"Thread {index} setup: {e}")
        return
    time.sleep(delay)
    for i in range(10):
        try:
            log.debug(f"Thread {index}: setting high_accuracy")
            start = time.perf_counter()
            depth_sensor.set_option(rs.option.visual_preset, int(rs.rs400_visual_preset.high_accuracy))
            log.debug(f"Thread {index}: setting high_accuracy took {time.perf_counter() - start:.3f}s")

            log.debug(f"Thread {index}: setting default")
            start = time.perf_counter()
            depth_sensor.set_option(rs.option.visual_preset, int(rs.rs400_visual_preset.default))
            log.debug(f"Thread {index}: setting default took {time.perf_counter() - start:.3f}s")
        except Exception as e:
            errors.append(f"Thread {index}, iteration {i}: {e}")


def test_uvc_power_stress(test_device):
    """Stress test for UVC power locking: multiple threads setting visual presets concurrently
    while GVD commands are issued in parallel. No exceptions should be thrown."""
    dev, ctx = test_device

    # Create a DDS-disabled context to test UVC locking specifically
    uvc_ctx = rs.context({'dds': {'enabled': False}})
    devices = uvc_ctx.query_devices()
    assert len(devices) > 0, "No devices found in DDS-disabled context"

    errors = []
    threads = []
    for i in range(len(devices)):
        # Two threads per device. Calling devices[i] twice creates two different device objects for the same camera
        t1 = threading.Thread(target=change_presets, args=(devices[i], i * 2, 0.05, errors))
        t2 = threading.Thread(target=change_presets, args=(devices[i], i * 2 + 1, 0.1, errors))
        threads.append(t1)
        threads.append(t2)

    for t in threads:
        t.start()

    # Issue GVD commands while threads are running
    raw_command = rs.debug_protocol(devices[0]).build_command(0x10)  # 0x10 is GVD opcode
    for i in range(10):
        log.debug("Sending GVD commands")
        for j in range(len(devices)):
            start = time.perf_counter()
            rs.debug_protocol(devices[j]).send_and_receive_raw_data(raw_command)
            log.debug(f"Got device {j} GVD in {time.perf_counter() - start:.3f}s")
        time.sleep(0.5)

    for t in threads:
        t.join()

    if errors:
        pytest.fail("\n".join(errors))
