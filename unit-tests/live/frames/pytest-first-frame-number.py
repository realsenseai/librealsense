# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
from pytest_check import check
import threading
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.context("nightly"),
    pytest.mark.device_each("D400*"),  # D500 excluded: FW counter starts at 2+, RSDEV-7649
    pytest.mark.device_type_exclude("GMSL"),  # GMSL excluded: FW counter starts at 2+, RSDSO-21070
]

CYCLES = 3


def test_first_frame_number_on_stream_start(test_device):
    """Every stream's first frame is numbered 0 or 1 after each start. Sensor callbacks are used
    because the pipeline drops frames until it has a full frameset."""
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
                first.setdefault(f.get_profile().stream_name(), f.get_frame_number())
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
        for stream, number in first.items():
            check.less_equal(number, 1, f"cycle {cycle}: {stream} first frame number is {number}")
