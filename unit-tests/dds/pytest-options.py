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

info = dds.message.device_info()
info.name = "Test Device"
info.topic_root = "realsense/options-device"

if rspy.log.nested is not None:
    dds.debug( log.isEnabledFor( logging.DEBUG ), rspy.log.nested )

    participant = dds.participant()
    participant.init( config_file.get_domain_from_config_file_or_default(), "server" )

    def test_no_options():
        # Create one stream with one profile so device init won't fail
        # No device options, no stream options
        s1p1 = dds.video_stream_profile( 9, dds.video_encoding.rgb, 10, 10 )
        s1profiles = [s1p1]
        s1 = dds.depth_stream_server( "s1", "sensor" )
        s1.init_profiles( s1profiles, 0 )
        dev_opts = []
        global server
        server = dds.device_server( participant, info.topic_root )
        server.init( [s1], dev_opts, {} )

    def test_device_options_discovery( values ):
        # Create one stream with one profile so device init won't fail, no stream options
        s1p1 = dds.video_stream_profile( 9, dds.video_encoding.rgb, 10, 10 )
        s1profiles = [s1p1]
        s1 = dds.depth_stream_server( "s1", "sensor" )
        s1.init_profiles( s1profiles, 0 )
        dev_opts = []
        for index, value in enumerate( values ):
            option = dds.option.from_json( [f'opt{index}', value, f'opt{index} description'] )
            dev_opts.append( option )
        global server
        server = dds.device_server( participant, info.topic_root )
        server.init( [s1], dev_opts, {})

    def test_stream_options_discovery( value, min, max, step, default, description ):
        s1p1 = dds.video_stream_profile( 9, dds.video_encoding.rgb, 10, 10 )
        s1profiles = [s1p1]
        s1 = dds.depth_stream_server( "s1", "sensor" )
        s1.init_profiles( s1profiles, 0 )
        s1.init_options( [
            dds.option.from_json( ["opt1", value, "opt1 is const"] ),
            dds.option.from_json( ["opt2", default, min, max, step, default, "opt2 with range"] ),
            dds.option.from_json( ["opt3", 0.15, 0, 1, 0.05, 0.15, description] ),
            dds.option.from_json( ["opt4", 'name', None, 'opt4 is an optional string with no default', ['optional']] )
            ] )
        global server
        server = dds.device_server( participant, info.topic_root )
        server.init( [s1], [], {} )

    def test_device_and_multiple_stream_options_discovery( dev_values, stream_values ):
        dev_options = []
        for index, value in enumerate( dev_values ):
            option = dds.option.from_json( [f'opt{index}', value, value, value, 0., value, f'opt{index} description'] )
            dev_options.append( option )

        s1p1 = dds.video_stream_profile( 9, dds.video_encoding.rgb, 10, 10 )
        s1profiles = [s1p1]
        s1 = dds.depth_stream_server( "s1", "sensor" )
        s1.init_profiles( s1profiles, 0 )
        stream_options = []
        for index, value in enumerate( stream_values ):
            option = dds.option.from_json( [f'opt{index}', value, value, value, 0., value, f'opt{index} description'] )
            stream_options.append( option )
        s1.init_options( stream_options )

        s2p1 = dds.video_stream_profile( 9, dds.video_encoding.rgb, 10, 10 )
        s2profiles = [s2p1]
        s2 = dds.depth_stream_server( "s2", "sensor" )
        s2.init_profiles( s2profiles, 0 )
        stream_options = []
        for index, value in enumerate( stream_values ):
            option = dds.option.from_json( [f'opt{index}', value, value, value, 0., value, f'opt{index} description'] )
            stream_options.append( option )
        s2.init_options( stream_options )

        global server
        server = dds.device_server( participant, info.topic_root )
        server.init( [s1, s2], dev_options, {} )

    def close_server():
        global server
        server = None

