# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import re
import logging
from rspy import test, config_file
import rspy.log
from pytest_check import check

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.dds,
    pytest.mark.flaky( retries=2 ),
]

if rspy.log.nested is not None:
    import pyrealdds as dds
    dds.debug( log.isEnabledFor( logging.DEBUG ) )

    # Start the server participant
    participant = dds.participant()
    participant.init( config_file.get_domain_from_config_file_or_default(), 'server' )

    # Create the server
    device_info = dds.message.device_info.from_json({
        "name": "Options device",
        "topic-root": "librs-options/device",
        "product-line": "D400"
    })
    s1p1 = dds.video_stream_profile( 9, dds.video_encoding.rgb, 10, 10 )
    s1profiles = [s1p1]
    s1 = dds.color_stream_server( 's1', 'sensor' )
    s1.init_profiles( s1profiles, 0 )
    s1.init_options( [
        dds.option.from_json( ['Backlight Compensation', 0, 0, 1, 1, 0, 'Backlight custom description'] ),
        dds.option.from_json( ['Boolean Option', False, False, 'Something'] ),
        dds.option.from_json( ['Integer Option', 1, None, 'Something', ['optional']] ),
        dds.option.from_json( ['Enum Option', 'First', ['First','Last','Everything'], 'Last', 'My'] ),
        dds.option.from_json( ['R/O Option', 'Value', 'Read-only string option'] ),
        dds.option.from_json( ['Visual Preset', 'Default', ['Default','Preset-1','Preset-2'], 'Default', 'Should enable serialization'] )
        ] )
    s2 = dds.depth_stream_server( 's2', 'depth' ) # Depth sensor is expected
    s2.init_profiles( [dds.video_stream_profile( 1, dds.video_encoding.z16, 10, 10 )], 0 )
    s2.init_options( [] )
    server = dds.device_server( participant, device_info.topic_root )
    server.init( [s1, s2], [], {} )

    # Set up a handler to keep track of the change order
    def _on_set_option( server, option, value ):
        print( option.get_name() )
    server.on_set_option( _on_set_option )

    # Broadcast the device
    server.broadcast( device_info )

else:
    ###############################################################################################################
    # The client is LibRS
    #
    from rspy import librs as rs
    if log.isEnabledFor( logging.DEBUG ):
        rs.log_to_console( rs.log_severity.debug )

    @pytest.fixture(scope='module')
    def remote_and_sensor():
        with test.remote.fork( script=__file__, nested_indent=None ) as remote:
            # Initialize librealsense context
            context = rs.context( { 'dds': { 'enabled': True, 'domain': config_file.get_domain_from_config_file_or_default() }} )
            # Find the server
            dev = rs.wait_for_devices( context, rs.only_sw_devices, n=1. )
            for s in dev.query_sensors():
                break
            options = s.get_supported_options()
            try:
                yield remote, s, options
            finally:
                # All done
                del dev
                del context

    #
    #############################################################################################
    #
    def test_supported_option_count(remote_and_sensor):
        _, s, options = remote_and_sensor
        check.equal( len(options), 7 )  # 'Frames Queue Size' gets added by SDK

    #
    #############################################################################################
    #
    def test_integer_option(remote_and_sensor):
        _, s, options = remote_and_sensor
        io = next( o for o in options if str(o) == 'Integer Option' )
        iv = s.get_option_value( io )
        check.equal( iv.type, rs.option_type.integer )
        check.equal( iv.value, 1 )
        check.equal( s.get_option( io ), 1. )
        s.set_option( io, 5 )
        check.equal( s.get_option( io ), 5. )

    #
    #############################################################################################
    #
    def test_boolean_option(remote_and_sensor):
        _, s, options = remote_and_sensor
        bo = next( o for o in options if str(o) == 'Boolean Option' )
        bv = s.get_option_value( bo )
        check.equal( bv.type, rs.option_type.boolean )
        check.equal( bv.value, False )
        check.equal( s.get_option( bo ), 0. )
        s.set_option( bo, 1. )
        check.equal( s.get_option( bo ), 1. )
        with pytest.raises( RuntimeError, match=re.escape( 'not a boolean: 2' ) ):
            s.set_option( bo, 2. )
        with pytest.raises( RuntimeError, match=re.escape( 'not a boolean: 1.01' ) ):
            s.set_option( bo, 1.01 )

    #
    #############################################################################################
    #
    def test_enum_option(remote_and_sensor):
        _, s, options = remote_and_sensor
        eo = next( o for o in options if str(o) == 'Enum Option' )
        ev = s.get_option_value( eo )
        check.equal( ev.type, rs.option_type.string )
        check.equal( ev.value, 'First' )
        er = s.get_option_range( ev.id )
        check.equal( er.min, 0. )
        check.equal( er.max, 2. )
        check.equal( er.default, 1. )
        check.equal( er.step, 1. )
        check.equal( s.get_option_value_description( eo, 0. ), 'First' )
        check.equal( s.get_option_value_description( eo, 1. ), 'Last' )
        check.equal( s.get_option_value_description( eo, 2. ), 'Everything' )
        check.equal( s.get_option( eo ), 0. )
        s.set_option( eo, 2. )
        check.equal( s.get_option_value( eo ).value, 'Everything' )
        s.set_option_value( eo, 'Last' )
        check.equal( s.get_option( eo ), 1. )

    #
    #############################################################################################
    #
    def test_read_only_option(remote_and_sensor):
        _, s, options = remote_and_sensor
        ro = next( o for o in options if str(o) == 'R/O Option' )
        with pytest.raises( RuntimeError, match=re.escape( 'use rs2_get_option_value to get string values' ) ):
            s.get_option( ro )
        rv = s.get_option_value( ro )
        check.equal( rv.type, rs.option_type.string )
        check.equal( rv.value, 'Value' )
        check.is_true( s.is_option_read_only( rv.id ) )
        rr = s.get_option_range( rv.id )
        check.equal( rr.min, 0. )
        check.equal( rr.max, 0. )
        check.equal( rr.default, 0. )
        check.equal( rr.step, 0. )
        with pytest.raises( RuntimeError, match=re.escape( 'use rs2_set_option_value to set string values' ) ):
            s.set_option( ro, 2. )
        with pytest.raises( RuntimeError, match=re.escape( 'option is read-only: R/O Option' ) ):
            s.set_option_value( ro, 'Blah' )

    # Serialization of DDS devices now implemented using advanced_mode interface, tested under live-options-presets UT
