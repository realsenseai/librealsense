# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import logging
import threading
import re
from time import sleep
import pyrealdds as dds
from rspy import test, config_file
import rspy.log
from rspy.stopwatch import Stopwatch
from pytest_check import check

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.dds,
    pytest.mark.flaky( retries=2 ),
]

if rspy.log.nested is not None:
    dds.debug( log.isEnabledFor( logging.DEBUG ), rspy.log.nested )

    participant = dds.participant()
    participant.init( config_file.get_domain_from_config_file_or_default(), 'server' )

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

else:
    log.nested = 'C  '
    dds.debug( log.isEnabledFor( logging.DEBUG ), 'C  ' )

    @pytest.fixture(scope='module')
    def remote_server():
        with test.remote.fork( script=__file__, nested_indent='  S' ) as remote:
            yield remote

    def test_device_discovery(remote_server):
        participant = dds.participant()
        participant.init( config_file.get_domain_from_config_file_or_default(), "client" )

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
                log.debug( f'on_device_info_available {j}' )
                nonlocal broadcast_devices
                broadcast_devices.append( j )
            broadcast_received.set()
        device_info_reader.on_data_available( on_device_info_available )
        device_info_reader.run( dds.topic_reader.qos() )

        def detect_broadcast():
            nonlocal broadcast_received, broadcast_devices
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
            nonlocal devices_added, devices
            devices_added += 1
            log.debug( f'+++-> device added {dev}' )
            devices[dev.device_info().topic_root] = dev
            change_received.set()
            check.is_true( dev.is_online() )

        def on_device_removed( watcher, dev ):
            nonlocal devices_removed, devices
            devices_removed += 1
            log.debug( f'<---- device removed {dev}' )
            del devices[dev.device_info().topic_root]
            change_received.set()

        def detect_change():
            change_received.clear()
            nonlocal devices_added, devices_removed
            devices_added = 0
            devices_removed = 0

        def wait_for_change( n_added=0, n_removed=0, timeout=3 ):
            nonlocal devices_added, devices_removed
            while timeout > 0:
                sw = Stopwatch()
                if not change_received.wait( timeout ):
                    raise TimeoutError( 'timeout waiting for add/remove' )
                change_received.clear()
                if n_added <= devices_added and n_removed <= devices_removed:
                    return
                timeout -= sw.get_elapsed()
            raise TimeoutError( f'timeout waiting for n_added={n_added} n_removed={n_removed}; got devices_added={devices_added} devices_removed={devices_removed}' )

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
                    nonlocal devices_added, devices_removed
                    check.equal( devices_added, self._n_added )
                    check.equal( devices_removed, self._n_removed )

        watcher = dds.device_watcher( participant )
        watcher.on_device_added( on_device_added )
        watcher.on_device_removed( on_device_removed )
        watcher.start()

        #############################################################################################
        # Broadcast one device
        with change_expected( n_added=1 ):
            remote_server.run( 'di1 = create_device_info({ "serial" : "123" })' )
            remote_server.run( 'd1 = create_server( di1.topic_root )' )
            remote_server.run( 'd1.broadcast( di1 )' )
        check.equal( len(broadcast_devices), 1 )
        check.equal( len(devices), 1 )
        d1 = devices[f'device123']  # remember it -- we'll re-add it later and want to test it's the same!
        d1guid = d1.guid()

        #############################################################################################
        # Broadcast second device
        with change_expected( n_added=1 ):
            remote_server.run( 'di2 = create_device_info({ "serial" : "456" })' )
            remote_server.run( 'd2 = create_server( di2.topic_root )' )
            remote_server.run( 'd2.broadcast( di2 )' )
        check.equal( len(broadcast_devices), 3 )  # each broadcast is of ALL the devices
        check.equal( len(devices), 2 )
        d2 = devices[f'device456']  # remember it -- we'll re-add it later and want to test it's the same!
        d2.wait_until_ready()
        d2option = d2.streams()[0].options()[0]
        d2.query_option_value( d2option )

        #############################################################################################
        # Add another client; expect rebroadcast of all
        with broadcast_expected( 2 ):
            reader_2 = dds.topic_reader( device_info_topic )
            reader_2.run( dds.topic_reader.qos() )
        check.equal( len(broadcast_devices), 2 )
        # Add short sleep to avoid a possible deadlock. Devices broadcast is handled by `on_device_info_available` callback,
        # we may still be checking for messages (in eProcima reader thread) when trying to delete reader_2.
        sleep( 0.1 )
        del reader_2

        #############################################################################################
        # We should see both in the watcher
        check.equal( len(devices), 2 )
        for dev in devices.values():
            log.info( f'device {dev}' )
            check.is_true( watcher.is_device_broadcast( dev ) )

        #############################################################################################
        # Disconnect one & remove the other
        with change_expected( n_removed=2 ):
            remote_server.run( 'd1.broadcast_disconnect( dds.time( 2. ) )' )
            remote_server.run( 'del d2' )
        check.equal( len(watcher.devices()), 0 )

        #############################################################################################
        # Both should go offline & not ready
        check.is_false( watcher.is_device_broadcast( d1 ) )
        check.is_false( d1.is_online() )
        check.is_false( d1.is_ready() )
        check.is_false( watcher.is_device_broadcast( d2 ) )
        check.is_false( d2.is_online() )
        check.is_false( d2.is_ready() )

        #############################################################################################
        # Offline device doesn't have streams or options
        check.equal( len( d1.streams() ), 0 )
        check.equal( len( d1.options() ), 0 )

        #############################################################################################
        # Unbroadcast server still sends out init messages
        info = dds.message.device_info()
        info.name = 'Test Device'
        info.topic_root = 'device123'
        dds.device( participant, info ).wait_until_ready()  # New subscriber to notifications will trigger new handshake

        #############################################################################################
        # Offline device should not get ready (ready means online)
        check.is_false( d1.is_ready() )
        check.is_false( d1.is_online() )

        #############################################################################################
        # Rebroadcast the disconnected device
        with change_expected( n_added=1 ):
            remote_server.run( 'd1.broadcast( di1 )' )
        check.is_true( watcher.is_device_broadcast( d1 ) )
        check.is_true( d1.is_online() )

        #############################################################################################
        # It needs to reinitialize to get ready again
        d1.wait_until_ready()  # NOTE: requires server to resend init messages on broadcast
        check.is_true( d1.is_ready() )
        check.equal( len(devices), 1 )
        check.equal( devices['device123'].guid(), d1guid )  # Same device
        d1.query_option_value( d1.streams()[0].options()[0] )

        #############################################################################################
        # Recreate device456, new content, without a broadcast
        detect_broadcast()
        detect_change()
        remote_server.run( 'd2 = create_server_2( di2.topic_root )' )
        sleep( 2 );

        #############################################################################################
        # It should not get ready (because it's not online)
        check.equal( len(broadcast_devices), 0 )
        check.equal( devices_added, 0 )
        check.is_false( d2.is_online() )

        #############################################################################################
        # Broadcast it again; it should come online
        with change_expected( n_added=1 ):
            remote_server.run( 'd2.broadcast( di2 )' )
        check.is_true( d2.is_online() )
        check.is_true( watcher.is_device_broadcast( d2 ) )

        #############################################################################################
        # It should get ready, too
        d2.wait_until_ready()
        check.is_true( d2.is_ready() )

        #############################################################################################
        # Check new content
        with pytest.raises( RuntimeError, match=re.escape( r'''["query-option"] device option 'Backlight Compensation' not found''' ) ):
            d2.query_option_value( d2option )
        if check.equal( len(d2.streams()), 1 ):
            stream = d2.streams()[0]
            check.equal( stream.name(), 's2' )
            options = stream.options()
            if check.equal( len(options), 1 ):
                d2.query_option_value( options[0] )

        del watcher
        device_info_reader.stop()
        del device_info_reader
        del participant
