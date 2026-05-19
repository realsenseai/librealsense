# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

import logging
import pyrealsense2 as rs
from pytest_check import check
import time

log = logging.getLogger(__name__)


# Constants
#
domain = rs.timestamp_domain.hardware_clock       # For either depth/color
#
# To be set before init() or playback()
#
fps_c = fps_d = 60
w = 640
h = 480
bpp = 2  # bytes
#
# Set by init() or playback() -- don't set these unless you know what you're doing!
#
gap_c = gap_d = 0
pixels = None
device = None
depth_sensor = None
color_sensor = None
depth_profile = None
color_profile = None
syncer = None
playback_status = None


def init( syncer_matcher = rs.matchers.default ):
    """
    One of the two initialization functions:

    Use init() to initialize a software device that will generate frames (as opposed to playback()
    which will initialize a software device for reading from a rosbag and will NOT generate frames).

    This should be followed by start() to actually start "streaming".

    This sets multiple module variables that are used to generate software frames, and initializes
    a syncer automatically if needed.

    :param syncer_matcher: The matcher to use in the syncer (if None, no syncer will be used) --
                           the default will compare all streams according to timestamp
    """
    global gap_c, gap_d
    gap_d = 1000 / fps_d
    gap_c = 1000 / fps_c
    #
    global pixels, w, h
    pixels = bytearray( b'\x00' * ( w * h * 2 ))  # Dummy data
    #
    global device
    device = rs.software_device()
    if syncer_matcher is not None:
        device.create_matcher( syncer_matcher )
    #
    global depth_sensor, color_sensor
    depth_sensor = device.add_sensor( "Depth" )
    color_sensor = device.add_sensor( "Color" )
    #
    depth_stream = rs.video_stream()
    depth_stream.type = rs.stream.depth
    depth_stream.uid = 0
    depth_stream.width = 640
    depth_stream.height = 480
    depth_stream.bpp = 2
    depth_stream.fmt = rs.format.z16
    depth_stream.fps = fps_d
    #
    global depth_profile
    depth_profile = rs.video_stream_profile( depth_sensor.add_video_stream( depth_stream ))
    #
    color_stream = rs.video_stream()
    color_stream.type = rs.stream.color
    color_stream.uid = 1
    color_stream.width = 640
    color_stream.height = 480
    color_stream.bpp = 2
    color_stream.fmt = rs.format.rgba8
    color_stream.fps = fps_c
    #
    global color_profile
    color_profile = rs.video_stream_profile( color_sensor.add_video_stream( color_stream ))
    #
    # We don't want to lose any frames so use a big queue size (default is 1)
    global syncer
    if syncer_matcher is not None:
        syncer = rs.syncer( 100 )  
    else:
        syncer = rs.frame_queue( 100 )
    #
    global playback_status
    playback_status = None


def playback_callback( status ):
    """
    """
    global playback_status
    playback_status = status
    log.debug( "... %s", status )


def playback( filename, use_syncer = True ):
    """
    One of the two initialization functions:

    Use playback() to initialize a software device for reading from a rosbag file. This device will
    NOT generate frames! Only the expect() functions will be available.

    If you use this function, it replaces init().

    This should be followed by start() to actually start "streaming".

    :param filename: The path to the file to open for playback
    :param use_syncer: If True, a syncer will be used to attempt frame synchronization -- otherwise
                       a regular queue will be used
    """
    ctx = rs.context()
    #
    global device
    device = rs.playback( ctx.load_device( filename ) )
    device.set_real_time( False )
    device.set_status_changed_callback( playback_callback )
    #
    global depth_sensor, color_sensor
    sensors = device.query_sensors()
    depth_sensor = next( s for s in sensors if s.name == "Depth" )
    color_sensor = next( s for s in sensors if s.name == "Color" )
    #
    global depth_profile, color_profile
    depth_profile = next( p for p in depth_sensor.profiles if p.stream_type() == rs.stream.depth )
    color_profile = next( p for p in color_sensor.profiles if p.stream_type() == rs.stream.color )
    #
    global syncer
    if use_syncer:
        syncer = rs.syncer( 100 )  # We don't want to lose any frames so uses a big queue size (default is 1)
    else:
        syncer = rs.frame_queue( 100 )
    #
    global playback_status
    playback_status = rs.playback_status.unknown


def start():
    """
    """
    global depth_profile, color_profile, depth_sensor, color_sensor, syncer
    depth_sensor.open( depth_profile )
    color_sensor.open( color_profile )
    depth_sensor.start( syncer )
    color_sensor.start( syncer )


