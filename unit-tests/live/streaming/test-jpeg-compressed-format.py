# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

# test:device D555
# test:donotrun:!dds
# test:donotrun:!nightly


import pyrealsense2 as rs
from rspy import test
from rspy import log


with test.closure( "Check jpeg format support"):
    ctx = rs.context({"format-conversion":"raw"})
    color_sensor = ctx.query_devices()[0].first_color_sensor()
    jpeg_profile = None
    for p in color_sensor.profiles:
        if p.stream_type() == rs.stream.color and p.format() == rs.format.mjpeg:
            jpeg_profile = p
            break
                
if jpeg_profile:
    log.d("Device supports jpeg streaming with profile:", jpeg_profile)
else:
    log.d("Device does not support jpeg streaming")
    test.print_results_and_exit()
    
    
    
with test.closure("Check streaming and conversion to RGB8"):
    pipeline = rs.pipeline()
    config = rs.config()
    vp = jpeg_profile.as_video_stream_profile()
    config.enable_stream(rs.stream.color, vp.stream_index(), vp.width(), vp.height(), rs.format.rgb8, vp.fps()) # JPEG is converted to RGB8
    pipeline.start(config)

    for i in range(10):
        frames = pipeline.wait_for_frames()
        log.d( frames )    
  
test.print_results_and_exit()
