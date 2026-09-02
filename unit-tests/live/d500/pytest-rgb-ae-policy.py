# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Auto-exposure policy arbitration between the color (IPU / 3A) and depth (SDP / 2A) pipelines, exposed
# on D585 dual-RGB SKUs as depth_auto_exposure_mode over the depth XU. Other D585 variants do not register it.

import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D585"),
    pytest.mark.skip(reason="No D585 dual-RGB camera in CI"), # Verified locally against a D585 Proto Dual RGB (PID 0x0C07)
]

# RS2_OPTION_DEPTH_AUTO_EXPOSURE_MODE: python names come from rs2_option_to_string, not the C identifier
AE_MODE = rs.option.auto_exposure_mode
POLICIES = rs.colored_ir_auto_exposure_mode


def get_depth_sensor(dev):
    depth_sensor = dev.first_depth_sensor()
    if not depth_sensor.supports(AE_MODE):
        pytest.skip("auto exposure policy not exposed on this device")
    return depth_sensor


def test_ae_policy_range(test_device):
    dev, _ = test_device
    depth_sensor = get_depth_sensor(dev)

    r = depth_sensor.get_option_range(AE_MODE)
    assert r.min == POLICIES.auto
    assert r.max == POLICIES.hybrid
    assert r.step == 1


def test_ae_policy_set_get(test_device):
    dev, _ = test_device
    depth_sensor = get_depth_sensor(dev)

    original = depth_sensor.get_option(AE_MODE)
    try:
        for policy in (POLICIES.auto, POLICIES.color_priority, POLICIES.depth_priority, POLICIES.hybrid):
            depth_sensor.set_option(AE_MODE, policy)
            assert depth_sensor.get_option(AE_MODE) == policy
    finally:
        depth_sensor.set_option(AE_MODE, original)


def test_ae_policy_rejects_out_of_range(test_device):
    dev, _ = test_device
    depth_sensor = get_depth_sensor(dev)

    for bad in (int(POLICIES.hybrid) + 1, 10, 255):
        with pytest.raises(Exception):
            depth_sensor.set_option(AE_MODE, bad)


def test_ae_policy_rejected_while_streaming(test_device):
    # The firmware applies the policy at stream start, so the option is not settable while streaming.
    dev, _ = test_device
    depth_sensor = get_depth_sensor(dev)

    original = depth_sensor.get_option(AE_MODE)
    target = POLICIES.color_priority if original != POLICIES.color_priority else POLICIES.depth_priority

    depth_profile = next(p for p in depth_sensor.profiles
                         if p.stream_type() == rs.stream.depth and p.format() == rs.format.z16)
    depth_sensor.open(depth_profile)
    depth_sensor.start(lambda frame: None)
    try:
        with pytest.raises(Exception):
            depth_sensor.set_option(AE_MODE, target)
        assert depth_sensor.get_option(AE_MODE) == original
    finally:
        depth_sensor.stop()
        depth_sensor.close()

    # ... and settable again once streaming stopped
    try:
        depth_sensor.set_option(AE_MODE, target)
        assert depth_sensor.get_option(AE_MODE) == target
    finally:
        depth_sensor.set_option(AE_MODE, original)
