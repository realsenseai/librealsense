# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Shared helpers for the D401 GMSL dual-RGB tests. On D401 GMSL depth, IR and both color streams
# live on the depth sensor; Color 1 exists only on FW with the raw (CSI pass-through) path.

import time
import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

FPS = 30
DURATION_S = 4
MIN_RATIO = 0.75   # frames received / frames expected

COLOR0 = (rs.stream.color, 0)
COLOR1 = (rs.stream.color, 1)
DEPTH = (rs.stream.depth, 0)
IR1 = (rs.stream.infrared, 1)
IR2 = (rs.stream.infrared, 2)


def find_profile( sensor, key, fmt, w, h, fps=FPS ):
    """The video profile for stream `key` = (type, index) at w x h @ fps in `fmt`, or None."""
    for p in sensor.get_stream_profiles():
        vp = p.as_video_stream_profile()
        if vp and (p.stream_type(), p.stream_index()) == key and p.format() == fmt \
                and vp.width() == w and vp.height() == h and p.fps() == fps:
            return p
    return None


def need_profile( sensor, key, fmt, w, h, fps=FPS ):
    p = find_profile( sensor, key, fmt, w, h, fps )
    if p is None:
        pytest.skip( f"Device publishes no {key[0]} {key[1]} {w}x{h}@{fps} {fmt}" )
    return p


def has_color1( sensor ):
    return any( (p.stream_type(), p.stream_index()) == COLOR1 for p in sensor.get_stream_profiles() )


def skip_without_color1( dev ):
    """Skip unless the device publishes Color 1. That needs both FW 5.17.4.13+ (the SDK's gate) and a MIPI
    driver exposing the raw format, so checking the result covers both, whichever driver version added it."""
    if has_color1( dev.first_depth_sensor() ):
        return
    def info( i ):
        return dev.get_info( i ) if dev.supports( i ) else "unknown"
    pytest.skip( f"No Color 1 profile on FW {info( rs.camera_info.firmware_version )}, MIPI driver "
                 f"{info( rs.camera_info.mipi_driver_version )}: raw dual-RGB needs FW 5.17.4.13+ and a MIPI "
                 f"driver with the raw format" )


def stream( sensor, profiles, duration=DURATION_S ):
    """Stream `profiles` for `duration` seconds.

    Returns ({key: [frame numbers in arrival order]}, {key: data size of the first frame}).
    """
    numbers = {}
    sizes = {}
    def on_frame( f ):
        p = f.get_profile()
        key = (p.stream_type(), p.stream_index())
        numbers.setdefault( key, [] ).append( f.get_frame_number() )
        sizes.setdefault( key, f.get_data_size() )
    sensor.open( profiles )
    try:
        sensor.start( on_frame )
        try:
            time.sleep( duration )
        finally:
            sensor.stop()   # always stop: a GMSL stream left open can wedge the camera until a power cycle
    finally:
        sensor.close()
    return numbers, sizes


def check_rate( numbers, key, fps=FPS, duration=DURATION_S ):
    """Assert the stream delivered at least MIN_RATIO of its expected frames; return the count."""
    got = numbers.get( key, [] )
    log.debug( "%s %d: %d frames", key[0], key[1], len( got ) )
    assert len( got ) >= fps * duration * MIN_RATIO, f"{key[0]} {key[1]} delivered {len( got )} frames"
    return len( got )
