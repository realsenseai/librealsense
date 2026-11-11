# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# On a dual-color (2C) device color shares the imagers with depth and IR, and the firmware refuses Emitter On/Off
# while they stream together. The SDK must report the control read-only then, and refuse it with a clear message.
# Setting it while only depth streams is covered by pytest-emitter-d500.py.

import pytest
import pyrealsense2 as rs


pytestmark = [
    pytest.mark.device_each( "Dual RGB" ),
    pytest.mark.context( "nightly" ),
]

W, H, FPS = 1280, 720, 30
OPT = rs.option.emitter_on_off


def profile(sensor, stream_type, index, fmt):
    for p in sensor.get_stream_profiles():
        vp = p.as_video_stream_profile()
        if vp and p.stream_type() == stream_type and p.stream_index() == index and p.format() == fmt                 and vp.width() == W and vp.height() == H and p.fps() == FPS:
            return p
    pytest.skip( f"Device publishes no {stream_type} {index} {W}x{H}@{FPS} {fmt}" )


# The lock checks depth and infrared separately
@pytest.mark.parametrize( "stream_type, index, fmt", [( rs.stream.depth, 0, rs.format.z16 ),
                                                      ( rs.stream.infrared, 1, rs.format.y8 )] )
def test_emitter_on_off_refused_while_streaming_with_color(test_device, stream_type, index, fmt):
    sensor = test_device[0].first_depth_sensor()   # on a dual-color device color streams from the depth sensor too
    sensor.open( [profile( sensor, stream_type, index, fmt ), profile( sensor, rs.stream.color, 1, rs.format.rgb8 )] )
    frames = rs.frame_queue()
    sensor.start( frames )
    try:
        frames.wait_for_frame()   # stopping before frames flow leaves the firmware unable to answer the next command
        assert sensor.is_option_read_only( OPT )   # the Viewer greys it out
        with pytest.raises( RuntimeError, match="cannot be changed while color is open together with depth or infrared" ):
            sensor.set_option( OPT, 1 )
    finally:
        sensor.stop()
        sensor.close()
