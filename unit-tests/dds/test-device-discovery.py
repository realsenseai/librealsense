# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#test:donotrun:!dds
#test:retries 2

from rspy import log, test
import pyrealdds as dds
from time import sleep
import re

with test.remote.fork( nested_indent='  S' ) as remote:
    if remote is None:  # we're the server fork
        dds.debug( log.is_debug_on(), log.nested )

        participant = dds.participant()
        participant.init( 123, 'server' )

        def create_device_info( props ):
            global broadcasters, publisher
            serial = props.setdefault( 'serial', str( participant.create_guid() ) )
            props.setdefault( 'name', f'device{serial}' )
            props.setdefault( 'topic-root', f'device{serial}' )
            di = dds.message.device_info.from_json( props )
            return di

        def create_server( root ):
            s1p1 = dds.video_stream_profile( 9, dds.video_encoding.rgb, 10, 10 )
            s1profiles = [s1p1]
            s1 = dds.color_stream_server( 's1', 'sensor' )
            s1.init_profiles( s1profiles, 0 )
            s1.init_options( [
                dds.option.from_json( ['Backlight Compensation', 0, 0, 1, 1, 0, 'Backlight custom description'] ),
                dds.option.from_json( ['Custom Option', 5., -10, 10, 1, -5., 'Description'] )
                ] )
            server = dds.device_server( participant, root )
            server.init( [s1], [], {} )
            return server

        def create_server_2( root ):
            s1p1 = dds.video_stream_profile( 3, dds.video_encoding.z16, 100, 100 )
            s1profiles = [s1p1]
            s1 = dds.color_stream_server( 's2', 'sensor2' )
            s1.init_profiles( s1profiles, 0 )
            s1.init_options( [
                dds.option.from_json( ['Another Option', 7., 5, 15, 2, 7., 'Another Option'] )
                ] )
            server = dds.device_server( participant, root )
            server.init( [s1], [], {} )
            return server

        # From here down, we're in "interactive" mode (see test-watcher.py)
        # ...
        raise StopIteration()  # quit the remote



    dds.debug( log.is_debug_on(), 'C  ' )
    log.nested = 'C  '

    import threading
    from rspy.stopwatch import Stopwatch


    participant = dds.participant()
    participant.init( 123, "client" )


    # We listen directly on the device-info topic
    device_info_topic = dds.message.device_info.create_topic( participant, dds.topics.device_info )
    device_info_reader = dds.topic_reader( device_info_topic )
    broadcast_received = threading.Event()
    broadcast_devices = []
    def on_device_info_available( reader ):
        while True:
            msg = dds.message.flexible.take_next( reader )
            if not msg:
                break
            j = msg.json_data()
            log.d( f'on_device_info_available {j}' )
            global broadcast_devices
            broadcast_devices.append( j )
        broadcast_received.set()
    device_info_reader.on_data_available( on_device_info_available )
    device_info_reader.run( dds.topic_reader.qos() )

    def detect_broadcast():
        global broadcast_received, broadcast_devices
        broadcast_received.clear()
        broadcast_devices = []

    def wait_for_broadcast( count=1, timeout=1 ):
        while timeout > 0:
            sw = Stopwatch()
            if not broadcast_received.wait( timeout ):
                raise TimeoutError( 'timeout waiting for broadcast' )
            if count <= len(broadcast_devices):
                return
            broadcast_received.clear()
            timeout -= sw.get_elapsed()
        if count is None:
            raise TimeoutError( 'timeout waiting broadcast' )
        raise TimeoutError( f'timeout waiting for {count} broadcasts; {len(broadcast_devices)} received' )

    class broadcast_expected:
        def __init__( self, n_expected=1, timeout=1 ):
            self._timeout = timeout
            self._n_expected = n_expected
        def __enter__( self ):
            detect_broadcast()
        def __exit__( self, type, value, traceback ):
            if type is None:  # If an exception is thrown, don't do anything
                wait_for_broadcast( count=self._n_expected, timeout=self._timeout )


    # Start a watcher, too...
    change_received = threading.Event()
    devices_added = 0
    devices_removed = 0
    devices = dict()

    def on_device_added( watcher, dev ):
        global devices_added, devices
        devices_added += 1
        log.d( '+++-> device added', dev )
        devices[dev.device_info().topic_root] = dev
        change_received.set()
        test.check( dev.is_online() )

    def on_device_removed( watcher, dev ):
        global devices_removed, devices
        devices_removed += 1
        log.d( '<---- device removed', dev )
        del devices[dev.device_info().topic_root]
        change_received.set()

    def detect_change():
        global devices_added, devices_removed
        change_received.clear()
        devices_added = 0
        devices_removed = 0

    def wait_for_change( n_added=0, n_removed=0, timeout=3 ):
        global devices_added, devices_removed
        while timeout > 0:
            sw = Stopwatch()
            if not change_received.wait( timeout ):
                raise TimeoutError( 'timeout waiting for add/remove' )
            change_received.clear()
            if n_added <= devices_added and n_removed <= devices_removed:
                return
            timeout -= sw.get_elapsed()
        raise TimeoutError( f'timeout waiting for {count} add/removes; {n_changes} received' )

    class change_expected:
        def __init__( self, n_added=0, n_removed=0, timeout=3 ):
            self._timeout = timeout
            self._n_added = n_added
            self._n_removed = n_removed
        def __enter__( self ):
            detect_change()
        def __exit__( self, type, value, traceback ):
            if type is None:  # If an exception is thrown, don't do anything
                wait_for_change( n_added=self._n_added, n_removed=self._n_removed, timeout=self._timeout )
                global devices_added, devices_removed
                test.check_equal( devices_added, self._n_added )
                test.check_equal( devices_removed, self._n_removed )


    watcher = dds.device_watcher( participant )
    watcher.on_device_added( on_device_added )
    watcher.on_device_removed( on_device_removed )
    watcher.start()


    #############################################################################################
    with test.closure( "Broadcast one device" ):
        with change_expected( n_added=1 ):
            remote.run( 'di1 = create_device_info({ "serial" : "123" })' )
            remote.run( 'd1 = create_server( di1.topic_root )' )
            remote.run( 'd1.broadcast( di1 )' )
        test.check_equal( len(broadcast_devices), 1 )
        test.check_equal( len(devices), 1 )
        d1 = devices[f'device123']  # remember it -- we'll re-add it later and want to test it's the same!
        d1guid = d1.guid()

    #############################################################################################
    with test.closure( "Broadcast second device" ):
        with change_expected( n_added=1 ):
            remote.run( 'di2 = create_device_info({ "serial" : "456" })' )
            remote.run( 'd2 = create_server( di2.topic_root )' )
            remote.run( 'd2.broadcast( di2 )' )
        test.check_equal( len(broadcast_devices), 3 )  # each broadcast is of ALL the devices
        test.check_equal( len(devices), 2 )
        d2 = devices[f'device456']  # remember it -- we'll re-add it later and want to test it's the same!
        d2.wait_until_ready()
        d2option = d2.streams()[0].options()[0]
        d2.query_option_value( d2option )

    #############################################################################################
    with test.closure( "Add another client; expect rebroadcast of all" ):
        with broadcast_expected( 2 ):
            reader_2 = dds.topic_reader( device_info_topic )
            reader_2.run( dds.topic_reader.qos() )
        test.check_equal( len(broadcast_devices), 2 )
        # Add short sleep to avoid a possible deadlock. Devices broadcast is handled by `on_device_info_available` callback,
        # we may still be checking for messages (in eProcima reader thread) when trying to delete reader_2.
        sleep( 0.1 )
        del reader_2

    #############################################################################################
    with test.closure( "We should see both in the watcher" ):
        test.check_equal( len(devices), 2 )
        for dev in devices.values():
            test.info( 'device', dev )
            test.check( watcher.is_device_broadcast( dev ) )

    #############################################################################################
    with test.closure( "Disconnect one & remove the other" ):
        with change_expected( n_removed=2 ):
            remote.run( 'd1.broadcast_disconnect( dds.time( 2. ) )' )
            remote.run( 'del d2' )
        test.check_equal( len(watcher.devices()), 0 )

    #############################################################################################
    with test.closure( "Both should go offline & not ready" ):
        test.check_false( watcher.is_device_broadcast( d1 ) )
        test.check_false( d1.is_online() )
        test.check_false( d1.is_ready() )
        test.check_false( watcher.is_device_broadcast( d2 ) )
        test.check_false( d2.is_online() )
        test.check_false( d2.is_ready() )

    #############################################################################################
    with test.closure( "Offline device doesn't have streams or options" ):
        test.check( len( d1.streams() ) == 0 )
        test.check( len( d1.options() ) == 0 )

    #############################################################################################
    with test.closure( "Unbroadcast server still sends out init messages" ):
        info = dds.message.device_info()
        info.name = 'Test Device'
        info.topic_root = 'device123'
        dds.device( participant, info ).wait_until_ready()  # New subscriber to notifications will trigger new handshake

    #############################################################################################
    with test.closure( "Offline device should not get ready (ready means online)" ):
        test.check_false( d1.is_ready() )
        test.check_false( d1.is_online() )

    #############################################################################################
    with test.closure( "Rebroadcast the disconnected device" ):
        with change_expected( n_added=1 ):
            remote.run( 'd1.broadcast( di1 )' )
        test.check( watcher.is_device_broadcast( d1 ) )
        test.check( d1.is_online() )

    #############################################################################################
    with test.closure( "It needs to reinitialize to get ready again" ):
        d1.wait_until_ready()  # NOTE: requires server to resend init messages on broadcast
        test.check( d1.is_ready() )
        test.check_equal( len(devices), 1 )
        test.check_equal( devices['device123'].guid(), d1guid )  # Same device
        d1.query_option_value( d1.streams()[0].options()[0] )

    #############################################################################################
    with test.closure( "Recreate device456, new content, without a broadcast" ):
        detect_broadcast()
        detect_change()
        remote.run( 'd2 = create_server_2( di2.topic_root )' )
        sleep( 2 );

    #############################################################################################
    with test.closure( "It should not get ready (because it's not online)" ):
        test.check_equal( len(broadcast_devices), 0 )
        test.check_equal( devices_added, 0 )
        test.check_false( d2.is_online() )

    #############################################################################################
    with test.closure( "Broadcast it again; it should come online" ):
        with change_expected( n_added=1 ):
            remote.run( 'd2.broadcast( di2 )' )
        test.check( d2.is_online() )
        test.check( watcher.is_device_broadcast( d2 ) )

    #############################################################################################
    with test.closure( "It should get ready, too" ):
        d2.wait_until_ready()
        test.check( d2.is_ready() )

    #############################################################################################
    with test.closure( "Check new content" ):
        test.check_throws( lambda:
            d2.query_option_value( d2option ),
            RuntimeError, r'''["query-option"] device option 'Backlight Compensation' not found''' )
        if test.check_equal( len(d2.streams()), 1 ):
            stream = d2.streams()[0]
            test.check_equal( stream.name(), 's2' )
            options = stream.options()
            if test.check_equal( len(options), 1 ):
                d2.query_option_value( options[0] )


    del watcher
    device_info_reader.stop()
    del device_info_reader
    del participant
    test.print_results_and_exit()
