# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
from pytest_check import check
import threading
import platform
import logging
log = logging.getLogger(__name__)

# This test checks that every stream's first frame number after each stream start is the expected one for the platform

pytestmark = [
    pytest.mark.device_each("D400*"),  # D500 excluded: FW counter starts at 2+, RSDEV-7649
    pytest.mark.device_type_exclude("GMSL"),  # GMSL excluded: FW counter starts at 2+, RSDSO-21070
]

CYCLES = 3


def test_first_frame_number_on_stream_start(test_device):
    """Sensor callbacks are used because the pipeline drops frames until it has a full frameset."""
    dev, ctx = test_device
    cfg = rs.config()
    cfg.enable_device(dev.get_info(rs.camera_info.serial_number))
    cfg.enable_all_streams()
    resolved = cfg.resolve(rs.pipeline_wrapper(rs.pipeline(ctx))).get_streams()
    keys = {str(p) for p in resolved}
    sensor_profiles = [(s, [p for p in s.get_stream_profiles() if str(p) in keys]) for s in dev.query_sensors()]
    sensor_profiles = [(s, ps) for s, ps in sensor_profiles if ps]
    assert sum(len(ps) for _, ps in sensor_profiles) == len(resolved)

    for cycle in range(CYCLES):
        first = {}
        lock = threading.Lock()
        all_arrived = threading.Event()

        def on_frame(f):
            with lock:
                first.setdefault(f.get_profile().stream_name(), (f.get_frame_number(), f.is_motion_frame()))
                if len(first) == len(resolved):
                    all_arrived.set()

        opened, started = [], []
        try:
            for s, ps in sensor_profiles:
                s.open(ps)
                opened.append(s)
                s.start(on_frame)
                started.append(s)
            all_arrived.wait(10)
        finally:
            for s in started:
                s.stop()
            for s in opened:
                s.close()

        log.info(f"cycle {cycle}: first frame numbers {first}")
        check.equal(len(first), len(resolved), f"cycle {cycle}: not all streams delivered a frame: {first}")
        for stream, (number, is_motion) in first.items():
            # The SDK counts IMU frames from 1; on Windows the FW counter's frame 0 never reaches the host
            expected = 1 if is_motion or platform.system() == "Windows" else 0
            check.equal(number, expected, f"cycle {cycle}: {stream} first frame number is {number}")
