# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

import pytest
import re
import logging
import threading
import pyrealdds as dds
from rspy import config_file
from pytest_check import check
import d435i

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.dds,
]

dds.debug( log.isEnabledFor( logging.DEBUG ) )

participant = None
device_server = None
bridge = None
servers = None
server_list = None
device = None
streams = None
subscriber = None
active_sensors = None
readers_changed = None
last_error = None
_bridge_error_expected = False

def on_start_sensor( sensor_name, profiles ):
    log.debug( f'starting {sensor_name} with {profiles}' )
    check.is_true( sensor_name not in active_sensors )
    active_sensors[sensor_name] = profiles
def on_stop_sensor( sensor_name ):
    log.debug( f'stopping {sensor_name}' )
    check.is_true( sensor_name in active_sensors )
    del active_sensors[sensor_name]

def on_error( error_string ):
    global last_error
    last_error = error_string
    if check.is_true( _bridge_error_expected, msg=f'Unexpected error on bridge: {error_string}' ):
        log.debug( f'error on bridge: {error_string}' )  # not an error because it may be expected...

class bridge_error_expected:
    def __init__( self, expected_msg ):
        self._msg = expected_msg
        global _bridge_error_expected
        _bridge_error_expected = True
    def __enter__( self ):
        global last_error
        last_error = None
    def __exit__( self, type, value, traceback ):
        if type is None:  # If an exception is thrown, don't check anything
            if check.is_true( last_error is not None, msg='Expected an error on bridge but got none' ):
                check.equal( last_error, self._msg )
        global _bridge_error_expected
        _bridge_error_expected = False

def on_readers_changed( server, n_readers ):
    readers_changed.set()

def detect_change():
    readers_changed.clear()

def wait_for_change( timeout = 1 ):  # seconds
    if not readers_changed.wait( timeout ):
        raise TimeoutError( 'timeout waiting for change' )

class change_expected:
    def __enter__( self ):
        detect_change()
    def __exit__( self, type, value, traceback ):
        if type is None:  # If an exception is thrown, don't do anything
            wait_for_change()

@pytest.fixture(scope='module', autouse=True)
def _setup():
    global participant, device_server, bridge, servers, server_list
    global device, streams, subscriber, active_sensors, readers_changed
    participant = dds.participant()
    participant.init( config_file.get_domain_from_config_file_or_default(), "test-stream-sensor-bridge" )

    # set up a server device with a bridge
    device_server = dds.device_server( participant, d435i.device_info.topic_root )
    servers = {}
    for server in d435i.build_streams():
        servers[server.name()] = server
    bridge = dds.stream_sensor_bridge()
    server_list = list( servers.values() )
    bridge.init( server_list )
    device_server.init( server_list, d435i.build_options(), d435i.get_extrinsics() )
    active_sensors = dict()  # sensors that are open; name -> active profiles

    bridge.on_start_sensor( on_start_sensor )
    bridge.on_stop_sensor( on_stop_sensor )
    bridge.on_error( on_error )

    # It can take a while for servers to get the message that a reader is available... we need to wait for it
    readers_changed = threading.Event()
    bridge.on_readers_changed( on_readers_changed )

    # set up the client device and keep all its streams
    device = dds.device( participant, d435i.device_info )
    device.wait_until_ready()  # this will throw if something's wrong
    check.is_true( device.is_ready() )
    streams = {}
    for stream in device.streams():
        streams[stream.name()] = stream
    subscriber = dds.subscriber( participant )

    yield

    streams = None
    subscriber = None
    device = None
    bridge = None
    device_server = None
    server_list = None
    servers = None
    participant = None

def start_stream( stream_name ):
    log.info( f'starting {stream_name}' )
    stream = streams[stream_name]
    topic_name = 'rt/' + d435i.device_info.topic_root + '_' + stream_name
    with change_expected():
        stream.open( topic_name, subscriber )
        #stream.start_streaming( on_frame )

