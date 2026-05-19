# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import json
import logging

import pytest
import pyrealsense2 as rs
from pytest_check import check

import hdr_helper
from hdr_helper import HDR_CONFIGURATIONS, MANUAL_HDR_CONFIG_1

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D455"),
    pytest.mark.device_each("D457"),
    pytest.mark.context("nightly"),
]

# Different depth resolutions to test
DEPTH_RESOLUTIONS = [
    (640, 480),
    (848, 480),
    (1280, 720),
]


@pytest.mark.parametrize("config_idx,config",
                         list(enumerate(HDR_CONFIGURATIONS)),
                         ids=[f"cfg{i+1}" for i in range(len(HDR_CONFIGURATIONS))])
@pytest.mark.parametrize("resolution",
                         DEPTH_RESOLUTIONS,
                         ids=[f"{r[0]}x{r[1]}" for r in DEPTH_RESOLUTIONS])
def test_hdr_configurations(test_device, config_idx, config, resolution):
    """
    Test each HDR configuration with different depth resolutions
    """
    hdr_helper.setup_for_device(test_device)
    config_type = "Auto" if "depth-ae" in json.dumps(config) else "Manual"
    num_items = len(config["hdr-preset"]["items"])
    test_name = f"Config {config_idx + 1} ({config_type}, {num_items} items) @ {resolution[0]}x{resolution[1]}"
    hdr_helper.load_and_perform_test(config, test_name, resolution)


def test_disable_auto_hdr(test_device):
    """
    Test disabling Auto-HDR and returning to default behavior
    """
    hdr_helper.setup_for_device(test_device)
    am = hdr_helper.am
    sensor = hdr_helper.sensor
    pipe = hdr_helper.pipe

    cfg = rs.config()
    log.info("Disable Auto-HDR - Return to default behavior")
    # First enable HDR
    am.load_json(json.dumps(MANUAL_HDR_CONFIG_1))
    check.equal(sensor.get_option(rs.option.hdr_enabled), 1)

    # Disable HDR
    sensor.set_option(rs.option.hdr_enabled, 0)
    check.equal(sensor.get_option(rs.option.hdr_enabled), 0)

    # Verify we're back to default single-frame behavior
    cfg.enable_stream(rs.stream.depth, 640, 480, rs.format.z16, 30)
    pipe.start(cfg)

    for _ in range(30):
        data = pipe.wait_for_frames()
        depth_frame = data.get_depth_frame()

        # In default mode, sequence size should be 0
        seq_size = depth_frame.get_frame_metadata(rs.frame_metadata_value.sequence_size)
        check.equal(seq_size, 0, f"Expected sequence size 0 in default mode, got {seq_size}")

        # Sequence ID should always be 0 in single-frame mode
        seq_id = depth_frame.get_frame_metadata(rs.frame_metadata_value.sequence_id)
        check.equal(seq_id, 0, f"Expected sequence ID 0 in default mode, got {seq_id}")

    pipe.stop()
    cfg.disable_all_streams()
