# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import logging
import pyrealdds as dds
from rspy import test, config_file
import rspy.log
from pytest_check import check

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.dds,
    pytest.mark.flaky( retries=2 ),
]

dds.debug( log.isEnabledFor( logging.DEBUG ) )

device_info = dds.message.device_info()
device_info.topic_root = 'server/device'

if rspy.log.nested is not None:
    # Start the server participant
    participant = dds.participant()
    participant.init( config_file.get_domain_from_config_file_or_default(), 'server' )

    # Create the server
    device_info.name = 'Some device'
    s1p1 = dds.video_stream_profile( 9, dds.video_encoding.rgb, 10, 10 )
    s1profiles = [s1p1]
    s1 = dds.color_stream_server( 's1', 'sensor' )
    s1.init_profiles( s1profiles, 0 )
    server = dds.device_server( participant, device_info.topic_root )
    server.init( [s1], [], {} )

    # Set up a control handler
    n_replies = 0
    def _on_control( server, id, control, reply ):
        # the control has already been output to debug by the calling code, as will the reply
        global n_replies
        n_replies += 1
        reply['sequence'] = n_replies            # to show that we've processed it
        reply['nested-json'] = { 'more': True }  # to show off
        return True  # otherwise the control will be flagged as error
    subscription = server.on_control( _on_control )

else:
    @pytest.fixture(scope='module')
    def remote_server():
        with test.remote.fork( script=__file__, nested_indent=None ) as remote:
            yield remote

    ###############################################################################################################
    # The client is a device from which we send controls
    #

    def test_control_reply(remote_server):
        # Start the client participant
        participant = dds.participant()
        participant.init( config_file.get_domain_from_config_file_or_default(), 'client' )

        # Wait for the device
        device_info.name = 'Device1'
        device = dds.device( participant, device_info )
        device.wait_until_ready()

        # Set up a notification handler
        n_replies = 0
        notification_count = dict()
        reply_count = dict()
        notification_count[device.guid()] = 0
        reply_count[device.guid()] = 0
        import threading
        notifications = threading.Event()
        n_notifications = 0
        def expect_notifications( n=1 ):
            nonlocal notifications, n_notifications
            notifications.clear()
            n_notifications = n
        def _on_notification( device, id, notification ):
            nonlocal notification_count, notifications, n_notifications
            n_notifications -= 1
            if n_notifications <= 0:
                notifications.set()
            notification_count[device.guid()] += 1
            sample = notification.get( 'sample' )
            if sample is None:
                log.debug( f'notification to {device}: {notification}' )
            elif sample[0] == str(device.guid()):
                log.debug( f'reply to {device}' )
                reply_count[device.guid()] += 1
            else:
                log.debug( f'notification to {device}' )
        notification_subscription = device.on_notification( _on_notification )

        # Send a notification that is not a reply
        dev1_notifications = notification_count[device.guid()]
        dev1_replies = reply_count[device.guid()]
        expect_notifications( 1 )
        remote_server.run( 'server.publish_notification( { "id": "something" } )' )
        notifications.wait( 3 )
        check.equal( notification_count[device.guid()], dev1_notifications + 1 )  # notification
        check.equal( reply_count[device.guid()], dev1_replies )                   # not a reply

        # Set up a control sender
        server_sequence = 0
        def control( device, json, n=1 ):
            nonlocal server_sequence
            server_sequence += 1
            expect_notifications( n )
            reply = device.send_control( json, True )  # Wait for reply
            if check.is_true( reply.get('control') is not None ):
                check.equal( reply['control']['id'], json['id'] )
            else:
                check.equal( reply['id'], json['id'] )
            check.equal( reply['sequence'], server_sequence )
            check.equal( reply['sample'][0], str(device.guid()) )
            notifications.wait( 3 )  # We may get the reply before the other notifications are received
            return reply

        # Send some controls
        control( device, { 'id': 'control' } )
        control( device, { 'id': 'control-2' } )

        # Add a second device!
        device_info.name = 'Device2'
        device2 = dds.device( participant, device_info )
        notification_count[device2.guid()] = 0
        reply_count[device2.guid()] = 0
        notification2_subscription = device2.on_notification( _on_notification )
        device2.wait_until_ready()

        # Controls generate notifications to all devices
        dev1_notifications = notification_count[device.guid()]
        dev2_notifications = notification_count[device2.guid()]
        control( device, { 'id': 'dev1' }, 2 )
        check.equal( notification_count[device.guid()], dev1_notifications + 1 )
        check.equal( notification_count[device2.guid()], dev2_notifications + 1 )  # both get notifications
        control( device2, { 'id': 'dev2' }, 2 )
        check.equal( notification_count[device.guid()], dev1_notifications + 2 )
        check.equal( notification_count[device2.guid()], dev2_notifications + 2 )

        # But only one gets a reply
        dev1_replies = reply_count[device.guid()]
        dev2_replies = reply_count[device2.guid()]
        control( device, { 'id': 'dev1' }, 2 )
        check.equal( reply_count[device.guid()], dev1_replies + 1 )
        check.equal( reply_count[device2.guid()], dev2_replies )
        control( device2, { 'id': 'dev2' }, 2 )
        check.equal( reply_count[device.guid()], dev1_replies + 1 )
        check.equal( reply_count[device2.guid()], dev2_replies + 1 )


