# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#test:donotrun:!dds
#test:retries 2

import pyrealdds as dds
from rspy import log, test, config_file

info = dds.message.device_info()
info.name = "Test Device"
info.topic_root = "realsense/options-device"

with test.remote.fork( nested_indent=None ) as remote:
    if remote is None:  # we're the fork

        dds.debug( log.is_debug_on(), log.nested )

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

        raise StopIteration()  # exit the 'with' statement


    ###############################################################################################################
    #
    dds.debug( log.is_debug_on(), 'C  ' )
    log.nested = 'C  '

    participant = dds.participant()
    participant.init( config_file.get_domain_from_config_file_or_default(), "test-options" )


    #############################################################################################
    with test.closure( "Test no options" ):
        remote.run( 'test_no_options()' )
        device = dds.device( participant, info )
        device.wait_until_ready()

        options = device.options();
        for option in options:
            test.unreachable() # Test no device option

        test.check_equal( device.n_streams(), 1 )
        for stream in device.streams():
            options = stream.options();
            for option in options:
                test.unreachable() # Test no stream option

        remote.run( 'close_server()' )
    device = None


    #############################################################################################
    with test.closure( "Test device options discovery" ):
        test_values = [1,2.,'haha',2.2]
        remote.run( f'test_device_options_discovery( {test_values} )' )
        device = dds.device( participant, info )
        device.wait_until_ready()

        options = device.options();
        test.check_equal( len( options ), len(test_values) )
        for index, option in enumerate( options ):
            test.check_equal( option.get_value(), test_values[index] )

        option.set_value( -1. )  # only on client!
        test.check_equal( device.query_option_value( option ), float( test_values[index] ) )
        test.check_equal( option.get_value(), test_values[index] )  # from server

        device.set_option_value( option, -2. )  # TODO this is not valid for the option range!
        test.check_equal( option.get_value(), -2. )

        remote.run( 'close_server()' )
    device = None


    #############################################################################################
    with test.closure( "Test stream options discovery" ):
        #send values to be checked later as string parameter to the function
        remote.run( 'test_stream_options_discovery(1, 0, 123456, 123, 12., "opt3 of s1")' )
        device = dds.device( participant, info )
        device.wait_until_ready()
        test.check_equal( device.n_streams(), 1 )
        for stream in device.streams():
            options = stream.options();
            test.check_equal( len( options ), 4 )
            test.check_equal( options[0].get_value(), 1 )
            test.check_equal( options[1].get_minimum_value(), 0 )
            test.check_equal( options[1].get_maximum_value(), 123456 )
            test.check_equal( options[1].get_stepping(), 123 )
            test.check_equal( options[1].get_default_value(), 12. )
            test.check_equal( options[2].get_description(), "opt3 of s1" )
            test.check_equal( options[3].value_type(), 'string' )
            test.check_equal( options[3].get_default_value(), None )
            test.check_equal( options[3].get_value(), 'name' )

        option = options[1]
        test.check_equal( option.get_value(), option.get_default_value() )
        option.set_value( 1. )  # only on client!
        test.check_equal( option.get_value(), 1. )
        test.check_equal( device.query_option_value( option ), option.get_default_value() )  # from server
        test.check_equal( option.get_value(), option.get_default_value() )  # client got updated!

        device.set_option_value( option, 12. )  # updates server & client
        test.check_equal( option.get_value(), 12. )

    device = None
    stream = None


    #############################################################################################
    with test.closure( "New device should get the new option value" ):
        test.check( option is not None )
        test.check( option.stream() is None )  # Because we removed the device & stream references
        device = dds.device( participant, info )
        device.wait_until_ready()
        if test.check_equal( device.n_streams(), 1 ):
            stream = device.streams()[0]
            options = stream.options();
            test.check_equal( len( options ), 4 )
            option = options[1]
            test.check_equal( option.get_value(), 12. )  # The new value - not the default

        remote.run( 'close_server()' )
    device = None


    #############################################################################################
    with test.closure( "Test device and multiple stream options discovery" ):
        test_values = list(range(5))
        remote.run( 'test_device_and_multiple_stream_options_discovery(' + str( test_values ) + ', ' + str( test_values ) + ')' )
        device = dds.device( participant, info )
        device.wait_until_ready()

        options = device.options();
        test.check_equal( len( options ), len(test_values) )
        for index, option in enumerate( options ):
            test.check_equal( option.get_value(), test_values[index] )

        for stream in device.streams():
            options = stream.options();
            test.check_equal( len( options ), len(test_values) )
            for index, option in enumerate( options ):
                test.check_equal( option.get_value(), test_values[index] )

        remote.run( 'close_server()' )
    device = None
    participant = None


#############################################################################################
test.print_results()
