# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
import time
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
    pytest.mark.device_exclude("D401"),
    pytest.mark.device_exclude("D555"),
    pytest.mark.context("nightly"),
]

queue_capacity = 1


def close_resources(sensor):
    """
    Stop and Close sensor.
    :sensor: sensor of device
    """
    if len(sensor.get_active_streams()) > 0:
        sensor.stop()
        sensor.close()


def set_and_verify_timestamp_domain(sensor, frame_queue, global_time_enabled: bool, sleep_time: float = 0.5):
    """
    Perform sensor (depth or color) test according given global time
    :sensor: depth or color sensor in device
    :global_time_enabled bool: True - timestamp is enabled otherwise false
    """
    sensor.set_option(rs.option.global_time_enabled, global_time_enabled)
    time.sleep(sleep_time)  # Waiting for new frame from device. Need in case low FPS.
    frame = frame_queue.wait_for_frame()

    if not frame:
        pytest.fail()

    expected_ts_domain = rs.timestamp_domain.global_time if global_time_enabled else \
        rs.timestamp_domain.hardware_clock

    assert bool(sensor.get_option(rs.option.global_time_enabled)) == global_time_enabled

    log.info(str(frame.get_profile().stream_type()) + " frame: " + str(frame))
    assert frame.get_frame_timestamp_domain() == expected_ts_domain


def _run_timestamp_domain_test(sensor, stream_type, global_time_enabled):
    """Open sensor, verify timestamp domain, then close."""
    profile = next(p for p in sensor.profiles if p.stream_type() == stream_type and p.is_default())
    frame_queue = rs.frame_queue(queue_capacity, keep_frames=False)
    sensor.open(profile)
    sensor.start(frame_queue)
    try:
        set_and_verify_timestamp_domain(sensor, frame_queue, global_time_enabled)
    finally:
        close_resources(sensor)


def test_depth_timestamp_domain_off(test_device):
    device, ctx = test_device
    log.info('Check setting global time domain: depth sensor - timestamp domain is OFF')
    _run_timestamp_domain_test(device.first_depth_sensor(), rs.stream.depth, False)


def test_depth_timestamp_domain_on(test_device):
    device, ctx = test_device
    log.info('Check setting global time domain: depth sensor - timestamp domain is ON')
    _run_timestamp_domain_test(device.first_depth_sensor(), rs.stream.depth, True)


def test_color_timestamp_domain_off(test_device):
    device, ctx = test_device
    product_name = device.get_info(rs.camera_info.name)
    try:
        color_sensor = device.first_color_sensor()
    except RuntimeError:
        if 'D421' in product_name or 'D405' in product_name:  # Cameras with no color sensor may fail.
            pytest.skip("No color sensor")
        raise
    log.info('Check setting global time domain: color sensor - timestamp domain is OFF')
    _run_timestamp_domain_test(color_sensor, rs.stream.color, False)


def test_color_timestamp_domain_on(test_device):
    device, ctx = test_device
    product_name = device.get_info(rs.camera_info.name)
    try:
        color_sensor = device.first_color_sensor()
    except RuntimeError:
        if 'D421' in product_name or 'D405' in product_name:  # Cameras with no color sensor may fail.
            pytest.skip("No color sensor")
        raise
    log.info('Check setting global time domain: color sensor - timestamp domain is ON')
    _run_timestamp_domain_test(color_sensor, rs.stream.color, True)
