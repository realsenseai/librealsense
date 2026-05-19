# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs, os
from pytest_check import check
from rspy import repo
from playback_helper import PlaybackStatusVerifier
import time
import logging

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.timeout(1500),
    pytest.mark.context("weekly"),
]

frames_in_bag_file = 64
number_of_iterations = 250

frames_count = 0

def frame_callback( f ):
    global frames_count
    frames_count += 1

################################################################################################
def test_playback_stress():
    global frames_count
    log.info("Playback stress test")
    # repo.build
    file_name = os.path.join(repo.build, 'unit-tests', 'recordings', 'all_combinations_depth_color.bag' )
    log.debug(f'recorded file: {file_name}')

    log.debug("Playing back: " + file_name )
    for i in range(number_of_iterations):
        try:
            log.debug(f"Test - Starting iteration # {i}")
            ctx = rs.context()
            dev = ctx.load_device( file_name )
            psv = PlaybackStatusVerifier( dev );
            dev.set_real_time( False )
            sensors = dev.query_sensors()
            frames_count = 0
            log.debug("Opening Sensors")
            for sensor in sensors:
                sensor.open( sensor.get_stream_profiles() )
            log.debug("Starting Sensors")
            for sensor in sensors:
                sensor.start( frame_callback )

            # We allow 10 seconds to each iteration to verify the playback_stopped event.
            timeout = 15
            number_of_statuses = 2
            psv.wait_for_status_changes(number_of_statuses,timeout);

            statuses = psv.get_statuses()
            # we expect to get start and then stop
            check.equal(number_of_statuses, len(statuses))
            check.equal(statuses[0], rs.playback_status.playing)
            check.equal(statuses[1], rs.playback_status.stopped)

            log.debug("Stopping Sensors")
            for sensor in sensors:
                sensor.stop()

            log.debug("Closing Sensors")
            for sensor in sensors:
                #log.debug(f"Test Closing Sensor {sensor}")
                sensor.close()
            log.debug("Test - Loop ended")
        except Exception as e:
            check.fail(f"Unexpected exception: {e}")
        finally:
            check.equal(frames_count, frames_in_bag_file)
            dev = None
#############################################################################################