def stop_stream( stream_name ):
    stream = streams[stream_name]
    if stream.is_open():
        log.info( f'stopping {stream_name}' )
        #stream.stop_streaming()  # will throw if not streaming
        with change_expected():
            stream.close()

def reset():
    for stream in streams:
        stop_stream( stream )
    bridge.reset()
    check.equal( len(active_sensors), 0 )

# profile utilities

def find_active_profile( stream_name ):
    for active_profiles in active_sensors.values():
        for profile in active_profiles:
            if profile.stream().name() == stream_name:
                return profile
    raise KeyError( f"can't find profile for stream '{stream_name}'" )

def find_server_profile( stream_name, profile_string ):
    by_string = f"<'{stream_name}' {profile_string}>"
    for profile in servers[stream_name].profiles():
        if profile.to_string() == by_string:
            return profile
    raise KeyError( f"can't find '{stream_name}' profile '{profile_string}'" )

#
#############################################################################################
#
def test_sanity():
    check.equal( len(active_sensors), 0 )
#
#############################################################################################
#
def test_reset_and_commit_nothing_open():
    bridge.reset()
    check.equal( len(active_sensors), 0 )
    bridge.commit()  # nothing to do
    bridge.commit()  # still nothing to do; no error
    bridge.open( servers['Color'].default_profile() )  # RGB sensor wasn't committed, so valid
    bridge.open( servers['Depth'].default_profile() )  # Stereo Module wasn't committed, so valid
    reset()
#
#############################################################################################
#
def test_single_stream_not_streaming():
    bridge.open( servers['Color'].default_profile() )  # 1920x1080 rgb8 @ 30 Hz
    check.equal( len(active_sensors), 0 )
    bridge.commit()
    check.equal( len(active_sensors), 0 )  # nothing streaming; no need to start a sensor
    reset()
#
#############################################################################################
#
def test_single_stream_streaming_default_profile():
    start_stream( 'Color' )
    check.equal( len(active_sensors), 1 )
    stop_stream( 'Color' )
    check.equal( len(active_sensors), 0 )
    reset()
#
#############################################################################################
#
def test_single_stream_explicit():
    bridge.open( find_server_profile( 'Depth', '640x480 16UC1 @ 30 Hz' ))
    with pytest.raises( RuntimeError, match=re.escape( "profile <'Depth' 848x480 16UC1 @ 30 Hz> is incompatible with already-open <'Depth' 640x480 16UC1 @ 30 Hz>" ) ):
        bridge.open( servers['Depth'].default_profile() )  # 848x480 16UC1 @ 30 Hz
    bridge.close( servers['Depth'] )
    bridge.open( servers['Depth'].default_profile() )  # 848x480 16UC1 @ 30 Hz
    bridge.commit()
    check.equal( len(active_sensors), 0 )  # not streaming yet
    start_stream( 'Depth' )
    if (check.equal( len(active_sensors), 1 )
            and check.equal( next(iter(active_sensors)), 'Stereo Module' )
            and check.equal( len(active_sensors['Stereo Module']), 1 )):  # Depth
        check.equal( find_active_profile( 'Depth' ).to_string(), "<'Depth' 848x480 16UC1 @ 30 Hz>" )
    # IR1 and IR2 are not open
    with pytest.raises( RuntimeError, match=re.escape( "sensor 'Stereo Module' was committed and cannot be changed" ) ):
        bridge.open( servers['Infrared_1'].default_profile() )
    reset()
