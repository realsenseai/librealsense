# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023-2024 RealSense, Inc. All Rights Reserved.

# test:donotrun:!nightly
# test:device each(D400*)
# test:device each(D500*) 
# On D455 and other units with IMU it takes ~4 seconds per iteration
# test:timeout 220

import pyrealsense2 as rs
from rspy.stopwatch import Stopwatch
from rspy import test, log
import time

# Run multiple start stop of all streams and verify we get a frame for each once
# relaxed to 3 as 50 was failing often, See [LRS-1213]
ITERATIONS_COUNT = 3

dev, ctx = test.find_first_device_or_exit()
pipe = rs.pipeline(ctx)
pipe.set_device(dev)

def run_and_verify_frame_received():    
    start_call_stopwatch = Stopwatch()
    pipe.start()
    # wait_for_frames will throw if no frames received so no assert is needed
    f = pipe.wait_for_frames()
    delay = start_call_stopwatch.get_elapsed()
    log.out("After ", delay, " [sec] got first frame of ", f)
    pipe.stop()


################################################################################################
test.start("Testing pipeline start/stop")
iteration_stopwatch = Stopwatch()
for i in range( ITERATIONS_COUNT ):
    iteration_stopwatch.reset()
    log.out("starting iteration #", i + 1, "/", ITERATIONS_COUNT)
    # When we had this line enabled and we used it on "run_and_verify_frame_received" the pipeline failed on second iteration on D455
    # IR frames do not arrive on 2-nd iteration
    # This is investigated in LRS-972 

    #cfg.enable_all_streams()
    run_and_verify_frame_received()
    iteration_time = iteration_stopwatch.get_elapsed()
    log.out("iteration took ", iteration_time, " [sec]")
test.finish()

################################################################################################
test.print_results_and_exit()
