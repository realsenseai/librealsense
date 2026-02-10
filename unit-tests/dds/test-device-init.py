# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#test:donotrun:!dds
#test:retries 2

import pyrealdds as dds
from rspy import log, test, config
import d435i
from time import sleep


dds.debug( log.is_debug_on(), 'C  ' )
log.nested = 'C  '


participant = dds.participant()
participant.init( config.get_domain_from_config_file(), "test-device-init" )

info = dds.message.device_info()
info.name = "Test Device"
info.topic_root = "realdds/device/topic-root"


import os.path
cwd = os.path.dirname(os.path.realpath(__file__))
remote_script = os.path.join( cwd, 'device-init-server.py' )
second_client_script = os.path.join( cwd, 'device-2nd-client.py' )
with test.remote( remote_script, nested_indent="  S" ) as remote:
    remote.wait_until_ready()
    #
    #############################################################################################
    #
    with test.closure( "Test 1 stream..." ):
        remote.run( 'test_one_stream()' )
        device = dds.device( participant, info )
        device.wait_until_ready()  # If no device is available before timeout, this will throw
        test.check( device.is_ready() )
        test.check_equal( device.n_streams(), 1 )
        for stream in device.streams():
            profiles = stream.profiles()
            test.check_equal( stream.name(), "s1" )
            test.check_equal( stream.sensor_name(), "sensor" )
            test.check_equal( len( profiles ), 1 )
            test.check_equal( str(profiles[0]), "<pyrealdds.depth_stream_profile 's1' 10x10 rgb8 @ 9 Hz>" )
            test.check_equal( profiles[0].stream(), stream )
            test.check_equal( stream.default_profile_index(), 0 )
        remote.run( 'close_server()' )
    device = None
    #
    #############################################################################################
    #
    with test.closure( "Test motion stream..." ):
        remote.run( 'test_one_motion_stream()' )
        device = dds.device( participant, info )
        device.wait_until_ready()  # If no device is available before timeout, this will throw
        test.check( device.is_ready() )
        test.check_equal( device.n_streams(), 1 )
        for stream in device.streams():
            profiles = stream.profiles()
            test.check_equal( stream.name(), "s2" )
            test.check_equal( stream.sensor_name(), "sensor2" )
            test.check_equal( stream.type_string(), "motion" )
            test.check_equal( len( profiles ), 1 )
            test.check_equal( str(profiles[0]), "<pyrealdds.motion_stream_profile 's2' @ 30 Hz>" )
            test.check_equal( profiles[0].stream(), stream )
            test.check_equal( stream.default_profile_index(), 0 )
        remote.run( 'close_server()' )
    device = None
    #
    #############################################################################################
    #
    with test.closure( "Test no streams..." ):
        remote.run( 'test_no_streams()' )
        device = dds.device( participant, info )
        device.wait_until_ready()  # If no device is available before timeout, this will throw
        test.check( device.is_ready() )
        test.check_equal( device.n_streams(), 0 )
        for stream in device.streams():
            test.unreachable()
        remote.run( 'close_server()' )
    device = None
    #
    #############################################################################################
    #
    with test.closure( "Test no profiles... (should fail on server side)" ):
        try:
            remote.run( 'test_no_profiles()' )
        except test.remote.Error as e:
            # this fails because streams require at least one profile
            test.check_exception( e, test.remote.Error, "[remote] RuntimeError: at least one profile is required to initialize stream 's1'" )
    device = None
    #
    #############################################################################################
    #
    with test.closure( "Test 10 profiles..." ):
        remote.run( 'test_n_profiles(10)' )
        device = dds.device( participant, info )
        device.wait_until_ready()  # If no device is available before timeout, this will throw
        test.check( device.is_ready() )
        test.check_equal( device.n_streams(), 1 )
        for stream in device.streams():
            profiles = stream.profiles()
        test.check_equal( 10, len( profiles ))
        # Go thru the profiles, make sure they're in the right order (json is unordered)
        fibo = [0,1]
        for i in range(10):
            v = fibo[-2] + fibo[-1]
            fibo[-2] = fibo[-1]
            fibo[-1] = v
            test.check_equal( profiles[i].width(), i )
            test.check_equal( profiles[i].height(), v )
        remote.run( 'close_server()' )
    device = None
    #
    #############################################################################################
    #
    with test.closure( "Test D435i..." ):
        remote.run( 'test_d435i()' )
        device = dds.device( participant, d435i.device_info )
        device.wait_until_ready()  # If no device is available before timeout, this will throw
        test.check( device.is_ready() )
        test.check_equal( device.n_streams(), 5 )
        for stream in device.streams():
            profiles = stream.profiles()
        remote.run( 'close_server()' )
    device = None
    #
    #############################################################################################
    #
    with test.closure( "Test discovery of another client device..." ):
        remote.run( 'test_one_stream()' )
        device = dds.device( participant, info )
        device.wait_until_ready()  # If no device is available before timeout, this will throw
        test.check( device.is_ready() )
        test.check_equal( device.n_streams(), 1 )
        # now start another server, and have it access the device
        # NOTE: I had trouble nesting another 'with' statement here...
        with test.remote( second_client_script, name="remote2", nested_indent=" 2 " ) as remote2:
            remote2.wait_until_ready()
            remote2.run( 'test_second_device()' )
            # the notifications for the second client should be sent again from the server -- we
            # should ignore them...
            remote2.run( 'close_device()' )
        remote.run( 'close_server()' )
    device = None
    #
    #############################################################################################
    #
    with test.closure( "Multiple overlapping client initializations" ):
        remote.run( 'test_one_stream()' )
        remote.run( 'notification_flood()' )
        # Start another two clients and have them access the device:
        # When the device initializations happen concurrently (which is hard to guarrantee) then
        # initialization messages may get confused and an exception produced. We try 5 times:
        for i in range(5):
            if i > 0:
                # if we get here, there was no exception; we try again with a small pause
                log.i( f'... iteration {i+1}' )
                sleep( 1 )
            with test.remote( second_client_script, name="client1", nested_indent=" 1 " ) as client1:
                client1.wait_until_ready()
                with test.remote( second_client_script, name="client2", nested_indent=" 2 " ) as client2:
                    client2.wait_until_ready()
                    with test.remote( second_client_script, name="client3", nested_indent=" 3 " ) as client3:
                        client3.wait_until_ready()
                        with test.remote( second_client_script, name="client4", nested_indent=" 4 " ) as client4:
                            client4.wait_until_ready()
                            client1.run( 'test_second_device()', timeout=None )  # Don't wait until done
                            client2.run( 'test_second_device()', timeout=None )
                            client3.run( 'test_second_device()', timeout=None )
                            client4.run( 'test_second_device()', timeout=None )
        remote.run( 'close_server()' )

    #
    #############################################################################################


participant = None
test.print_results_and_exit()