#
#############################################################################################
#
def test_explicit_implicit_streams_all_compatible():
    bridge.open( servers['Depth'].default_profile() )  # 848x480 16UC1 @ 30 Hz
    bridge.add_implicit_profiles()                     # adds IR1, IR2
    with pytest.raises( RuntimeError, match=re.escape( "profile <'Infrared_1' 1280x800 mono8 @ 30 Hz> is incompatible with already-open <'Depth' 848x480 16UC1 @ 30 Hz>" ) ):
        bridge.open( find_server_profile( 'Infrared_1', '1280x800 mono8 @ 30 Hz' ) )
    bridge.open( find_server_profile( 'Infrared_1', '848x480 mono8 @ 30 Hz' ))  # same profile, makes it explicit!
    bridge.commit()
    check.equal( len(active_sensors), 0 )  # not streaming yet
    start_stream( 'Depth' )
    if (check.equal( len(active_sensors), 1 )
            and check.equal( next(iter(active_sensors)), 'Stereo Module' )):
        check.equal( len(active_sensors['Stereo Module']), 3 )
    bridge.open( find_active_profile( 'Infrared_1' ))  # already explicit, same profile: does nothing
    start_stream( 'Infrared_2' )  # starts it implicitly
    reset()
#
#############################################################################################
#
def test_stream_profiles_reset():
    bridge.open( find_server_profile( 'Infrared_1', '640x480 mono8 @ 60 Hz' ) )
    bridge.add_implicit_profiles()                     # adds Depth, IR2
    bridge.commit()
    check.equal( len(active_sensors), 0 )  # not streaming yet
    start_stream( 'Infrared_1' )
    if (check.equal( len(active_sensors), 1 )
            and check.equal( next(iter(active_sensors)), 'Stereo Module' )):
        check.equal( len(active_sensors['Stereo Module']), 3 )
    check.equal( find_active_profile( 'Infrared_2' ).to_string(), "<'Infrared_2' 640x480 mono8 @ 60 Hz>" )
    stop_stream( 'Infrared_1' )
    check.equal( len(active_sensors), 0 )  # not streaming again
    # We don't reset - last commit should still stand!
    start_stream( 'Infrared_2' )
    if (check.equal( len(active_sensors), 1 )
            and check.equal( next(iter(active_sensors)), 'Stereo Module' )):
        check.equal( len(active_sensors['Stereo Module']), 3 )
    check.equal( find_active_profile( 'Infrared_2' ).to_string(), "<'Infrared_2' 640x480 mono8 @ 60 Hz>" )
    stop_stream( 'Infrared_2' )
    check.equal( len(active_sensors), 0 )  # not streaming again
    # Now reset - commit should be lost and we should be back to the default profile
    bridge.reset()
    start_stream( 'Infrared_2' )
    check.equal( find_active_profile( 'Infrared_2' ).to_string(), servers['Infrared_2'].default_profile().to_string() )
    reset()
#
#############################################################################################
#
def test_two_different_sensors():
    bridge.open( servers['Depth'].default_profile() )  # 1280x720 16UC1 @ 30 Hz
    bridge.add_implicit_profiles()                     # adds IR1, IR2
    bridge.commit()
    start_stream( 'Depth' )
    # We have a stream streaming; reset shouldn't touch it
    bridge.reset()
    check.equal( len(active_sensors), 1 )
    # Start another sensor while Depth is streaming
    # NOTE we didn't open any profile so it should pick the default
    start_stream( 'Color' )
    check.equal( len(active_sensors), 2 )
    check.equal( find_active_profile( 'Color' ).to_string(), servers['Color'].default_profile().to_string() )
    reset()
#
#############################################################################################
#
def test_incompatible_profiles_start_stream_failure_but_stream_open():
    bridge.open( find_server_profile( 'Infrared_1', '1280x800 mono8 @ 30 Hz' ) )
    with bridge_error_expected( "failure trying to start/stop 'Depth': profile <'Depth' 848x480 16UC1 @ 30 Hz> is incompatible with already-open <'Infrared_1' 1280x800 mono8 @ 30 Hz>" ):
        start_stream( 'Depth' )
    # Note that while the stream shouldn't be streaming because the _SERVER_ failed, the stream still
    # has the reader open and therefore we still think is streaming...! This requires handling on
    # the client-side (device needs some kind of on-error callback)
    with pytest.raises( RuntimeError, match=re.escape( "stream 'Depth' is already open" ) ):
        start_stream( 'Depth' )
    reset()
