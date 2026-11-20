# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs

pytestmark = [
    pytest.mark.device("D400*"),
    pytest.mark.context("nightly"),
]


@pytest.mark.parametrize("close_first", [False, True])
def test_pipeline_stop_after_sensor_stop(test_device, close_first):
    dev, ctx = test_device
    pipe = rs.pipeline(ctx)
    cfg = rs.config()
    cfg.enable_device(dev.get_info(rs.camera_info.serial_number))
    dev = pipe.start(cfg).get_device()
    sensors = [s for s in dev.query_sensors() if s.get_active_streams()]

    sensors[0].stop()  # stopped (and closed) through the sensor API while the pipeline runs
    if close_first:
        sensors[0].close()
    pipe.stop()  # must still stop and close every pipeline sensor

    for s in sensors:
        assert not s.get_active_streams()
        s.open(s.get_stream_profiles()[0])
        s.close()
