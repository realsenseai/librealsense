# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Currently, we exclude D555 as it's failing

import pytest
import pyrealsense2 as rs, os, time, tempfile, platform, sys
from pytest_check import check
from rspy import frame_utils
import logging

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_exclude("D401"),
    pytest.mark.device_each("D500*"),
    pytest.mark.device_exclude("D555"),
]

cp = dp = None
color_format = depth_format = None
color_fps = depth_fps = None
color_width = depth_width = None
color_height = depth_height = None
previous_depth_frame_number = -1
previous_color_frame_number = -1
got_frames_rgb = False
got_frames_depth = False

is_d400 = False
is_jetson_context = False

# Our KPI is to prevent sequential frame drops, therefore single frame drop is allowed.
allowed_drops = 1

# finding the wanted profile settings. We want to use default settings except for color fps where we want
# the lowest value available
def find_default_profile_settings():
    global color_format, color_fps, color_width, color_height
    global depth_format, depth_fps, depth_width, depth_height
    for p in color_sensor.profiles:
        if p.is_default() and p.stream_type() == rs.stream.color:
            color_format = p.format()
            color_fps = p.fps()
            color_width = p.as_video_stream_profile().width()
            color_height = p.as_video_stream_profile().height()
            break
    for p in color_sensor.profiles:
        if p.stream_type() == rs.stream.color and p.format() == color_format and \
           p.fps() < color_fps and\
           p.as_video_stream_profile().width() == color_width and \
           p.as_video_stream_profile().height() == color_height:
            color_fps = p.fps()
    for p in depth_sensor.profiles:
        if p.is_default() and p.stream_type() == rs.stream.depth:
            depth_format = p.format()
            depth_fps = p.fps()
            depth_width = p.as_video_stream_profile().width()
            depth_height = p.as_video_stream_profile().height()
            break


def color_frame_call_back( frame ):
    global previous_color_frame_number
    global is_d400
    global allowed_drops
    global got_frames_rgb
    got_frames_rgb = True
    frame_utils.check_frame_drops( frame, previous_color_frame_number, allowed_drops, is_d400 )
    previous_color_frame_number = frame.get_frame_number()

def depth_frame_call_back( frame ):
    global previous_depth_frame_number
    global is_d400
    global allowed_drops
    global got_frames_depth

    got_frames_depth = True
    # On Jetson, capture-side drops surface here; skip the check (RSDEV-7935)
    if not is_jetson_context:
        frame_utils.check_frame_drops( frame, previous_depth_frame_number, allowed_drops, is_d400 )
    previous_depth_frame_number = frame.get_frame_number()

def restart_profiles():
    """
    You can't use the same profile twice, but we need the same profile several times. So this function resets the
    profiles with the given parameters to allow quick profile creation
    """
    global cp, dp, color_sensor, depth_sensor
    global color_format, color_fps, color_width, color_height
    global depth_format, depth_fps, depth_width, depth_height
    cp = next( p for p in color_sensor.profiles if p.fps() == color_fps
               and p.stream_type() == rs.stream.color
               and p.format() == color_format
               and p.as_video_stream_profile().width() == color_width
               and p.as_video_stream_profile().height() == color_height )

    dp = next( p for p in depth_sensor.profiles if p.fps() == depth_fps
               and p.stream_type() == rs.stream.depth
               and p.format() == depth_format
               and p.as_video_stream_profile().width() == depth_width
               and p.as_video_stream_profile().height() == depth_height )

def stop_pipeline( pipeline ):
    if pipeline:
        try:
            pipeline.stop()
        except RuntimeError as rte:
            # if the error Occurred because the pipeline wasn't started we ignore it
            if str( rte ) != "stop() cannot be called before start()":
                check.fail(f"Unexpected exception: {rte}")
        except Exception as e:
            check.fail(f"Unexpected exception: {e}")

def stop_sensor( sensor ):
    if sensor:
        # if the sensor is already closed get_active_streams returns an empty list
        if sensor.get_active_streams():
            try:
                sensor.stop()
                sensor.close()
            except RuntimeError as rte:
                if str( rte ) != "stop_streaming() failed. UVC device is not streaming!":
                    check.fail(f"Unexpected exception: {rte}")
            except Exception as e:
                check.fail(f"Unexpected exception: {e}")


################################################################################################
def test_pipeline_interface(test_device):
    log.info("Trying to record and playback using pipeline interface")
    dev, ctx = test_device

    # create temporary folder to record to that will be deleted automatically at the end of the script
    # (requires that no files are being held open inside this directory. Important to not keep any handle open to a file
    # in this directory, any handle as such must be set to None)
    temp_dir = tempfile.TemporaryDirectory( prefix='recordings_' )
    file_name = temp_dir.name + os.sep + 'rec.db3'

    cfg = pipeline = None
    try:
        # creating a pipeline and recording to a file
        pipeline = rs.pipeline(ctx)
        cfg = rs.config()
        cfg.enable_record_to_file( file_name )
        pipeline.start( cfg )
        time.sleep(3)
        pipeline.stop()
        # we create a new pipeline and use it to playback from the file we just recoded to
        pipeline = rs.pipeline(ctx)
        cfg = rs.config()
        cfg.enable_device_from_file(file_name)
        pipeline.start(cfg)
        # if the record-playback worked we will get frames, otherwise the next line will timeout and throw
        pipeline.wait_for_frames()
    except Exception as e:
        check.fail(f"Unexpected exception: {e}")
    finally: # we must remove all references to the file so we can use it again in the next test
        cfg = None
        stop_pipeline( pipeline )


