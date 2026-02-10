# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#test:donotrun:!dds

import pyrealdds as dds
from rspy import log, test, config_file
dds.debug( log.is_debug_on() )
from time import sleep

participant = dds.participant()
participant.init( config_file.get_domain_from_config_file(), "test-stream-sensor-bridge" )

# set up a server device with a bridge
import d435i
device_server = dds.device_server( participant, d435i.device_info.topic_root )
servers = {}
for server in d435i.build_streams():
    servers[server.name()] = server
bridge = dds.stream_sensor_bridge()
server_list = list( servers.values() )
bridge.init( server_list )
device_server.init( server_list, d435i.build_options(), d435i.get_extrinsics() )
active_sensors = dict()  # sensors that are open; name -> active profiles

def on_start_sensor( sensor_name, profiles ):
    log.d( f'starting {sensor_name} with {profiles}' )
    global active_sensors
    test.check( sensor_name not in active_sensors )
    active_sensors[sensor_name] = profiles
def on_stop_sensor( sensor_name ):
    log.d( f'stopping {sensor_name}' )
    global active_sensors
    test.check( sensor_name in active_sensors )
    del active_sensors[sensor_name]
bridge.on_start_sensor( on_start_sensor )
bridge.on_stop_sensor( on_stop_sensor )

last_error = None
_bridge_error_expected = False

def on_error( error_string ):
    global last_error
    last_error = error_string
    if test.check( _bridge_error_expected, description = f'Unexpected error on bridge: {error_string}' ):
        log.d( f'error on bridge: {error_string}' )  # not an error because it may be expected...
bridge.on_error( on_error )

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
            if test.check( last_error is not None, description = 'Expected an error on bridge but got none' ):
                test.check_equal( last_error, self._msg )
        global _bridge_error_expected
        _bridge_error_expected = False

# It can take a while for servers to get the message that a reader is available... we need to wait for it
import threading
readers_changed = threading.Event()

def on_readers_changed( server, n_readers ):
    readers_changed.set()

def detect_change():
    readers_changed.clear()

def wait_for_change( timeout = 1 ):  # seconds
    if not readers_changed.wait( timeout ):
        raise TimeoutError( 'timeout waiting for change' )

bridge.on_readers_changed( on_readers_changed )

class change_expected:
    def __enter__( self ):
        detect_change()
    def __exit__( self, type, value, traceback ):
        if type is None:  # If an exception is thrown, don't do anything
            wait_for_change()

# set up the client device and keep all its streams
device = dds.device( participant, d435i.device_info )
device.wait_until_ready()  # this will throw if something's wrong
test.check( device.is_ready() )
streams = {}
for stream in device.streams():
    streams[stream.name()] = stream
subscriber = dds.subscriber( participant )

def start_stream( stream_name ):
    log.i( 'starting', stream_name )
    stream = streams[stream_name]
    topic_name = 'rt/' + d435i.device_info.topic_root + '_' + stream_name
    with change_expected():
        stream.open( topic_name, subscriber )
        #stream.start_streaming( on_frame )

def stop_stream( stream_name ):
    stream = streams[stream_name]
    if stream.is_open():
        log.i( 'stopping', stream_name )
        #stream.stop_streaming()  # will throw if not streaming
        with change_expected():
            stream.close()

def reset():
    for stream in streams:
        stop_stream( stream )
    bridge.reset()
    test.check_equal( len(active_sensors), 0 )


# profile utilities

def find_active_profile( stream_name ):
    for active_profiles in active_sensors.values():
        for profile in active_profiles:
            if profile.stream().name() == stream_name:
                return profile
    raise KeyError( f"can't find '{sensor_name}' profile for stream '{stream_name}'" )

def find_server_profile( stream_name, profile_string ):
    by_string = f"<'{stream_name}' {profile_string}>"    
    for profile in servers[stream_name].profiles():
        if profile.to_string() == by_string:
            return profile
    raise KeyError( f"can't find '{stream_name}' profile '{profile_string}'" )


#############################################################################################
#
with test.closure( "sanity" ):
    test.check_equal( len(active_sensors), 0 )
#
#############################################################################################
#
with test.closure( "reset and commit, nothing open" ):
    bridge.reset()
    test.check_equal( len(active_sensors), 0 )
    bridge.commit()  # nothing to do
    bridge.commit()  # still nothing to do; no error
    bridge.open( servers['Color'].default_profile() )  # RGB sensor wasn't committed, so valid
    bridge.open( servers['Depth'].default_profile() )  # Stereo Module wasn't committed, so valid
reset()
#
#############################################################################################
#
with test.closure( "single stream, not streaming" ):
    bridge.open( servers['Color'].default_profile() )  # 1920x1080 rgb8 @ 30 Hz
    test.check_equal( len(active_sensors), 0 )
    bridge.commit()
    test.check_equal( len(active_sensors), 0 )  # nothing streaming; no need to start a sensor
reset()
#
#############################################################################################
#
with test.closure( "single stream streaming default profile" ):
    start_stream( 'Color' )
    test.check_equal( len(active_sensors), 1 )
    stop_stream( 'Color' )
    test.check_equal( len(active_sensors), 0 )