else:
    #############################################################################################
    #
    log.nested = 'C  '
    dds.debug( log.isEnabledFor( logging.DEBUG ), 'C  ' )

    @pytest.fixture(scope='module')
    def remote_server_and_participant():
        with test.remote.fork( script=__file__, nested_indent=None ) as remote:
            participant = dds.participant()
            participant.init( config_file.get_domain_from_config_file_or_default(), "test-options" )
            yield remote, participant

    def test_options(remote_server_and_participant):
        remote, participant = remote_server_and_participant

        #############################################################################################
        # Test no options
        remote.run( 'test_no_options()' )
        device = dds.device( participant, info )
        device.wait_until_ready()

        options = device.options();
        for option in options:
            pytest.fail( 'unreachable' )  # Test no device option

        check.equal( device.n_streams(), 1 )
        for stream in device.streams():
            options = stream.options();
            for option in options:
                pytest.fail( 'unreachable' )  # Test no stream option

        remote.run( 'close_server()' )
        device = None

        #############################################################################################
        # Test device options discovery
        test_values = [1,2.,'haha',2.2]
        remote.run( f'test_device_options_discovery( {test_values} )' )
        device = dds.device( participant, info )
        device.wait_until_ready()

        options = device.options();
        check.equal( len( options ), len(test_values) )
        for index, option in enumerate( options ):
            check.equal( option.get_value(), test_values[index] )

        option.set_value( -1. )  # only on client!
        check.equal( device.query_option_value( option ), float( test_values[index] ) )
        check.equal( option.get_value(), test_values[index] )  # from server

        device.set_option_value( option, -2. )  # TODO this is not valid for the option range!
        check.equal( option.get_value(), -2. )

        remote.run( 'close_server()' )
        device = None

        #############################################################################################
        # Test stream options discovery
        #send values to be checked later as string parameter to the function
        remote.run( 'test_stream_options_discovery(1, 0, 123456, 123, 12., "opt3 of s1")' )
        device = dds.device( participant, info )
        device.wait_until_ready()
        check.equal( device.n_streams(), 1 )
        for stream in device.streams():
            options = stream.options();
            check.equal( len( options ), 4 )
            check.equal( options[0].get_value(), 1 )
            check.equal( options[1].get_minimum_value(), 0 )
            check.equal( options[1].get_maximum_value(), 123456 )
            check.equal( options[1].get_stepping(), 123 )
            check.equal( options[1].get_default_value(), 12. )
            check.equal( options[2].get_description(), "opt3 of s1" )
            check.equal( options[3].value_type(), 'string' )
            check.equal( options[3].get_default_value(), None )
            check.equal( options[3].get_value(), 'name' )

            option = options[1]
            check.equal( option.get_value(), option.get_default_value() )
            option.set_value( 1. )  # only on client!
            check.equal( option.get_value(), 1. )
            check.equal( device.query_option_value( option ), option.get_default_value() )  # from server
            check.equal( option.get_value(), option.get_default_value() )  # client got updated!

            device.set_option_value( option, 12. )  # updates server & client
            check.equal( option.get_value(), 12. )

        device = None
        stream = None

        #############################################################################################
        # New device should get the new option value
        check.is_true( option is not None )
        check.is_true( option.stream() is None )  # Because we removed the device & stream references
        device = dds.device( participant, info )
        device.wait_until_ready()
        if check.equal( device.n_streams(), 1 ):
            stream = device.streams()[0]
            options = stream.options();
            check.equal( len( options ), 4 )
            option = options[1]
            check.equal( option.get_value(), 12. )  # The new value - not the default

        remote.run( 'close_server()' )
        device = None

        #############################################################################################
        # Test device and multiple stream options discovery
        test_values = list(range(5))
        remote.run( 'test_device_and_multiple_stream_options_discovery(' + str( test_values ) + ', ' + str( test_values ) + ')' )
        device = dds.device( participant, info )
        device.wait_until_ready()

        options = device.options();
        check.equal( len( options ), len(test_values) )
        for index, option in enumerate( options ):
            check.equal( option.get_value(), test_values[index] )

        for stream in device.streams():
            options = stream.options();
            check.equal( len( options ), len(test_values) )
            for index, option in enumerate( options ):
                check.equal( option.get_value(), test_values[index] )

        remote.run( 'close_server()' )
#############################################################################################
