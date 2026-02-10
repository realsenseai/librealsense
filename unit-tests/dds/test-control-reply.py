# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023-4 RealSense, Inc. All Rights Reserved.

#test:donotrun:!dds
#test:retries 2

from rspy import log, test, config
import pyrealdds as dds
dds.debug( log.is_debug_on() )

device_info = dds.message.device_info()
device_info.topic_root = 'server/device'

with test.remote.fork( nested_indent=None ) as remote:
    if remote is None:  # we're the fork

        with test.closure( 'Start the server participant' ):
            participant = dds.participant()
            participant.init( config.get_domain_from_config_file(), 'server' )

        with test.closure( 'Create the server' ):
            device_info.name = 'Some device'
            s1p1 = dds.video_stream_profile( 9, dds.video_encoding.rgb, 10, 10 )
            s1profiles = [s1p1]
            s1 = dds.color_stream_server( 's1', 'sensor' )
            s1.init_profiles( s1profiles, 0 )
            server = dds.device_server( participant, device_info.topic_root )
            server.init( [s1], [], {} )

        with test.closure( 'Set up a control handler' ):
            n_replies = 0
            def _on_control( server, id, control, reply ):
                # the control has already been output to debug by the calling code, as will the reply
                global n_replies
                n_replies += 1
                reply['sequence'] = n_replies            # to show that we've processed it
                reply['nested-json'] = { 'more': True }  # to show off
                return True  # otherwise the control will be flagged as error
            subscription = server.on_control( _on_control )

        raise StopIteration()  # exit the 'with' statement


    ###############################################################################################################
    # The client is a device from which we send controls
    #

    with test.closure( 'Start the client participant' ):
        participant = dds.participant()
        participant.init( config.get_domain_from_config_file(), 'client' )

    with test.closure( 'Wait for the device' ):
        device_info.name = 'Device1'
        device = dds.device( participant, device_info )
        device.wait_until_ready()

    with test.closure( 'Set up a notification handler' ):
        n_replies = 0
        notification_count = dict()
        reply_count = dict()
        notification_count[device.guid()] = 0
        reply_count[device.guid()] = 0
        import threading
        notifications = threading.Event()
        def expect_notifications( n=1 ):
            global notifications, n_notifications
            notifications.clear()
            n_notifications = n
        def _on_notification( device, id, notification ):
            global notification_count, notifications, n_notifications
            n_notifications -= 1
            if n_notifications <= 0:
                notifications.set()
            notification_count[device.guid()] += 1
            sample = notification.get( 'sample' )
            if sample is None:
                log.d( f'notification to {device}: {notification}' )
            elif sample[0] == str(device.guid()):
                log.d( f'reply to {device}' )
                reply_count[device.guid()] += 1
            else:
                log.d( f'notification to {device}' )
        notification_subscription = device.on_notification( _on_notification )

    with test.closure( 'Send a notification that is not a reply' ):
        dev1_notifications = notification_count[device.guid()]
        dev1_replies = reply_count[device.guid()]
        expect_notifications( 1 )
        remote.run( 'server.publish_notification( { "id": "something" } )' )
        notifications.wait( 3 )
        test.check_equal( notification_count[device.guid()], dev1_notifications + 1 )  # notification
        test.check_equal( reply_count[device.guid()], dev1_replies )                   # not a reply

    with test.closure( 'Set up a control sender' ):
        server_sequence = 0
        def control( device, json, n=1 ):
            global server_sequence
            server_sequence += 1
            expect_notifications( n )
            reply = device.send_control( json, True )  # Wait for reply
            if test.check( reply.get('control') is not None ):
                test.check_equal( reply['control']['id'], json['id'] )
            else:
                test.check_equal( reply['id'], json['id'] )
            test.check_equal( reply['sequence'], server_sequence )
            test.check_equal( reply['sample'][0], str(device.guid()) )
            notifications.wait( 3 )  # We may get the reply before the other notifications are received
            return reply

    with test.closure( 'Send some controls' ):
        control( device, { 'id': 'control' } )
        control( device, { 'id': 'control-2' } )

    with test.closure( 'Add a second device!' ):
        device_info.name = 'Device2'
        device2 = dds.device( participant, device_info )
        notification_count[device2.guid()] = 0
        reply_count[device2.guid()] = 0
        notification2_subscription = device2.on_notification( _on_notification )
        device2.wait_until_ready()

    with test.closure( 'Controls generate notifications to all devices' ):
        dev1_notifications = notification_count[device.guid()]
        dev2_notifications = notification_count[device2.guid()]
        control( device, { 'id': 'dev1' }, 2 )
        test.check_equal( notification_count[device.guid()], dev1_notifications + 1 )
        test.check_equal( notification_count[device2.guid()], dev2_notifications + 1 )  # both get notifications
        control( device2, { 'id': 'dev2' }, 2 )
        test.check_equal( notification_count[device.guid()], dev1_notifications + 2 )
        test.check_equal( notification_count[device2.guid()], dev2_notifications + 2 )

    with test.closure( 'But only one gets a reply' ):
        dev1_replies = reply_count[device.guid()]
        dev2_replies = reply_count[device2.guid()]
        control( device, { 'id': 'dev1' }, 2 )
        test.check_equal( reply_count[device.guid()], dev1_replies + 1 )
        test.check_equal( reply_count[device2.guid()], dev2_replies )
        control( device2, { 'id': 'dev2' }, 2 )
        test.check_equal( reply_count[device.guid()], dev1_replies + 1 )
        test.check_equal( reply_count[device2.guid()], dev2_replies + 1 )

test.print_results()

