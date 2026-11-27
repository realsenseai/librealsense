# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs, os, time, tempfile
from contextlib import contextmanager
from pytest_check import check
from rspy import frame_utils
from rspy.timer import Timer
import logging

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.extras_type_exclude("GMSL"),   # the USB dual RGB and GMSL dedicated sensor runs cover it
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
]

RECORD_TIME = 3           # [sec] gating and nightly; module set-up dominates their run time
WEEKLY_RECORD_TIME = 120  # [sec] long recording, played back in full
FPS_TOLERANCE = 0.1     # recording writes to disk on top of streaming
# Our KPI is to prevent sequential frame drops, therefore single frame drop is allowed.
allowed_drops = 1


def same_mode( p, q, fps=True ):
    a, b = p.as_video_stream_profile(), q.as_video_stream_profile()
    return ( p.stream_type(), p.stream_index(), p.format(), a.width(), a.height() ) == \
           ( q.stream_type(), q.stream_index(), q.format(), b.width(), b.height() ) and ( not fps or p.fps() == q.fps() )


def pick_profiles( dev ):
    """Default depth profile, plus the default color resolution at its lowest fps when the device has color.
    Without a color sensor (D405) color comes from the depth sensor, at the depth fps."""
    depth_sensor = dev.first_depth_sensor()
    dp = next( p for p in depth_sensor.profiles if p.is_default() and p.stream_type() == rs.stream.depth )
    color_sensor = next( ( s for s in dev.query_sensors() if s.is_color_sensor() ), depth_sensor )
    cp = next( ( p for p in color_sensor.profiles if p.is_default() and p.stream_type() == rs.stream.color ), None )
    if not cp:
        return [( depth_sensor, dp )]
    modes = [p for p in color_sensor.profiles if same_mode( p, cp, fps=False )]
    if color_sensor.is_depth_sensor():  # one sensor streams both at a single fps
        modes = [p for p in modes if p.fps() == dp.fps()]
    return [( depth_sensor, dp ), ( color_sensor, min( modes, key=lambda p: p.fps() ) )]


def matching_picks( device, picks ):
    """A profile can't be opened twice; find the same sensor and mode again (on the live or a playback device)."""
    out = []
    for s, p in picks:
        sensor = next( x for x in device.query_sensors() if x.get_info( rs.camera_info.name ) == s.get_info( rs.camera_info.name ) )
        matches = [x for x in sensor.profiles if same_mode( x, p )]
        assert matches, f"{p} not found on {sensor.get_info( rs.camera_info.name )}"
        out.append( ( sensor, matches[0] ) )
    return out


@contextmanager
def streaming( picks, sink ):
    """Open each sensor once with all its profiles (D405 streams depth and color on one sensor)."""
    sensors = {}
    for s, p in picks:
        sensors.setdefault( s.get_info( rs.camera_info.name ), ( s, [] ) )[1].append( p )
    opened, started = [], []
    try:
        for s, profiles in sensors.values():
            s.open( profiles )
            opened.append( s )
            s.start( sink )
            started.append( s )
        yield
    finally:  # undo only what succeeded, so a failed open/start doesn't leave another sensor streaming
        for s in started:
            s.stop()
        for s in opened:
            s.close()


class FrameLog:
    """Frame callback: per-stream arrival times, optionally checking frame-number drops."""
    def __init__( self, check_drops=False, is_d400=False, skip_depth_drops=False ):
        self.arrivals = {}
        self.last_number = {}
        self.check_drops, self.is_d400, self.skip_depth_drops = check_drops, is_d400, skip_depth_drops

    def __call__( self, f ):
        name = f.get_profile().stream_name()
        self.arrivals.setdefault( name, [] ).append( time.perf_counter() )
        if self.check_drops and not ( self.skip_depth_drops and f.get_profile().stream_type() == rs.stream.depth ):
            # D400 depth may reset its frame counter
            frame_utils.check_frame_drops( f, self.last_number.get( name, -1 ), allowed_drops, self.is_d400 )
        self.last_number[name] = f.get_frame_number()

    def fps( self, name ):
        t = self.arrivals.get( name, [] )
        return ( len( t ) - 1 ) / ( t[-1] - t[0] ) if len( t ) > 1 else 0


def wait_for( condition, timeout ):
    t = Timer( timeout )
    t.start()
    while not condition() and not t.has_expired():
        time.sleep( 0.1 )


def toggle_emitter( depth_sensor ):
    """An option re-written while recording used to leave the whole recorded snapshot replaced by that single option."""
    if not depth_sensor.supports( rs.option.emitter_enabled ):
        return None
    emitter = depth_sensor.get_option( rs.option.emitter_enabled )
    try:
        depth_sensor.set_option( rs.option.emitter_enabled, 0 if emitter else 1 )
    except RuntimeError as e:
        # D585S rejects the write (nothing to restore); the playback check still verifies the snapshot
        log.warning( f"could not change an option mid-recording: {e}" )
        return None
    return emitter