reset()
#
#############################################################################################
#
with test.closure( "single stream, explicit" ):
    bridge.open( find_server_profile( 'Depth', '640x480 16UC1 @ 30 Hz' )),
    test.check_throws( lambda:
        bridge.open( servers['Depth'].default_profile() ),  # 848x480 16UC1 @ 30 Hz
        RuntimeError, "profile <'Depth' 848x480 16UC1 @ 30 Hz> is incompatible with already-open <'Depth' 640x480 16UC1 @ 30 Hz>" )
    bridge.close( servers['Depth'] )
    bridge.open( servers['Depth'].default_profile() )  # 848x480 16UC1 @ 30 Hz
    bridge.commit()
    test.check_equal( len(active_sensors), 0 )  # not streaming yet
    start_stream( 'Depth' )
    ( test.check_equal( len(active_sensors), 1 )
        and test.check_equal( next(iter(active_sensors)), 'Stereo Module' )
        and test.check_equal( len(active_sensors['Stereo Module']), 1 )  # Depth
        and test.check_equal( find_active_profile( 'Depth' ).to_string(), "<'Depth' 848x480 16UC1 @ 30 Hz>" )
        )
    # IR1 and IR2 are not open
    test.check_throws( lambda:
        bridge.open( servers['Infrared_1'].default_profile() ),
        RuntimeError, "sensor 'Stereo Module' was committed and cannot be changed" )
reset()
#
#############################################################################################
#
with test.closure( "explicit+implicit streams, all compatible" ):
    bridge.open( servers['Depth'].default_profile() )  # 848x480 16UC1 @ 30 Hz
    bridge.add_implicit_profiles()                     # adds IR1, IR2
    test.check_throws( lambda:
        bridge.open( find_server_profile( 'Infrared_1', '1280x800 mono8 @ 30 Hz' ) ),
        RuntimeError, "profile <'Infrared_1' 1280x800 mono8 @ 30 Hz> is incompatible with already-open <'Depth' 848x480 16UC1 @ 30 Hz>" )
    bridge.open( find_server_profile( 'Infrared_1', '848x480 mono8 @ 30 Hz' ))  # same profile, makes it explicit!
    bridge.commit()
    test.check_equal( len(active_sensors), 0 )  # not streaming yet
    start_stream( 'Depth' )
    ( test.check_equal( len(active_sensors), 1 )
        and test.check_equal( next(iter(active_sensors)), 'Stereo Module' )
        and test.check_equal( len(active_sensors['Stereo Module']), 3 )
        )
    bridge.open( find_active_profile( 'Infrared_1' ))  # already explicit, same profile: does nothing
    start_stream( 'Infrared_2' )  # starts it implicitly
reset()
#
#############################################################################################
#
with test.closure( "stream profiles reset" ):
    bridge.open( find_server_profile( 'Infrared_1', '640x480 mono8 @ 60 Hz' ) )
    bridge.add_implicit_profiles()                     # adds Depth, IR2
    bridge.commit()
    test.check_equal( len(active_sensors), 0 )  # not streaming yet
    start_stream( 'Infrared_1' )
    ( test.check_equal( len(active_sensors), 1 )
        and test.check_equal( next(iter(active_sensors)), 'Stereo Module' )
        and test.check_equal( len(active_sensors['Stereo Module']), 3 )
        )
    test.check_equal( find_active_profile( 'Infrared_2' ).to_string(), "<'Infrared_2' 640x480 mono8 @ 60 Hz>" )
    stop_stream( 'Infrared_1' )
    test.check_equal( len(active_sensors), 0 )  # not streaming again
    # We don't reset - last commit should still stand!
    start_stream( 'Infrared_2' )
    ( test.check_equal( len(active_sensors), 1 )
        and test.check_equal( next(iter(active_sensors)), 'Stereo Module' )
        and test.check_equal( len(active_sensors['Stereo Module']), 3 )
        )
    test.check_equal( find_active_profile( 'Infrared_2' ).to_string(), "<'Infrared_2' 640x480 mono8 @ 60 Hz>" )
    stop_stream( 'Infrared_2' )
    test.check_equal( len(active_sensors), 0 )  # not streaming again
    # Now reset - commit should be lost and we should be back to the default profile
    bridge.reset()
    start_stream( 'Infrared_2' )
    test.check_equal( find_active_profile( 'Infrared_2' ).to_string(), servers['Infrared_2'].default_profile().to_string() )
reset()
#
#############################################################################################
#
with test.closure( "two different sensors" ):
    bridge.open( servers['Depth'].default_profile() )  # 1280x720 16UC1 @ 30 Hz
    bridge.add_implicit_profiles()                     # adds IR1, IR2
    bridge.commit()
    start_stream( 'Depth' )
    # We have a stream streaming; reset shouldn't touch it
    bridge.reset()
    test.check_equal( len(active_sensors), 1 )
    # Start another sensor while Depth is streaming
    # NOTE we didn't open any profile so it should pick the default
    start_stream( 'Color' )
    test.check_equal( len(active_sensors), 2 )
    test.check_equal( find_active_profile( 'Color' ).to_string(), servers['Color'].default_profile().to_string() )
