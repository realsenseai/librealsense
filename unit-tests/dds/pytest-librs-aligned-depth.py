# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import time
import pytest
import logging
from rspy import test, config_file
from rspy.timer import Timer
import rspy.log
from pytest_check import check

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.dds,
    pytest.mark.flaky( reruns=2 ),
]

# A device that publishes aligned depth on its own topic, alongside the raw depth one. Both carry the
# same physical profile; only their camera model differs - aligned depth lives in the color viewport.
WIDTH = 1280
HEIGHT = 800

DEPTH_FOCAL = 651.822
DEPTH_PRINCIPAL = ( 640.3, 398.914 )
COLOR_FOCAL = 646.005
COLOR_PRINCIPAL = ( 637.699, 387.801 )

IDENTITY_ROTATION = ( 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 )

# No index in the name: the client indexes streams by the order they are declared in, so raw depth is
# still 0 - even though this name sorts before 'Depth' - and this one, the second depth stream, is 1
ALIGNED_STREAM = "Aligned_Depth_To_Color"
ALIGNED_INDEX = 1

MOCK_SERIAL = "123456789012"


if rspy.log.nested is not None:
    ###############################################################################################################
    # The server is a mock DDS device
    #
    import pyrealdds as dds

    dds.debug( log.isEnabledFor( logging.DEBUG ), rspy.log.nested )

    participant = dds.participant()
    participant.init( config_file.get_domain_from_config_file_or_default(), "aligned-depth-server" )

    device_info = dds.message.device_info.from_json( {
        "name": "RealSense D555",
        "serial": MOCK_SERIAL,
        "product-line": "D500",
        "topic-root": "realdds/D555/" + MOCK_SERIAL
    } )

    def intrinsics( focal, principal ):
        i = dds.video_intrinsics()
        i.width = WIDTH
        i.height = HEIGHT
        i.focal_length.x = focal
        i.focal_length.y = focal
        i.principal_point.x = principal[0]
        i.principal_point.y = principal[1]
        i.distortion.model = dds.distortion_model.brown
        i.distortion.coeffs = [0.0, 0.0, 0.0, 0.0, 0.0]
        return set( [i] )

    def depth_stream( name, focal, principal, options ):
        stream = dds.depth_stream_server( name, "Stereo Module" )
        stream.init_profiles( [dds.video_stream_profile( 15, dds.video_encoding.z16, WIDTH, HEIGHT )], 0 )
        stream.init_options( options )
        stream.set_intrinsics( intrinsics( focal, principal ) )
        return stream

    def ir_stream( number ):
        stream = dds.ir_stream_server( f"Infrared_{number}", "Stereo Module" )
        stream.init_profiles( [dds.video_stream_profile( 15, dds.video_encoding.y8, WIDTH, HEIGHT )], 0 )
        stream.init_options( [] )
        stream.set_intrinsics( intrinsics( DEPTH_FOCAL, DEPTH_PRINCIPAL ) )
        return stream

    def build_server():
        color = dds.color_stream_server( "Color", "RGB Camera" )
        color.init_profiles( [dds.video_stream_profile( 30, dds.video_encoding.rgb, WIDTH, HEIGHT )], 0 )
        color.init_options( [] )
        color.set_intrinsics( intrinsics( COLOR_FOCAL, COLOR_PRINCIPAL ) )

        align_option = dds.option.from_json(
            ["Align Depth", 0, 0, 1, 1, 0, "Enable depth-to-color alignment (UV map)"] )
        depth = depth_stream( "Depth", DEPTH_FOCAL, DEPTH_PRINCIPAL, [align_option] )
        # The aligned stream is always declared; its topic is only registered while the option is on
        aligned = depth_stream( ALIGNED_STREAM, COLOR_FOCAL, COLOR_PRINCIPAL, [] )

        extrinsics = {}
        extr = dds.extrinsics()
        extr.rotation = IDENTITY_ROTATION
        extr.translation = ( -0.059, 0.0, 0.0 )
        extrinsics[("Depth", "Color")] = extr
        extr = dds.extrinsics()
        extr.rotation = IDENTITY_ROTATION
        extr.translation = ( 0.0, 0.0, 0.0 )
        extrinsics[(ALIGNED_STREAM, "Color")] = extr

        server = dds.device_server( participant, device_info.topic_root )
        server.init( [color, depth, ir_stream( 1 ), ir_stream( 2 ), aligned], [], extrinsics )
        return server

    servers = dict()

    def broadcast_device( server, device_info ):
        global servers
        servers[device_info.serial] = { 'info': device_info, 'server': server }
        server.broadcast( device_info )
        return device_info.serial

    def close_server( instance ):
        global servers
        del servers[instance]  # throws if does not exist

