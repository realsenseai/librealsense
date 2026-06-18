# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import logging
import pyrealsense2 as rs
from pytest_check import check

log = logging.getLogger(__name__)

prev_counter = 0
prev_ts = 0

def reset_data():
    global prev_counter, prev_ts
    prev_counter = 0
    prev_ts = 0

# if you call it in more than 1 test remember to first call 'reset_data()'
def check_counter_and_timestamp_increase(frame, fps):
    global prev_counter, prev_ts
    if prev_counter == 0 and prev_ts == 0:
        prev_counter = frame.get_frame_metadata(rs.frame_metadata_value.frame_counter)
        prev_ts = frame.get_frame_metadata(rs.frame_metadata_value.frame_timestamp)
    else:
        current_counter = frame.get_frame_metadata(rs.frame_metadata_value.frame_counter)
        current_ts = frame.get_frame_metadata(rs.frame_metadata_value.frame_timestamp)
        log.debug("prev_counter %s", prev_counter)
        log.debug("current_counter %s", current_counter)
        log.debug("prev_ts %s", prev_ts)
        log.debug("current_ts %s", current_ts)
        check.is_true(current_counter > prev_counter)  # D500 has a skip frames mechanism on low fps meaning no sequential frame numbers
        check.is_true((current_ts - prev_ts) / 1000 < 2 * 1000 / fps)
        prev_counter = current_counter
        prev_ts = current_ts

def check_md_value(frame, md_value):
    check.is_true(frame.supports_frame_metadata(md_value))
    val = frame.get_frame_metadata(md_value)
    log.debug("%s: %s", repr(md_value), repr(val))