################################################################################################
def test_sensor_interface(test_device, test_context_var):
    global previous_depth_frame_number, previous_color_frame_number, got_frames_rgb, got_frames_depth, is_jetson_context
    previous_depth_frame_number = -1
    previous_color_frame_number = -1
    got_frames_rgb = False
    got_frames_depth = False
    is_jetson_context = 'jetson' in test_context_var

    log.info("Trying to record and playback using sensor interface")
    global dev, ctx, depth_sensor, color_sensor, is_d400
    dev, ctx = test_device
    depth_sensor = dev.first_depth_sensor()
    color_sensor = dev.first_color_sensor()

    # The test also checks frame drops, therefore D400-specific relaxation must apply
    # The follow code is borrowed fro test-drops-on-set.py and later can be merged/refactored
    is_d400 = (dev.get_info(rs.camera_info.product_line) == "D400")

    find_default_profile_settings()

    temp_dir = tempfile.TemporaryDirectory( prefix='recordings_' )
    file_name = temp_dir.name + os.sep + 'rec.db3'

    recorder = playback = None
    try:
        recorder = rs.recorder( file_name, dev )

        restart_profiles()

        depth_sensor.open( dp )
        depth_sensor.start( lambda f: None )
        color_sensor.open( cp )
        color_sensor.start( lambda f: None )

        time.sleep(3)

        recorder.pause()

        stop_sensor(depth_sensor)
        stop_sensor(color_sensor)

        recorder = None

        color_filters = [f.get_info(rs.camera_info.name) for f in color_sensor.get_recommended_filters()]
        depth_filters = [f.get_info(rs.camera_info.name) for f in depth_sensor.get_recommended_filters()]

        check.is_true( len(color_filters) > 0 )
        check.is_true( len(depth_filters) > 0 )

        playback = ctx.load_device( file_name )

        depth_sensor = playback.first_depth_sensor()
        color_sensor = playback.first_color_sensor()

        playback_color_filters = [f.get_info(rs.camera_info.name) for f in color_sensor.get_recommended_filters()]
        playback_depth_filters = [f.get_info(rs.camera_info.name) for f in depth_sensor.get_recommended_filters()]

        check.equal( playback_color_filters, color_filters )
        check.equal( playback_depth_filters, depth_filters )

        restart_profiles()

        depth_sensor.open( dp )
        depth_sensor.start( depth_frame_call_back )
        color_sensor.open( cp )
        color_sensor.start( color_frame_call_back )

        time.sleep(3)

        # if record and playback worked we will receive frames, the callback functions will be called and got-frames
        # will be True. If the record and playback failed it will be false
        check.is_true( got_frames_depth )
        check.is_true( got_frames_rgb )
    except Exception as e:
        check.fail(f"Unexpected exception: {e}")
    finally: # we must remove all references to the file so we can use it again in the next test
        stop_sensor( depth_sensor )
        stop_sensor( color_sensor )
        depth_sensor = None
        color_sensor = None
        if recorder:
            recorder = None
        if playback:
            playback = None


#####################################################################################################
def test_sensor_interface_with_syncer(test_device):
    log.info("Trying to record and playback using sensor interface with syncer")
    global dev, ctx, depth_sensor, color_sensor
    dev, ctx = test_device
    depth_sensor = dev.first_depth_sensor()
    color_sensor = dev.first_color_sensor()
    find_default_profile_settings()

    temp_dir = tempfile.TemporaryDirectory( prefix='recordings_' )
    file_name = temp_dir.name + os.sep + 'rec.db3'

    recorder = playback = None
    try:
        sync = rs.syncer()
        recorder = rs.recorder( file_name, dev )

        restart_profiles()

        depth_sensor.open( dp )
        depth_sensor.start( sync )
        color_sensor.open( cp )
        color_sensor.start( sync )

        time.sleep(3)

        recorder.pause()

        stop_sensor(depth_sensor)
        stop_sensor(color_sensor)

        recorder = None

        playback = ctx.load_device( file_name )

        depth_sensor = playback.first_depth_sensor()
        color_sensor = playback.first_color_sensor()

        restart_profiles()

        depth_sensor.open( dp )
        depth_sensor.start( sync )
        color_sensor.open( cp )
        color_sensor.start( sync )

        # if the record-playback worked we will get frames, otherwise the next line will timeout and throw
        sync.wait_for_frames()
    except Exception as e:
        check.fail(f"Unexpected exception: {e}")
    finally: # we must remove all references to the file so the temporary folder can be deleted
        stop_sensor( depth_sensor )
        stop_sensor( color_sensor )
        depth_sensor = None
        color_sensor = None
        if recorder:
            recorder = None
        if playback:
            playback = None