else:
    ###############################################################################################################
    # The client is LibRS
    #
    log.nested = 'C  '

    from rspy import librs as rs
    if log.isEnabledFor( logging.DEBUG ):
        rs.log_to_console( rs.log_severity.debug )

    @pytest.fixture(scope='module')
    def remote_and_context():
        with test.remote.fork( script=__file__, nested_indent=None ) as remote:
            context = rs.context( { 'dds': { 'enabled': True, 'domain': config_file.get_domain_from_config_file_or_default() }} )
            try:
                yield remote, context
            finally:
                del context

    def wait_for_mock(context, timeout=10):
        """Our mock, picked out by serial - a real camera may be broadcasting on the same domain."""
        timer = Timer( timeout )
        timer.start()
        while True:
            for dev in context.query_devices( rs.only_sw_devices ):
                if dev.get_info( rs.camera_info.serial_number ) == MOCK_SERIAL:
                    return dev
            if timer.has_expired():
                raise TimeoutError( f"timed out waiting for mock device {MOCK_SERIAL}" )
            time.sleep( 0.5 )

    @pytest.fixture(scope='module')
    def device(remote_and_context):
        remote, context = remote_and_context
        remote.run( 'instance = broadcast_device( build_server(), device_info )' )
        yield wait_for_mock( context )
        remote.run( 'close_server( instance )' )

    def depth_profiles(dev):
        """The depth profiles of the stereo module, keyed by their stream name."""
        sensor = next( s for s in dev.query_sensors() if s.get_info( rs.camera_info.name ) == 'Stereo Module' )
        return sensor, { p.stream_name(): p.as_video_stream_profile()
                         for p in sensor.get_stream_profiles() if p.stream_type() == rs.stream.depth }

    #############################################################################################
    #
    def test_both_depth_streams_are_exposed(device):
        """Aligned depth is a second depth stream on the same sensor, distinguished by its index."""
        sensor, profiles = depth_profiles( device )
        check.equal( sorted( profiles.keys() ), sorted( ['Depth', ALIGNED_STREAM] ) )
        if len( profiles ) != 2:
            return
        # Raw depth must stay index 0: an app enabling RS2_STREAM_DEPTH without an index gets that one
        check.equal( profiles['Depth'].stream_index(), 0 )
        # Indices are per stream type, so this sits at 1 alongside Infrared_1 - the sensor also carries
        # two infrared streams, as the real device does
        check.equal( profiles[ALIGNED_STREAM].stream_index(), ALIGNED_INDEX )

    #############################################################################################
    #
    def test_option_maps_to_align_depth(device):
        """The firmware option name resolves to RS2_OPTION_ALIGN_DEPTH rather than a by-name option."""
        sensor, _ = depth_profiles( device )
        assert sensor.supports( rs.option.align_depth )
        check.equal( sensor.get_option( rs.option.align_depth ), 0 )
        option_range = sensor.get_option_range( rs.option.align_depth )
        check.equal( ( option_range.min, option_range.max, option_range.default ), ( 0, 1, 0 ) )

    #############################################################################################
    #
    def test_profiles_are_identical_but_for_the_camera_model(device):
        """Both streams carry the same physical profile; only their intrinsics differ."""
        sensor, profiles = depth_profiles( device )
        if len( profiles ) != 2:
            pytest.skip( "both depth streams are required" )
        raw, aligned = profiles['Depth'], profiles[ALIGNED_STREAM]

        check.equal( ( aligned.width(), aligned.height(), aligned.fps(), aligned.format() ),
                     ( raw.width(), raw.height(), raw.fps(), raw.format() ) )
        check.almost_equal( raw.get_intrinsics().fx, DEPTH_FOCAL, abs=0.01 )
        check.almost_equal( aligned.get_intrinsics().fx, COLOR_FOCAL, abs=0.01 )
        check.almost_equal( aligned.get_intrinsics().ppx, COLOR_PRINCIPAL[0], abs=0.01 )

    #############################################################################################
    #
    def test_aligned_depth_sits_in_the_color_frame(device):
        """Aligned depth needs no transformation to reach color; raw depth does."""
        sensor, profiles = depth_profiles( device )
        if len( profiles ) != 2:
            pytest.skip( "both depth streams are required" )
        color = next( p for s in device.query_sensors() for p in s.get_stream_profiles()
                      if p.stream_type() == rs.stream.color )

        aligned = profiles[ALIGNED_STREAM].get_extrinsics_to( color )
        check.equal( [round( t, 6 ) for t in aligned.translation], [0, 0, 0] )
        check.equal( [round( r, 6 ) for r in aligned.rotation], list( IDENTITY_ROTATION ) )

        raw = profiles['Depth'].get_extrinsics_to( color )
        check.almost_equal( raw.translation[0], -0.059, abs=1e-6 )
