# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import os
import pytest
import pyrealsense2 as rs2
from rspy import repo
import tempfile
import logging

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
    pytest.mark.timeout(20),
    pytest.mark.context("weekly"),
]

# This test checks that stop of pipeline with playback file
# and non realtime mode is not stuck due to deadlock of
# pipeline stop thread and syncer blocking enqueue thread (DSO-15157)
#############################################################################################
def test_non_realtime_stop(test_device):
    log.info("Playback with non realtime isn't stuck at stop")

    filename = os.path.join( repo.build, 'unit-tests', 'recordings', 'recording_deadlock.bag' )
    log.debug(f'deadlock file: {filename}')

    pipeline = rs2.pipeline()
    config = rs2.config()
    config.enable_all_streams()
    config.enable_device_from_file(filename, repeat_playback=False)
    profile = pipeline.start(config)
    device = profile.get_device().as_playback().set_real_time(False)
    success = True
    while success:
        success, _ = pipeline.try_wait_for_frames(1000)
    print("stopping...")
    pipeline.stop()
    print("stopped")
#############################################################################################