@pytest.fixture( scope="module" )
def recording( test_device, tmp_path_factory, request ):
    """Record depth (+ color when available) through the sensor API once per camera."""
    dev, ctx = test_device
    weekly = 'weekly' in request.config.getoption( "--context", default="" ).split()
    seconds = WEEKLY_RECORD_TIME if weekly else RECORD_TIME
    file_name = str( tmp_path_factory.mktemp( "recordings" ) / "rec.db3" )
    picks = pick_profiles( dev )
    log.info( "Recording %s for %s [sec]", [str( p ) for _, p in picks], seconds )

    frames = FrameLog()
    with streaming( picks, frames ):
        wait_for( lambda: frames.arrivals, 5 )
        recorder = rs.recorder( file_name, dev )  # attached to a device that is already streaming
        frames.arrivals.clear()  # measure fps while recording only
        time.sleep( 1 )  # an option written right after attaching replaces the recorded snapshot (SDK race)
        emitter = toggle_emitter( picks[0][0] )
        time.sleep( seconds )
        recorder.pause()
    recorder = None
    if emitter is not None:
        picks[0][0].set_option( rs.option.emitter_enabled, emitter )
    yield dict( file_name=file_name, picks=picks, frames=frames, seconds=seconds )
    if weekly:
        os.remove( file_name )  # weekly files are very large (GBs); no room to keep pytest's run history


@pytest.mark.device_exclude( "D401" )  # failure not yet checked
@pytest.mark.device_exclude( "D555" )  # unstable
@pytest.mark.device_type_exclude( "GMSL" )  # recording slows depth delivery on Jetson
def test_fps_while_recording( recording ):
    for _, p in recording['picks']:
        fps = recording['frames'].fps( p.stream_name() )
        log.info( "%s recorded at %.2f fps", p, fps )
        check.almost_equal( fps, p.fps(), rel=FPS_TOLERANCE, msg=f"{p} recorded at {fps:.2f} fps" )


def test_streaming_after_record_stop( test_device, recording ):
    """Starting a stream after a recording session used to crash the viewer."""
    dev, _ = test_device
    frames = FrameLog()
    with streaming( matching_picks( dev, recording['picks'] ), frames ):
        wait_for( lambda: len( frames.arrivals ) == len( recording['picks'] ), 5 )
    for _, p in recording['picks']:
        check.is_true( frames.arrivals.get( p.stream_name() ), f"no {p.stream_name()} frames after recording stopped" )


@pytest.mark.device_exclude( "D401" )  # failure not yet checked
@pytest.mark.device_exclude( "D405" )  # playback drops its RGB Rectification filter
@pytest.mark.device_exclude( "D555" )  # unstable
def test_playback_of_recorded_profiles( test_device, test_context_var, recording ):
    dev, ctx = test_device
    playback = ctx.load_device( recording['file_name'] )
    picks = matching_picks( playback, recording['picks'] )

    # Not every option reaches the file, so count instead of comparing sets
    check.greater( len( picks[0][0].get_supported_options() ), 1 )
    for ( live, _ ), ( played, _ ) in zip( recording['picks'], picks ):
        live_filters = [f.get_info( rs.camera_info.name ) for f in live.get_recommended_filters()]
        check.is_true( live_filters )
        check.equal( [f.get_info( rs.camera_info.name ) for f in played.get_recommended_filters()], live_filters )

    # On Jetson, capture-side depth drops surface here; skip the depth drop check (RSDEV-7935)
    frames = FrameLog( check_drops=True, is_d400=dev.get_info( rs.camera_info.product_line ) == "D400",
                       skip_depth_drops='jetson' in test_context_var )
    with streaming( picks, frames ):  # play the whole recording, so the drop check covers all of it
        wait_for( lambda: frames.arrivals and playback.current_status() == rs.playback_status.stopped,
                  recording['seconds'] + 10 )
    for _, p in picks:
        check.is_true( frames.arrivals.get( p.stream_name() ), f"no {p.stream_name()} frames in playback" )
    playback = None
    ctx.unload_device( recording['file_name'] )


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


################################################################################################
@pytest.mark.device_exclude( "D401" )  # failure not yet checked
@pytest.mark.device_exclude( "D555" )  # unstable
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
        # On hubless multi-device rigs (e.g. Jetson with D457 + D436) the context sees every
        # connected device; without enable_device(sn) the pipeline picks the first match.
        cfg.enable_device(dev.get_info(rs.camera_info.serial_number))
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
@pytest.mark.device_exclude( "D401" )  # failure not yet checked
@pytest.mark.device_exclude( "D555" )  # unstable
def test_sensor_interface_with_syncer( test_device, tmp_path ):
    log.info( "Trying to record and playback using sensor interface with syncer" )
    dev, ctx = test_device
    file_name = str( tmp_path / "rec.db3" )
    picks = pick_profiles( dev )

    sync = rs.syncer()
    recorder = rs.recorder( file_name, dev )
    with streaming( picks, sync ):
        time.sleep( 3 )
        recorder.pause()
    recorder = None

    playback = ctx.load_device( file_name )
    with streaming( matching_picks( playback, picks ), sync ):
        # if the record-playback worked we will get frames, otherwise the next line will timeout and throw
        sync.wait_for_frames()
    playback = None
    ctx.unload_device( file_name )
