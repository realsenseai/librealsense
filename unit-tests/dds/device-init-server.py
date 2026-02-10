# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

import pyrealdds as dds
from rspy import log, test, config_file
import d435i
import threading
from time import sleep

dds.debug( log.is_debug_on(), log.nested )


participant = dds.participant()
# in some cases we need a bigger buffer for our messages, to avoid missing messages sent
settings = { "device" :
                 { "notification" :
                       { "history" :
                             { "depth" : 60 }
                         }
                   }
             }
participant.init( config_file.get_domain_from_config_file(), "device-init-server", settings )


def test_one_stream():
    s1p1 = dds.video_stream_profile( 9, dds.video_encoding.rgb, 10, 10 )
    s1profiles = [s1p1]
    s1 = dds.depth_stream_server( "s1", "sensor" )
    s1.init_profiles( s1profiles, 0 )
    log.d( s1.profiles() )
    global server
    server = dds.device_server( participant, "realdds/device/topic-root" )
    server.init( [s1], [], {} )

def test_one_motion_stream():
    s1p1 = dds.motion_stream_profile( 30 )
    s1profiles = [s1p1]
    s1 = dds.motion_stream_server( "s2", "sensor2" )
    s1.init_profiles( s1profiles, 0 )
    log.d( s1.profiles() )
    global server
    server = dds.device_server( participant, "realdds/device/topic-root" )
    server.init( [s1], [], {} )

def test_no_profiles():
    s1profiles = []
    s1 = dds.color_stream_server( "s1", "sensor" )
    s1.init_profiles( s1profiles, 0 )
    global server
    server = dds.device_server( participant, "realdds/device/topic-root" )
    server.init( [s1], [], {} )

def test_no_streams():
    global server
    server = dds.device_server( participant, "realdds/device/topic-root" )
    server.init( [], [], {} )

def test_n_profiles( n_profiles ):
    assert n_profiles > 0
    s1profiles = []
    fibo = [0,1]
    for i in range(n_profiles):
        width = i
        height = fibo[-2] + fibo[-1]
        fibo[-2] = fibo[-1]
        fibo[-1] = height
        s1profiles += [dds.video_stream_profile( 9, dds.video_encoding.rgb, width, height )]
    s1 = dds.ir_stream_server( "s1", "sensor" )
    s1.init_profiles( s1profiles, 0 )
    log.d( s1.profiles() )
    global server
    server = dds.device_server( participant, "realdds/device/topic-root" )
    server.init( [s1], [], {} )


def test_d435i():
    global server
    server = d435i.build( participant )


notification_thread = None
def notification_flood():
    def notification_flooder():
        global server
        i = 0
        while server is not None:
            i += 1
            #log.d( f'----> {i}' )
            try:
                server.publish_notification( { 'id' : 'some-notification', 'counter' : i } )
            except AttributeError as e:
                # another thread had just made the server None at this point
                if not server:
                    log.d("tried calling publish_notification but server is None, was close_server called?")
                else:
                    raise e # unexpected error
            sleep( 0.005 )
    global notification_thread
    notification_thread = threading.Thread( target=notification_flooder )
    notification_thread.start()


def close_server():
    global server, notification_thread
    server = None
    if notification_thread is not None:
        notification_thread.join()
        notification_thread = None



# From here down, we're in "interactive" mode (see test-device-init.py)
# ...