def stop():
    """
    """
    global depth_profile, color_profile, depth_sensor, color_sensor
    color_sensor.stop()
    depth_sensor.stop()
    color_sensor.close()
    depth_sensor.close()


def reset():
    """
    """
    global depth_profile, color_profile, depth_sensor, color_sensor
    color_sensor = None
    depth_sensor = None
    depth_profile = None
    color_profile = None
    #
    global device
    device = None
    #
    global syncer
    syncer = None


def generate_depth_frame( frame_number, timestamp, next_expected=None ):
    """
    """
    global playback_status
    if playback_status is not None:
        raise RuntimeError( "cannot generate frames when playing back" )
    #
    global depth_profile, domain, pixels, depth_sensor, w, bpp
    depth_frame = rs.software_video_frame()
    depth_frame.pixels = pixels
    depth_frame.stride = w * bpp
    depth_frame.bpp = bpp
    depth_frame.frame_number = frame_number
    depth_frame.timestamp = timestamp
    depth_frame.domain = domain
    depth_frame.profile = depth_profile
    #
    if next_expected is not None:
        actual_fps = round( 1000000 / ( next_expected - timestamp ) )
        depth_sensor.set_metadata( rs.frame_metadata_value.actual_fps, actual_fps )
        log.debug( "--> %s with actual FPS %s", depth_frame, actual_fps )
    else:
        depth_sensor.set_metadata( rs.frame_metadata_value.actual_fps, 0 )  # force to not use
        log.debug( "--> %s", depth_frame )
    depth_sensor.on_video_frame( depth_frame )

def generate_color_frame( frame_number, timestamp, next_expected=None ):
    """
    """
    global playback_status
    if playback_status is not None:
        raise RuntimeError( "cannot generate frames when playing back" )
    #
    global color_profile, domain, pixels, color_sensor, w, bpp
    color_frame = rs.software_video_frame()
    color_frame.pixels = pixels
    color_frame.stride = w * bpp
    color_frame.bpp = bpp
    color_frame.frame_number = frame_number
    color_frame.timestamp = timestamp
    color_frame.domain = domain
    color_frame.profile = color_profile
    #
    if next_expected is not None:
        actual_fps = round( 1000000 / ( next_expected - timestamp ) )
        color_sensor.set_metadata( rs.frame_metadata_value.actual_fps, actual_fps )
        log.debug( "--> %s with actual FPS %s", color_frame, actual_fps )
    else:
        color_sensor.set_metadata( rs.frame_metadata_value.actual_fps, 0 )  # force to not use
        log.debug( "--> %s", color_frame )
    color_sensor.on_video_frame( color_frame )

def generate_depth_and_color( frame_number, timestamp ):
    generate_depth_frame( frame_number, timestamp )
    generate_color_frame( frame_number, timestamp )

def expect( depth_frame = None, color_frame = None, nothing_else = False ):
    """
    Looks at the syncer queue and gets the next frame from it if available, checking its contents
    against the expected frame numbers.
    """
    global syncer, playback_status
    f = syncer.poll_for_frame()
    if playback_status is not None:
        countdown = 50  # 5 seconds
        while not f  and  playback_status != rs.playback_status.stopped:
            countdown -= 1
            if countdown == 0:
                break
            time.sleep( 0.1 )
            f = syncer.poll_for_frame()
    # NOTE: f will never be None
    if not f:
        check.is_true( depth_frame is None, "expected a depth frame" )
        check.is_true( color_frame is None, "expected a color frame" )
        return False

    log.debug( "Got %s", f )

    fs = rs.composite_frame( f )

    if fs:
        depth = fs.get_depth_frame()
    else:
        depth = rs.depth_frame( f )
    log.debug( "actual depth %s", depth )
    check.equal( depth_frame is None, not depth )
    if depth_frame is not None and depth:
        check.equal( depth.get_frame_number(), depth_frame )

    if fs:
        color = fs.get_color_frame()
    elif not depth:
        color = rs.video_frame( f )
    else:
        color = None
    log.debug( "actual color %s", color )
    check.equal( color_frame is None, not color )
    if color_frame is not None and color:
        check.equal( color.get_frame_number(), color_frame )

    if nothing_else:
        f = syncer.poll_for_frame()
        log.debug( "Expected nothing else; actual %s", f )
        check.is_true( not f )

    return True

def expect_nothing():
    expect( nothing_else = True )

