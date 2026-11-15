# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# The D500 FW reports the IMU in different formats per FW generation, over USB and GMSL (int16 / int32,
# 1 mg / 10 ug accel) and the SDK picks the decoder and scale from the FW version. A mismatch shows up
# as a stationary camera reading a wildly wrong magnitude (accel 100x too high, or garbage on both
# sensors), so a stationary camera must report |accel| close to standard gravity and a small gyro.

import pytest
import pyrealsense2 as rs
import logging
import math
import time
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D500*"),
    pytest.mark.device_exclude("D585S"),
    pytest.mark.device_type_exclude("DDS"),  # DDS motion goes through a separate transport and scale
    pytest.mark.context("nightly"),  # needs a stationary camera
]

GRAVITY = 9.80665
SAMPLES = 200
# uncalibrated units read ~1-2% low; a scale mismatch is 100x, so this window is loose on purpose
LOW, HIGH = 0.8 * GRAVITY, 1.2 * GRAVITY
# a stationary gyro reads a few mrad/s of noise and bias; a wrong decode reads orders of magnitude more
MAX_GYRO_REST = 0.2  # rad/s


def test_stationary_imu_magnitudes(test_device):
    dev, _ = test_device
    sensor = next((s for s in dev.query_sensors()
                   if any(p.stream_type() == rs.stream.accel for p in s.get_stream_profiles())), None)
    if sensor is None:
        pytest.skip("device has no accelerometer")

    fw = dev.get_info(rs.camera_info.firmware_version)
    accel = [p for p in sensor.get_stream_profiles() if p.stream_type() == rs.stream.accel]
    gyro = [p for p in sensor.get_stream_profiles() if p.stream_type() == rs.stream.gyro]
    accel_profile = next((p for p in accel if p.fps() == 200), max(accel, key=lambda p: p.fps()))
    gyro_profile = next((p for p in gyro if p.fps() == accel_profile.fps()), max(gyro, key=lambda p: p.fps()))

    # sensor API rather than rs.pipeline: a motion-only config does not always resolve
    queue = rs.frame_queue(SAMPLES * 4)
    sensor.open([accel_profile, gyro_profile])
    sensor.start(queue)
    accel_norms, gyro_norms = [], []
    try:
        deadline = time.time() + 8  # a cap, not a wait
        while (len(accel_norms) < SAMPLES or len(gyro_norms) < SAMPLES) and time.time() < deadline:
            f = queue.wait_for_frame(1000).as_motion_frame()
            if not f:
                continue
            d = f.get_motion_data()
            norm = math.sqrt(d.x * d.x + d.y * d.y + d.z * d.z)
            if f.get_profile().stream_type() == rs.stream.accel:
                accel_norms.append(norm)
            else:
                gyro_norms.append(norm)
    finally:
        sensor.stop()
        sensor.close()

    assert len(accel_norms) >= SAMPLES // 2, f"only {len(accel_norms)} accel frames received"
    assert len(gyro_norms) >= SAMPLES // 2, f"only {len(gyro_norms)} gyro frames received"
    accel_mean = sum(accel_norms) / len(accel_norms)
    gyro_mean = sum(gyro_norms) / len(gyro_norms)
    log.info("FW %s: |accel| mean %.4f m/s^2 (n=%d), |gyro| mean %.5f rad/s (n=%d)",
             fw, accel_mean, len(accel_norms), gyro_mean, len(gyro_norms))

    assert LOW < accel_mean < HIGH, (
        f"|accel| = {accel_mean:.3f} m/s^2 on FW {fw}; expected ~{GRAVITY}. "
        f"A value near {100 * GRAVITY:.0f} or {GRAVITY / 100:.3f} means the SDK accel scale does not match this FW")
    assert gyro_mean < MAX_GYRO_REST, (
        f"|gyro| = {gyro_mean:.4f} rad/s on a stationary camera, FW {fw}; "
        f"the SDK gyro decode or scale does not match this FW")