#
#############################################################################################
#
def test_motion_module():
    bridge.open( servers['Motion'].default_profile() )  # @ 200 Hz
    start_stream( 'Motion' )
    with pytest.raises( RuntimeError, match=re.escape( "stream 'Motion' is already open" ) ):
        start_stream( 'Motion' )
    reset()
#
#############################################################################################
#
def test_motion_color():
    start_stream( 'Motion' )  # @ 200 Hz
    check.equal( len(active_sensors), 1 )
    start_stream( 'Color' )
    check.equal( len(active_sensors), 2 )
    stop_stream( 'Motion' )
    check.equal( len(active_sensors), 1 )
    stop_stream( 'Color' )
    check.equal( len(active_sensors), 0 )
    reset()
#
#############################################################################################
#
def test_incompatible_streams():
    bridge.open( find_server_profile( 'Infrared_1', '1280x800 mono8 @ 30 Hz' ) )
    bridge.add_implicit_profiles()  # IR2
    check.equal( len(active_sensors), 0 )  # not streaming yet
    with bridge_error_expected( "failure trying to start/stop 'Depth': profile <'Depth' 848x480 16UC1 @ 30 Hz> is incompatible with already-open <'Infrared_1' 1280x800 mono8 @ 30 Hz>" ):
        start_stream( 'Depth' )  # no depth at 1280x800, so no stream!
    check.equal( len(active_sensors), 0 )
    with pytest.raises( RuntimeError, match=re.escape( "profile <'Depth' 848x480 16UC1 @ 30 Hz> is incompatible with already-open <'Infrared_1' 1280x800 mono8 @ 30 Hz>" ) ):
        bridge.open( servers['Depth'].default_profile() )
    with pytest.raises( RuntimeError, match=re.escape( "profile <'Infrared_2' 848x480 mono8 @ 30 Hz> is incompatible with already-open <'Infrared_1' 1280x800 mono8 @ 30 Hz>" ) ):
        bridge.open( find_server_profile( 'Infrared_2', '848x480 mono8 @ 30 Hz' ))
    with pytest.raises( RuntimeError, match=re.escape( "profile <'Infrared_2' 1280x800 mono8 @ 15 Hz> is incompatible with already-open <'Infrared_1' 1280x800 mono8 @ 30 Hz>" ) ):
        bridge.open( find_server_profile( 'Infrared_2', '1280x800 mono8 @ 15 Hz' ))
    start_stream( 'Infrared_2' )
    if (check.equal( len(active_sensors), 1 )
            and check.equal( next(iter(active_sensors)), 'Stereo Module' )):
        check.equal( len(active_sensors['Stereo Module']), 2 )  # IR1, IR2
    with pytest.raises( RuntimeError, match=re.escape( "sensor 'Stereo Module' was committed and cannot be changed" ) ):
        bridge.open( servers['Depth'].default_profile() )
    reset()
#
#############################################################################################
#
def test_open_and_close():
    bridge.open( servers['Infrared_1'].default_profile() )  # 848x480 mono8 @ 30 Hz
    bridge.open( servers['Infrared_1'].default_profile() )  # "compatible"
    bridge.close( servers['Infrared_1'] )
    bridge.open( find_server_profile( 'Infrared_1', '1280x800 mono8 @ 30 Hz' ))
    bridge.close( servers['Infrared_1'] )
    bridge.close( servers['Infrared_1'] )
    bridge.open( find_server_profile( 'Infrared_1', '1280x800 mono16 @ 25 Hz' ))
    bridge.reset()
    bridge.open( find_server_profile( 'Infrared_1', '1280x800 mono16 @ 15 Hz' ))
    with pytest.raises( RuntimeError, match=re.escape( "profile <'Infrared_1' 1280x800 mono16 @ 25 Hz> is incompatible with already-open <'Infrared_1' 1280x800 mono16 @ 15 Hz>" ) ):
        bridge.open( find_server_profile( 'Infrared_1', '1280x800 mono16 @ 25 Hz' ))
    reset()
#
#############################################################################################
