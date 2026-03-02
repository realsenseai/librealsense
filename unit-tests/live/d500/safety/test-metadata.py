# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

# Not frequently changing, no need to test for each commit
# test:donotrun:!nightly
# test:device D585S

import pyrealsense2 as rs
from rspy import test,log
from metadata_common import *
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

################# Checking safety metadata required fileds are received ##################
test.start("Checking safety stream metadata received")

cfg = rs.config()
cfg.enable_stream(rs.stream.safety)
pipe = rs.pipeline()
pipe.start(cfg)
iterations = 0
while iterations < 20:
    iterations += 1
    f = pipe.wait_for_frames()
    check_safety_metadata(f)
pipe.stop()
test.finish()

################# Checking safety frame counter and timestamp increasing ##################
test.start("Checking safety stream metadata frame counter and timestamp increasing")
cfg = rs.config()
fps = 30
cfg.enable_stream(rs.stream.safety, rs.format.y8, fps)
pipe = rs.pipeline()
pipe.start(cfg)
iterations = 0
while iterations < 20:
    iterations += 1
    f = pipe.wait_for_frames()
    check_counter_and_timestamp_increase(f, fps)
pipe.stop()
test.finish()

test.print_results_and_exit()
