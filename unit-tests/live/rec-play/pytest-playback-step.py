# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# RSDEV-11578: paused frame-by-frame stepping showed a frozen image and logged
# "null frame passed to handle_frame". This mimics the viewer's step: pause(),
# then seek( position +- 1e9/fps ). Each paused seek must deliver a fresh frame
# whose timestamp moves with the step.

import datetime
import time
import os.path

import pyrealsense2 as rs
from pytest_check import check
from rspy import repo

RECORDING = "d555_all_streams_native_ros2.db3"
NUM_STEPS = 3
STEP_TIMEOUT_MS = 2000  # the broken behavior delivers nothing and times out


def test_playback_step():
    path = os.path.join( repo.build, "unit-tests", "recordings", RECORDING )

    dev = rs.context().load_device( path )
    dev.set_real_time( False )
    dev.pause()  # paused before streaming; every frame below comes from a step

    depth_sensor = next( s for s in dev.query_sensors()
                         if any( p.stream_type() == rs.stream.depth for p in s.get_stream_profiles() ) )
    depth_profile = next( p for p in depth_sensor.get_stream_profiles()
                          if p.stream_type() == rs.stream.depth )
    fps = depth_profile.fps() or 30  # native bags report fps=0
    step_ns = int( 1e9 / fps )       # one frame, the viewer's step size
    queue = rs.frame_queue( 10 )
    depth_sensor.open( depth_profile )
    depth_sensor.start( queue )

    def step( delta_ns ):
        target = dev.get_position() + delta_ns
        dev.seek( datetime.timedelta( microseconds=target / 1000 ) )
        success, frame = queue.try_wait_for_frame( STEP_TIMEOUT_MS )
        check.is_true( success, f"no frame delivered for paused seek to {target}" )
        ts = frame.get_timestamp() if success else None
        while True:  # drain leftovers so the next step reads fresh
            more, frame = queue.try_wait_for_frame( 50 )
            if not more:
                break
            ts = frame.get_timestamp()
        return ts

    try:
        time.sleep( 0.2 )  # let the sensor dispatcher spin up before the first step

        # forward, like the viewer's "next frame" button
        prev_ts = None
        for _ in range( NUM_STEPS ):
            ts = step( step_ns )
            if None not in (ts, prev_ts):
                check.greater( ts, prev_ts, "timestamp did not advance on forward step" )
            prev_ts = ts

        # backward, like the viewer's "previous frame" button; one less than forward so we
        # never step past the stream's first frame
        for _ in range( NUM_STEPS - 1 ):
            ts = step( -step_ns )
            if None not in (ts, prev_ts):
                check.less( ts, prev_ts, "timestamp did not recede on backward step" )
            prev_ts = ts
    finally:
        depth_sensor.stop()
        depth_sensor.close()
