# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import time
import pyrealsense2 as rs
from pytest_check import check
import logging
log = logging.getLogger(__name__)

pytestmark = [pytest.mark.device_each("D585S")]


def verify_frames_received(pipe, count):
    for i in range(count):
        # no check is needed, assume wait_for_frames will raise exception if not frames arrive
        fs = pipe.wait_for_frames()
        if len(fs) > 1:
            for f in fs:
                log.debug(f)
        else:
            log.debug(fs)


########################### SRS - 3.3.1.14.b ##############################################
def test_pause_resume_no_impact_on_streaming(test_context):
    cfg = rs.config()
    cfg.enable_stream(rs.stream.safety, rs.format.y8, 30)
    cfg.enable_stream(rs.stream.depth, rs.format.z16, 30)
    cfg.enable_stream(rs.stream.color, rs.format.rgb8, 30)

    pipe = rs.pipeline(test_context)
    profile = pipe.start(cfg)
    
    try:
        f = pipe.wait_for_frames()

        pipeline_device = profile.get_device()
        safety_sensor = pipeline_device.first_safety_sensor()
        log.debug("Verify default is run mode")
        check.equal(safety_sensor.get_option(rs.option.safety_mode), float(rs.safety_mode.run))  # verify default

        log.debug("Command standby mode")
        safety_sensor.set_option(rs.option.safety_mode, rs.safety_mode.standby)
        check.equal(safety_sensor.get_option(rs.option.safety_mode), float(rs.safety_mode.standby))
        verify_frames_received(pipe, count=10)

        pipe.stop()
        time.sleep(1)  # allow some time for the streaming to actually stop
        pipe.start(cfg)
        verify_frames_received(pipe, count=10)

        log.debug("Command run mode")
        safety_sensor.set_option(rs.option.safety_mode, rs.safety_mode.run)
        check.equal(safety_sensor.get_option(rs.option.safety_mode), float(rs.safety_mode.run))
        verify_frames_received(pipe, count=10)
    finally:
        pipe.stop()
        time.sleep(1)  # allow device to fully release before next test


########################### SRS - 3.3.1.14.c ##############################################
def test_resume_to_maintenance_keeps_video_streaming(test_context):
    cfg = rs.config()
    cfg.enable_stream(rs.stream.safety, rs.format.y8, 30)
    cfg.enable_stream(rs.stream.depth, rs.format.z16, 30)
    cfg.enable_stream(rs.stream.color, rs.format.rgb8, 30)

    pipe = rs.pipeline(test_context)
    profile = pipe.start(cfg)

    try:
        f = pipe.wait_for_frames()

        pipeline_device = profile.get_device()
        safety_sensor = pipeline_device.first_safety_sensor()

        log.debug("Command run mode")
        safety_sensor.set_option(rs.option.safety_mode, rs.safety_mode.run)
        check.equal(safety_sensor.get_option(rs.option.safety_mode), float(rs.safety_mode.run))
        # Verify that on RUN mode we get frames
        verify_frames_received(pipe, count=10)

        log.debug("Command service mode")
        safety_sensor.set_option(rs.option.safety_mode, rs.safety_mode.service)
        check.equal(safety_sensor.get_option(rs.option.safety_mode), float(rs.safety_mode.service))
        verify_frames_received(pipe, count=10)

        # Restore Run mode
        log.debug("Command run mode")
        safety_sensor.set_option(rs.option.safety_mode, rs.safety_mode.run)
        check.equal(safety_sensor.get_option(rs.option.safety_mode), float(rs.safety_mode.run))
        # Verify that on RUN mode we get frames
        verify_frames_received(pipe, count=10)
    finally:
        pipe.stop()
        time.sleep(1)  # allow device to fully release before next test


########################### SRS - 3.3.1.14.c ##############################################
def test_resume_to_maintenance_keeps_safety_streaming(test_context):
    cfg = rs.config()
    cfg.enable_stream(rs.stream.safety, rs.format.y8, 30)

    pipe = rs.pipeline(test_context)
    profile = pipe.start(cfg)

    try:
        f = pipe.wait_for_frames()

        pipeline_device = profile.get_device()
        safety_sensor = pipeline_device.first_safety_sensor()

        log.debug("Command run mode")
        safety_sensor.set_option(rs.option.safety_mode, rs.safety_mode.run)
        check.equal(safety_sensor.get_option(rs.option.safety_mode), float(rs.safety_mode.run))
        # Verify that on RUN mode we get frames
        verify_frames_received(pipe, count=10)

        log.debug("Command service mode")
        safety_sensor.set_option(rs.option.safety_mode, rs.safety_mode.service)
        check.equal(safety_sensor.get_option(rs.option.safety_mode), float(rs.safety_mode.service))
        # Verify that on SERVICE mode we still get frames
        verify_frames_received(pipe, count=10)

        # Restore Run mode
        log.debug("Command run mode")
        safety_sensor.set_option(rs.option.safety_mode, rs.safety_mode.run)
        check.equal(safety_sensor.get_option(rs.option.safety_mode), float(rs.safety_mode.run))

        # We know that returning to run mode will not restart the safety stream.
        # FW expect the user to restart the stream at host side
        pipe.stop()
        time.sleep(1)  # allow some time for the streaming to actually stop
        pipe.start(cfg)

        # Verify that on RUN mode we get frames
        verify_frames_received(pipe, count=10)
    finally:
        pipe.stop()
        time.sleep(1)  # allow device to fully release before next test
