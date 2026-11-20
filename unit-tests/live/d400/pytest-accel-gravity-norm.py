# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# The FW accel unit changed from milli-g to 10 micro-g at FW 5.17.4.24 and the SDK picks the
# matching scale per FW version. A mismatch reads exactly 100x off, so a stationary camera
# must report |a| close to standard gravity regardless of FW generation.

import pytest
import pyrealsense2 as rs
import logging
import math
import time
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.context("nightly"),
    pytest.mark.requires(sensor="Motion Module"),
]

GRAVITY = 9.80665
SAMPLES = 200
# uncalibrated units read ~2% low; a FW/SDK scale mismatch is 100x, so this window is loose on
# purpose and still rejects the mismatch by two orders of magnitude
LOW, HIGH = 0.8 * GRAVITY, 1.2 * GRAVITY


def test_accel_norm_is_gravity(test_device):
    dev, _ = test_device
    sensor = dev.first_motion_sensor()

    fw = dev.get_info(rs.camera_info.firmware_version)
    accel = [p for p in sensor.get_stream_profiles() if p.stream_type() == rs.stream.accel]
    # older IMUs (BMI055) expose only 63/250 Hz; prefer 200, else the fastest available
    profile = next((p for p in accel if p.fps() == 200), max(accel, key=lambda p: p.fps()))

    # sensor API rather than rs.pipeline: a motion-only config does not always resolve
    queue = rs.frame_queue(SAMPLES * 2)
    sensor.open(profile)
    sensor.start(queue)
    norms = []
    try:
        # a cap, not a wait: 200 samples is ~1 s at 200 Hz and ~3 s on a 63 Hz BMI055
        deadline = time.time() + 8
        while len(norms) < SAMPLES and time.time() < deadline:
            f = queue.wait_for_frame(1000).as_motion_frame()
            if f and f.get_profile().stream_type() == rs.stream.accel:
                d = f.get_motion_data()
                norms.append(math.sqrt(d.x * d.x + d.y * d.y + d.z * d.z))
    finally:
        sensor.stop()
        sensor.close()

    assert len(norms) >= SAMPLES // 2, f"only {len(norms)} accel frames received"
    mean = sum(norms) / len(norms)
    sd = math.sqrt(sum((n - mean) ** 2 for n in norms) / len(norms))
    log.info("FW %s: |a| mean %.4f m/s^2, sd %.4f, n=%d (standard gravity %.5f)",
             fw, mean, sd, len(norms), GRAVITY)

    assert LOW < mean < HIGH, (
        f"|a| = {mean:.3f} m/s^2 on FW {fw}; expected ~{GRAVITY}. "
        f"A value near {100 * GRAVITY:.0f} or {GRAVITY / 100:.3f} means the SDK accel scale does not match this FW")
