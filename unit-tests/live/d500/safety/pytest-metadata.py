# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Not frequently changing, no need to test for each commit

import pytest
import pyrealsense2 as rs
import logging
import metadata_common
from metadata_common import check_md_value, check_counter_and_timestamp_increase, reset_data
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D585S"),
    pytest.mark.context("nightly"),
]


safety_metadata_values = [rs.frame_metadata_value.frame_counter,
                          rs.frame_metadata_value.safety_depth_frame_counter,
                          rs.frame_metadata_value.frame_timestamp,
                          rs.frame_metadata_value.safety_level1,
                          rs.frame_metadata_value.safety_level2,
                          rs.frame_metadata_value.safety_level1_verdict,
                          rs.frame_metadata_value.safety_level2_verdict,
                          rs.frame_metadata_value.safety_operational_mode,
                          rs.frame_metadata_value.safety_vision_verdict,
                          rs.frame_metadata_value.safety_hara_events,
                          rs.frame_metadata_value.safety_preset_integrity,
                          rs.frame_metadata_value.safety_preset_id_used,
                          rs.frame_metadata_value.safety_mb_fusa_event,
                          rs.frame_metadata_value.safety_mb_fusa_action,
                          rs.frame_metadata_value.safety_mb_status]


def check_safety_metadata(frame):
    for md_value in safety_metadata_values:
        check_md_value(frame, md_value)


def test_safety_stream_metadata_received(test_context):
    cfg = rs.config()
    cfg.enable_stream(rs.stream.safety)
    pipe = rs.pipeline(test_context)
    pipe.start(cfg)
    iterations = 0
    while iterations < 20:
        iterations += 1
        f = pipe.wait_for_frames()
        check_safety_metadata(f)
    pipe.stop()


def test_safety_counter_and_timestamp_increase(test_context):
    cfg = rs.config()
    fps = 30
    cfg.enable_stream(rs.stream.safety, rs.format.y8, fps)
    pipe = rs.pipeline(test_context)
    pipe.start(cfg)
    iterations = 0
    reset_data()
    while iterations < 20:
        iterations += 1
        f = pipe.wait_for_frames()
        check_counter_and_timestamp_increase(f, fps)
    pipe.stop()
