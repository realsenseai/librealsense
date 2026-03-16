# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

# test:device each(D400*)
# test:donotrun:!weekly

import pyrealsense2 as rs
from rspy import log, repo, test
import threading
import time

# This test objective is to check the locking mechanism on UVC devices (MIPI classes extend UVC).
# HWMC lock the device and are also "invoke_power"ed so we use lots of them with several threads trying to send commands simultaneously.
# We set visual preset as internally it issues many commands - PU, XU and HWM.
    
def change_presets( dev, index, delay ):
    depth_sensor = dev.first_depth_sensor()
    test.check( depth_sensor.supports( rs.option.visual_preset ) )
    time.sleep( delay )
    for i in range(10):
        try:
            log.d( "Thread", index, "setting high_accuracy" )
            start_time = time.perf_counter()
            depth_sensor.set_option( rs.option.visual_preset, int(rs.rs400_visual_preset.high_accuracy ) )
            end_time = time.perf_counter()
            log.d( "Thread", index, "setting high_accuracy took ", end_time - start_time )
            log.d( "Thread", index, "setting default" )
            start_time = time.perf_counter()
            depth_sensor.set_option( rs.option.visual_preset, int(rs.rs400_visual_preset.default ) )
            end_time = time.perf_counter()
            log.d( "Thread", index, "setting default took ", end_time - start_time )
        except:
            test.unexpected_exception()


with test.closure( 'Setup', on_fail=test.ABORT ):
    ctx = rs.context( { 'dds': { 'enabled': False } } ) # We want to test UVC locking so no DDS
    pipe = rs.pipeline(ctx)
    devices = ctx.query_devices()
    test.check( len( devices ) > 0 )
    threads = []
    for i in range( len( devices ) ):
        # Open 2 threads per device. Calling devices[i] twice creates two different device objects for the same camera.
        t1 = threading.Thread( target = change_presets, args = ( devices[i], i * 2, 0.05 ) )
        threads.append( t1 )
        t2 = threading.Thread( target = change_presets, args = ( devices[i], i * 2 + 1, 0.1) )
        threads.append( t2 )


with test.closure( 'Run threads' ):
    for t in threads:
        t.start()

with test.closure( 'Issue GVD commands in the middle' ):
    raw_command = rs.debug_protocol( devices[0] ).build_command( 0x10 ) # 0x10 is GVD opcode
    for i in range( 10 ):
        log.d( "Sending GVD commands" )
        for j in range( len( devices ) ):
            start_time = time.perf_counter()
            raw_result = rs.debug_protocol( devices[j] ).send_and_receive_raw_data( raw_command )
            end_time = time.perf_counter()
            log.d( "Got device ", j, " GVD in ", end_time - start_time )
        time.sleep( 0.5 )

with test.closure( 'Wait for threads to finish' ):
    for t in threads:
        t.join()

