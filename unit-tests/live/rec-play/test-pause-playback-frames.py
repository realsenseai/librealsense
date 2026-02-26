# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2021 RealSense, Inc. All Rights Reserved.


#test:device D400*
#test:device D500*
# Running weekly as this tests specific bug fix in a code that is seldom touched, no need to run for each push.
# test:donotrun:!weekly

# Objective:
#
# Verify that pause & resume did not mess up the recorded timestamps and the sleep time between each 2 frames is
# reasonable. We had a BUG with calculating the sleep time between each 2 frames when the pause action occurred
# before the recording base time was set (first frame arrival time), causing the recorded bag file "capture
# time" to go up and down, and therefore huge sleep times. See [RSDSO-14342]
#
# Here we test multiple flows on pause & resume actions and verify that the whole file will be played until a
# stop event (EOF) within a reasonable time.

import pyrealsense2 as rs, os, time, tempfile
from rspy import log, test
from playback_helper import PlaybackStatusVerifier

# create temporary folder to record to that will be deleted automatically at the end of the script
# (requires that no files are being held open inside this directory. Important to not keep any handle open to a file
# in this directory, any handle as such must be set to None)
temp_dir = tempfile.TemporaryDirectory( prefix='recordings_' )
file_name = temp_dir.name + os.sep + 'rec.bag'

stop_detected = False
STREAMING_DURATION = 3
TIMEOUT_BUFFER = 3  # [sec] extra time to the expected playback time for not failing on runtime hiccups..


def record_with_pause( file_name, iterations, pause_delay=0, resume_delay=0 ):
    # creating a pipeline and recording to a file
    pipeline = rs.pipeline()
    cfg = rs.config()
    cfg.enable_record_to_file( file_name )
    pipeline_record_profile = pipeline.start( cfg )
    device = pipeline_record_profile.get_device()
    device_recorder = device.as_recorder()

    for i in range( iterations ):
        if pause_delay > 0:
            log.d('Sleeping for', pause_delay, '[sec]')
            time.sleep( pause_delay )
        log.d( 'Pausing...' )
        rs.recorder.pause(device_recorder)

        if resume_delay > 0:
            log.d('Sleeping for', resume_delay, '[sec]')
            time.sleep( resume_delay )
        log.d('Resumed...')
        rs.recorder.resume( device_recorder )
        time.sleep( STREAMING_DURATION )

    pipeline.stop()
    return calc_playback_timeout( iterations, pause_delay )


def playback( pipeline, file_name ):
    cfg = rs.config()
    cfg.enable_device_from_file( file_name, repeat_playback=False )
    log.d( 'Playing...' )
    pipeline_playback_profile = pipeline.start( cfg )
    device = pipeline_playback_profile.get_device()
    playback_dev = device.as_playback()
    # We force realtime=True to ensure that a sleep with be performed between frames while playback is on,
    # without it we would have to manually look at the frame timestamps. Instead, we turn it on and depend on the
    # timeout, albeit at the cost of playback runtime.
    playback_dev.set_real_time( True )
    pipeline.wait_for_frames()
    test.check_equal( playback_dev.current_status(), rs.playback_status.playing )
    return playback_dev




def calc_playback_timeout( iterations, pause_delay ):
    global TIMEOUT_BUFFER
    # NOTE: the recording resume-delay is the time we have paused the stream, and is not
    # reflected in the playback! Therefore it's not reflected here:
    return iterations * ( pause_delay + STREAMING_DURATION ) + TIMEOUT_BUFFER


################################################################################################
#
test.start("Immediate pause & test")
# probably pause & resume will occur before recording base time is set.

try:
    timeout = record_with_pause( file_name, iterations = 1, pause_delay = 0, resume_delay = 0 )
    pipeline = rs.pipeline()
    device_playback = playback( pipeline, file_name )
    psv = PlaybackStatusVerifier( device_playback );
    psv.wait_for_status(timeout, rs.playback_status.stopped)
except Exception:
    test.unexpected_exception()
finally:  # remove all references to the file and dereference the pipeline
    device_playback = None
    pipeline = None

test.finish()
#
################################################################################################
#
test.start("Immediate pause & delayed resume test")

# Pause time should be lower than recording base time and resume time higher
try:
    timeout = record_with_pause( file_name, iterations = 1, pause_delay = 0, resume_delay = 5 )
    pipeline = rs.pipeline()
    device_playback = playback( pipeline, file_name )
    psv = PlaybackStatusVerifier( device_playback );
    psv.wait_for_status( timeout, rs.playback_status.stopped )
except Exception:
    test.unexpected_exception()
finally:   # remove all references to the file and dereference the pipeline
    device_playback = None
    pipeline = None

test.finish()
#
################################################################################################
#
test.start("delayed pause & delayed resume test")
# Pause & resume will occur after recording base time is set
try:
    timeout = record_with_pause( file_name, iterations = 1, pause_delay = 3, resume_delay = 2 )
    pipeline = rs.pipeline()
    device_playback = playback( pipeline, file_name )
    psv = PlaybackStatusVerifier( device_playback );
    psv.wait_for_status( timeout, rs.playback_status.stopped )
except Exception:
    test.unexpected_exception()
finally:   # remove all references to the file and dereference the pipeline
    device_playback = None
    pipeline = None

test.finish()
#
################################################################################################
#
test.start("multiple delay & pause test")
# Combination of some of the previous tests, testing accumulated recording capture time

try:
    timeout = record_with_pause( file_name, iterations = 2, pause_delay = 0, resume_delay = 2 )
    pipeline = rs.pipeline()
    device_playback = playback( pipeline, file_name )
    psv = PlaybackStatusVerifier( device_playback );
    psv.wait_for_status( timeout, rs.playback_status.stopped )
except Exception:
    test.unexpected_exception()
finally:   # remove all references to the file and dereference the pipeline
    device_playback = None
    pipeline = None

test.finish()
#
#############################################################################################
#
test.print_results_and_exit()
