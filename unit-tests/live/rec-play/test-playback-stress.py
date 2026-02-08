# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

#test:timeout 1500
#test:donotrun:!nightly
#test:donotrun  # disabled due to rosbag upgrade

import pyrealsense2 as rs, os
from rspy import log, test, repo
from playback_helper import PlaybackStatusVerifier
import time

# repo.build
file_name = os.path.join(repo.build, 'unit-tests', 'recordings', 'all_combinations_depth_color.bag' )
log.d( 'deadlock file:', file_name )
frames_in_bag_file = 64
number_of_iterations = 250

frames_count = 0

def frame_callback( f ):
    global frames_count
    frames_count += 1

################################################################################################
test.start( "Playback stress test" )

log.d( "Playing back: " + file_name )
for i in range(number_of_iterations):
    try:
        log.d("Test - Starting iteration # " , i)
        ctx = rs.context()
        dev = ctx.load_device( file_name )
        psv = PlaybackStatusVerifier( dev );
        dev.set_real_time( False )
        sensors = dev.query_sensors()
        frames_count = 0
        log.d("Opening Sensors")
        for sensor in sensors:
            sensor.open( sensor.get_stream_profiles() )
        log.d("Starting Sensors")
        for sensor in sensors:
            sensor.start( frame_callback )

        # We allow 10 seconds to each iteration to verify the playback_stopped event.
        timeout = 15
        number_of_statuses = 2
        psv.wait_for_status_changes(number_of_statuses,timeout);
        
        statuses = psv.get_statuses()
        # we expect to get start and then stop
        test.check_equal(number_of_statuses, len(statuses))
        test.check_equal(statuses[0], rs.playback_status.playing)
        test.check_equal(statuses[1], rs.playback_status.stopped)

        log.d("Stopping Sensors")
        for sensor in sensors:
            sensor.stop()

        log.d("Closing Sensors")
        for sensor in sensors:
            #log.d("Test Closing Sensor ", sensor)
            sensor.close()
        log.d("Test - Loop ended")
    except Exception:
        test.unexpected_exception()
    finally:
        test.check_equal(frames_count, frames_in_bag_file)
        dev = None
test.finish()
#############################################################################################

test.print_results_and_exit()
 