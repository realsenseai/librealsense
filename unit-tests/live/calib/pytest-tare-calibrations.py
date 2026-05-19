# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import time
import pytest
import pyrealsense2 as rs
import logging
from calibrations_common import calibration_main, get_calibration_device, is_mipi_device

log = logging.getLogger(__name__)

# Disabled until we stabilize the lab (was #test:donotrun in the legacy directive form).
pytestmark = [
    pytest.mark.skip(reason="disabled until lab stabilization is complete"),
    pytest.mark.context("calibration"),
]


def tare_calibration_json(tare_json_file, host_assistance):
    tare_json = None
    if tare_json_file is not None:
        try:
            tare_json = open(tare_json_file).read()
        except:
            tare_json = None
            log.error(f'Error reading tare_json_file: {tare_json_file}')
    if tare_json is None:
        log.info('Using default parameters for Tare calibration.')
        tare_json = '{\n  '+\
                    '"host assistance": ' + str(int(host_assistance)) + ',\n'+\
                    '"speed": 3,\n'+\
                    '"scan parameter": 0,\n'+\
                    '"step count": 20,\n'+\
                    '"apply preset": 1,\n'+\
                    '"accuracy": 2,\n'+\
                    '"depth": 0,\n'+\
                    '"resize factor": 1\n'+\
                    '}'
    return tare_json


def calculate_target_z():
    number_of_images = 50  # The required number of frames is 10+
    warmup_frames = 30     # Allow AE to stabilize before capturing (1 sec at 30fps)
    timeout_s = 30
    target_size = [175, 100]

    cfg = rs.config()
    cfg.enable_stream(rs.stream.infrared, 1, 1280, 720, rs.format.y8, 30)

    q = rs.frame_queue(capacity=number_of_images, keep_frames=True)
    # Frame queues q2, q3 should be left empty. Provision for future enhancements.
    q2 = rs.frame_queue(capacity=number_of_images, keep_frames=True)
    q3 = rs.frame_queue(capacity=number_of_images, keep_frames=True)

    counter = 0
    warmup_counter = 0

    def cb(frame):
        nonlocal counter, warmup_counter
        if counter >= number_of_images:
            return
        if warmup_counter < warmup_frames:
            warmup_counter += 1
            return
        q.enqueue(frame)
        counter += 1

    ctx = rs.context()
    pipe = rs.pipeline(ctx)
    pp = pipe.start(cfg, cb)
    dev = pp.get_device()

    try:
        stime = time.time()
        while counter < number_of_images:
            time.sleep(0.5)
            if timeout_s < (time.time() - stime):
                raise RuntimeError(f"Failed to capture {number_of_images} frames in {timeout_s} seconds, got only {counter} frames")

        adev = dev.as_auto_calibrated_device()
        log.info('Calculating distance to target...')
        log.info(f'\tTarget Size:\t{target_size}')
        target_z = adev.calculate_target_z(q, q2, q3, target_size[0], target_size[1])
        log.info(f'Calculated distance to target is {target_z}')
    finally:
        pipe.stop()

    return target_z


# Constants for validation
HEALTH_FACTOR_THRESHOLD = 0.25
TARGET_Z_MIN = 600
TARGET_Z_MAX = 1500
_target_z = None
"""
def test_tare_calibration_with_host_assistance(test_device):
    if not is_mipi_device():
        pytest.skip("Host-assistance Tare calibration is only run on MIPI/GMSL devices")
    global _target_z
    try:
        host_assistance = True
        if (_target_z is None):
            _target_z = calculate_target_z()
            assert _target_z > TARGET_Z_MIN and _target_z < TARGET_Z_MAX

        tare_json = tare_calibration_json(None, host_assistance)
        image_width, image_height, fps = 1280, 720, 30
        config, pipeline, calib_dev = get_calibration_device(image_width, image_height, fps)
        health_factor, new_calib_bytes = calibration_main(config, pipeline, calib_dev, False, tare_json, _target_z, host_assistance, return_table=True)

        assert abs(health_factor) < HEALTH_FACTOR_THRESHOLD
    except Exception as e:
        log.error(f"Tare calibration test with host assistance failed: {e}")
        pytest.fail(f"Tare calibration test with host assistance failed: {e}")
"""


def test_tare_calibration(test_device):
    # mipi devices do not support OCC calibration without host assistance
    if is_mipi_device():
        pytest.skip("MIPI/GMSL devices require host assistance for tare calibration")
    global _target_z
    try:
        host_assistance = False
        if _target_z is None:
            _target_z = calculate_target_z()
            assert _target_z > TARGET_Z_MIN and _target_z < TARGET_Z_MAX

        tare_json = tare_calibration_json(None, host_assistance)
        image_width, image_height, fps = 256, 144, 90
        config, pipeline, calib_dev = get_calibration_device(image_width, image_height, fps)
        health_factor, new_calib_bytes = calibration_main(config, pipeline, calib_dev, False, tare_json, _target_z, host_assistance, return_table=True)

        assert abs(health_factor) < HEALTH_FACTOR_THRESHOLD
    except Exception as e:
        log.error(f"Tare calibration test failed: {e}")
        pytest.fail(f"Tare calibration test failed: {e}")
