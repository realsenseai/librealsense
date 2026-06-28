# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import re
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

else:
    @pytest.fixture(scope='module')
    def remote_and_device():
        with test.remote.fork( script=__file__, nested_indent=None ) as remote:
            # Start the client participant
            participant = dds.participant()
            participant.init( config_file.get_domain_from_config_file_or_default(), 'client' )

            # Wait for the device
            device_info.name = 'Device1'
            device = dds.device( participant, device_info )
            device.wait_until_ready()
            try:
                yield device
            finally:
                del device
                del participant

    ###############################################################################################################
    # The client is a device from which we send controls
    #

    def test_query_single_option_option_3(remote_and_device):
        device = remote_and_device
        reply = device.send_control( {
                'id': 'query-option',
                'stream-name': 's1',
                'option-name': 'Option 3'
            }, True )  # Wait for reply
        log.info( f'reply: {reply}' )
        check.equal( reply.get( 'value' ), 25. )

    #
    #############################################################################################
    #
    def test_option_names_are_case_sensitive(remote_and_device):
        device = remote_and_device
        with pytest.raises( RuntimeError, match=re.escape( '["query-option"] \'s1\' option \'custom option\' not found' ) ):
            device.send_control( {
                'id': 'query-option',
                'stream-name': 's1',
                'option-name': 'custom option'
            }, True )  # Wait for reply

    #
    #############################################################################################
    #
    def test_query_all_options_in_stream(remote_and_device):
        device = remote_and_device
        reply = device.send_control( {
                'id': 'query-options',
                'stream-name': 's1'
            }, True )  # Wait for reply
        log.info( f'reply: {reply}' )
        values = reply.get( 'option-values' )
        if check.is_true( values ):
            if check.equal( len(values), 1 ):
                values = values.get( 's1' )
                if check.is_true( values ):
                    check.equal( len(values), 3 )
                    check.equal( type(values), dict )

    #
    #############################################################################################
    #
    def test_query_all_options_in_device(remote_and_device):
        device = remote_and_device
        reply = device.send_control( {
                'id': 'query-options',
                'stream-name': ''
            }, True )  # Wait for reply
        log.info( f'reply: {reply}' )
        values = reply.get( 'option-values' )
        check.is_true( values )
        check.equal( len(values), 1 )  # 1 device option
        check.equal( type(values), dict )
        check.is_true( values.get( 'IP Address' ) )

    #
    #############################################################################################
    #
    def test_query_all_options_in_sensor(remote_and_device):
        device = remote_and_device
        reply = device.send_control( {
                'id': 'query-options',
                'sensor-name': 'sensor'
            }, True )  # Wait for reply
        log.info( f'reply: {reply}' )
        values = reply.get( 'option-values' )
        check.is_true( values )
        check.equal( len(values), 2 )
        check.equal( type(values), dict )
        check.equal( len( values.get( 's1' )), 3 )
        check.equal( len( values.get( 's2' )), 1 )

    #
    #############################################################################################
    #
    def test_query_all_options_everywhere(remote_and_device):
        device = remote_and_device
        reply = device.send_control( {
                'id': 'query-options'
            }, True )  # Wait for reply
        log.info( f'reply: {reply}' )
        values = reply.get( 'option-values' )
        check.is_true( values )
        check.equal( len(values), 3 )
        check.equal( type(values), dict )
        check.is_true( values.get( 'IP Address' ) )
        check.equal( len( values.get( 's1' )), 3 )
        check.equal( len( values.get( 's2' )), 1 )
