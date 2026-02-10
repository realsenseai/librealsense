# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#test:donotrun:!dds
#test:retries 2

from rspy import log, test, config_file
import pyrealdds as dds
dds.debug( log.is_debug_on() )

device_info = dds.message.device_info()
device_info.topic_root = 'server/device'


with test.remote.fork( nested_indent=None ) as remote:
    if remote is None:  # we're the fork

        with test.closure( 'Start the server participant' ):
            participant = dds.participant()
            participant.init( config_file.get_domain_from_config_file(), 'server' )

        with test.closure( 'Create the server' ):
            device_info.name = 'Some device'
            s1 = dds.color_stream_server( 's1', 'sensor' )
            s1.init_profiles( [
                dds.video_stream_profile( 9, dds.video_encoding.rgb, 10, 10 )
                ], 0 )
            s1.init_options( [
                dds.option.from_json( ['Backlight Compensation', 0, 0, 1, 1, 0, 'Backlight custom description'] ),
                dds.option.from_json( ['Custom Option', 0.5, 0, 1, 0.1, 0.5, 'Something'] ),
                dds.option.from_json( ['Option 3', 25., 0, 50, 1, 25, 'Something Else'] )
                ] )
            s2 = dds.depth_stream_server( 's2', 'sensor' )
            s2.init_profiles( [
                dds.video_stream_profile( 27, dds.video_encoding.z16, 100, 100 )
                ], 0 )
            s2.init_options( [
                dds.option.from_json( ['s2 option', 123, 'read-only integer'] )
                ] )
            server = dds.device_server( participant, device_info.topic_root )
            server.init( [s1, s2], [
                dds.option.from_json( ['IP Address', '1.2.3.4', None, 'IP', ['optional', 'IPv4']] )
                ], {} )

        raise StopIteration()  # exit the 'with' statement


    ###############################################################################################################
    # The client is a device from which we send controls
    #

    with test.closure( 'Start the client participant' ):
        participant = dds.participant()
        participant.init( config_file.get_domain_from_config_file(), 'client' )

    with test.closure( 'Wait for the device' ):
        device_info.name = 'Device1'
        device = dds.device( participant, device_info )
        device.wait_until_ready()

    with test.closure( 'Query single option, option-name: "Option 3"' ):
        reply = device.send_control( {
                'id': 'query-option',
                'stream-name': 's1',
                'option-name': 'Option 3'
            }, True )  # Wait for reply
        test.info( 'reply', reply )
        test.check_equal( reply.get( 'value' ), 25. )

    with test.closure( 'Option names are case-sensitive' ):
        test.check_throws( lambda:
            device.send_control( {
                'id': 'query-option',
                'stream-name': 's1',
                'option-name': 'custom option'
            }, True ),  # Wait for reply
            RuntimeError,
            '["query-option"] \'s1\' option \'custom option\' not found' )

    with test.closure( 'Query all options in a stream' ):
        reply = device.send_control( {
                'id': 'query-options',
                'stream-name': 's1'
            }, True )  # Wait for reply
        test.info( 'reply', reply )
        values = reply.get( 'option-values' )
        if test.check( values ):
            if test.check_equal( len(values), 1 ):
                values = values.get( 's1' )
                if test.check( values ):
                    test.check_equal( len(values), 3 )
                    test.check_equal( type(values), dict )

    with test.closure( 'Query all options in the device', on_fail=test.RAISE ):
        reply = device.send_control( {
                'id': 'query-options',
                'stream-name': ''
            }, True )  # Wait for reply
        test.info( 'reply', reply )
        values = reply.get( 'option-values' )
        test.check( values )
        test.check_equal( len(values), 1 )  # 1 device option
        test.check_equal( type(values), dict )
        test.check( values.get( 'IP Address' ) )

    with test.closure( 'Query all options in the sensor', on_fail=test.RAISE ):
        reply = device.send_control( {
                'id': 'query-options',
                'sensor-name': 'sensor'
            }, True )  # Wait for reply
        test.info( 'reply', reply )
        values = reply.get( 'option-values' )
        test.check( values )
        test.check_equal( len(values), 2 )
        test.check_equal( type(values), dict )
        test.check_equal( len( values.get( 's1' )), 3 )
        test.check_equal( len( values.get( 's2' )), 1 )

    with test.closure( 'Query all options, everywhere', on_fail=test.RAISE ):
        reply = device.send_control( {
                'id': 'query-options'
            }, True )  # Wait for reply
        test.info( 'reply', reply )
        values = reply.get( 'option-values' )
        test.check( values )
        test.check_equal( len(values), 3 )
        test.check_equal( type(values), dict )
        test.check( values.get( 'IP Address' ) )
        test.check_equal( len( values.get( 's1' )), 3 )
        test.check_equal( len( values.get( 's2' )), 1 )

    device = None

test.print_results()