reset()
#
#############################################################################################
#
with test.closure( "incompatible profiles; start_stream failure but stream open" ):
    bridge.open( find_server_profile( 'Infrared_1', '1280x800 mono8 @ 30 Hz' ) )
    with bridge_error_expected( "failure trying to start/stop 'Depth': profile <'Depth' 848x480 16UC1 @ 30 Hz> is incompatible with already-open <'Infrared_1' 1280x800 mono8 @ 30 Hz>" ):
        start_stream( 'Depth' )
    # Note that while the stream shouldn't be streaming because the _SERVER_ failed, the stream still
    # has the reader open and therefore we still think is streaming...! This requires handling on
    # the client-side (device needs some kind of on-error callback)
    test.check_throws( lambda:
        start_stream( 'Depth' ),
        RuntimeError, "stream 'Depth' is already open" )
reset()
#
#############################################################################################
#
with test.closure( "motion module" ):
    bridge.open( servers['Motion'].default_profile() )  # @ 200 Hz
    start_stream( 'Motion' )
    test.check_throws( lambda:
        start_stream( 'Motion' ),
        RuntimeError, "stream 'Motion' is already open" )
reset()
#
#############################################################################################
#
with test.closure( "motion + color" ):
    start_stream( 'Motion' )  # @ 200 Hz
    test.check_equal( len(active_sensors), 1 )
    start_stream( 'Color' )
    test.check_equal( len(active_sensors), 2 )
    stop_stream( 'Motion' )
    test.check_equal( len(active_sensors), 1 )
    stop_stream( 'Color' )
    test.check_equal( len(active_sensors), 0 )
reset()
#
#############################################################################################
#
with test.closure( "incompatible streams" ):
    bridge.open( find_server_profile( 'Infrared_1', '1280x800 mono8 @ 30 Hz' ) )
    bridge.add_implicit_profiles()  # IR2
    test.check_equal( len(active_sensors), 0 )  # not streaming yet
    with bridge_error_expected( "failure trying to start/stop 'Depth': profile <'Depth' 848x480 16UC1 @ 30 Hz> is incompatible with already-open <'Infrared_1' 1280x800 mono8 @ 30 Hz>" ):
        start_stream( 'Depth' )  # no depth at 1280x800, so no stream!
    test.check_equal( len(active_sensors), 0 )
    test.check_throws( lambda:
        bridge.open( servers['Depth'].default_profile() ),
        RuntimeError, "profile <'Depth' 848x480 16UC1 @ 30 Hz> is incompatible with already-open <'Infrared_1' 1280x800 mono8 @ 30 Hz>" )
    test.check_throws( lambda:
        bridge.open( find_server_profile( 'Infrared_2', '848x480 mono8 @ 30 Hz' )),
        RuntimeError, "profile <'Infrared_2' 848x480 mono8 @ 30 Hz> is incompatible with already-open <'Infrared_1' 1280x800 mono8 @ 30 Hz>" )
    test.check_throws( lambda:
        bridge.open( find_server_profile( 'Infrared_2', '1280x800 mono8 @ 15 Hz' )),
        RuntimeError, "profile <'Infrared_2' 1280x800 mono8 @ 15 Hz> is incompatible with already-open <'Infrared_1' 1280x800 mono8 @ 30 Hz>" )
    start_stream( 'Infrared_2' )
    ( test.check_equal( len(active_sensors), 1 )
        and test.check_equal( next(iter(active_sensors)), 'Stereo Module' )
        and test.check_equal( len(active_sensors['Stereo Module']), 2 )  # IR1, IR2
        )
    test.check_throws( lambda:
        bridge.open( servers['Depth'].default_profile() ),
        RuntimeError, "sensor 'Stereo Module' was committed and cannot be changed" )
reset()
#
#############################################################################################
#
with test.closure( "open and close" ):
    bridge.open( servers['Infrared_1'].default_profile() )  # 848x480 mono8 @ 30 Hz
    bridge.open( servers['Infrared_1'].default_profile() )  # "compatible"
    bridge.close( servers['Infrared_1'] )
    bridge.open( find_server_profile( 'Infrared_1', '1280x800 mono8 @ 30 Hz' ))
    bridge.close( servers['Infrared_1'] )
    bridge.close( servers['Infrared_1'] )
    bridge.open( find_server_profile( 'Infrared_1', '1280x800 mono16 @ 25 Hz' ))
    bridge.reset()
    bridge.open( find_server_profile( 'Infrared_1', '1280x800 mono16 @ 15 Hz' ))
    test.check_throws( lambda:
        bridge.open( find_server_profile( 'Infrared_1', '1280x800 mono16 @ 25 Hz' )),
        RuntimeError, "profile <'Infrared_1' 1280x800 mono16 @ 25 Hz> is incompatible with already-open <'Infrared_1' 1280x800 mono16 @ 15 Hz>" )
reset()
#
#############################################################################################
test.print_results_and_exit()
