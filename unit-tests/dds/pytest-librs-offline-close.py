# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import gc
import time
import pytest
import logging
from rspy import test, config_file
from rspy.timer import Timer
import rspy.log

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.dds,
]

# A device that drops off the network while a stream is open and then comes back. The client keeps
# the same device object across the outage, so state that survived the outage must not poison it.
WIDTH = 1280
HEIGHT = 800

MOCK_SERIAL = "123456789013"


if rspy.log.nested is not None:
    ###############################################################################################################
    # The server is a mock DDS device
    #
    import pyrealdds as dds

    dds.debug( log.isEnabledFor( logging.DEBUG ), rspy.log.nested )

    participant = dds.participant()
    participant.init( config_file.get_domain_from_config_file_or_default(), "offline-close-server" )

    device_info = dds.message.device_info.from_json( {
        "name": "RealSense D555",
        "serial": MOCK_SERIAL,
        "product-line": "D500",
        "topic-root": "realdds/D555/" + MOCK_SERIAL
    } )

    def intrinsics():
        i = dds.video_intrinsics()
        i.width = WIDTH
        i.height = HEIGHT
        i.focal_length.x = 650.0
        i.focal_length.y = 650.0
        i.principal_point.x = WIDTH / 2
        i.principal_point.y = HEIGHT / 2
        i.distortion.model = dds.distortion_model.brown
        i.distortion.coeffs = [0.0, 0.0, 0.0, 0.0, 0.0]
        return set( [i] )

    def video_stream( stream, fps, encoding ):
        stream.init_profiles( [dds.video_stream_profile( fps, encoding, WIDTH, HEIGHT )], 0 )
        stream.init_options( [] )
        stream.set_intrinsics( intrinsics() )
        return stream

    def build_server():
        color = video_stream( dds.color_stream_server( "Color", "RGB Camera" ), 30, dds.video_encoding.rgb )
        depth = video_stream( dds.depth_stream_server( "Depth", "Stereo Module" ), 15, dds.video_encoding.z16 )

        extr = dds.extrinsics()
        extr.rotation = ( 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 )
        extr.translation = ( -0.059, 0.0, 0.0 )

        server = dds.device_server( participant, device_info.topic_root )
        server.init( [color, depth], [], { ("Depth", "Color"): extr } )
        server.on_control( lambda srv, id, control, reply: True )  # accept open-streams etc.
        return server

    server = build_server()

    def broadcast():
        server.broadcast( device_info )

    def disconnect():
        server.broadcast_disconnect()

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

    def find_mock(context):
        """Our mock, picked out by serial - a real camera may be broadcasting on the same domain."""
        for dev in context.query_devices( rs.only_sw_devices ):
            if dev.get_info( rs.camera_info.serial_number ) == MOCK_SERIAL:
                return dev
        return None

    def wait_for_mock(context, present=True, timeout=10):
        timer = Timer( timeout )
        timer.start()
        while True:
            dev = find_mock( context )
            if bool( dev ) == present:
                return dev
            if timer.has_expired():
                raise TimeoutError( f"timed out waiting for mock device {MOCK_SERIAL} to be {'found' if present else 'removed'}" )
            time.sleep( 0.5 )

    def sensor_named(dev, name):
        return next( s for s in dev.query_sensors() if s.get_info( rs.camera_info.name ) == name )

    def profile_of(sensor, stream_type):
        return next( p for p in sensor.get_stream_profiles() if p.stream_type() == stream_type )

    #############################################################################################
    #
    def test_close_after_device_dropped_mid_stream(remote_and_context):
        """A stream left open when the device drops offline must not break close() on any sensor once
        the device is back."""
        remote, context = remote_and_context
        remote.run( 'broadcast()' )
        dev = wait_for_mock( context )

        depth_sensor = sensor_named( dev, 'Stereo Module' )
        depth_sensor.open( profile_of( depth_sensor, rs.stream.depth ) )
        depth_sensor.start( rs.frame_queue( 10 ) )

        # The device goes away with the stream still open; closing it now cannot reach the device
        remote.run( 'disconnect()' )
        wait_for_mock( context, present=False )
        depth_sensor.stop()
        try:
            depth_sensor.close()
        except RuntimeError as e:
            log.info( f"close() while offline: {e}" )

        # Still holding 'dev', so the client reuses the same device object when it comes back
        remote.run( 'broadcast()' )
        new_dev = wait_for_mock( context )

        # Drop the first incarnation so its streams are released, as a real application would over time
        del depth_sensor, dev
        gc.collect()

        color_sensor = sensor_named( new_dev, 'RGB Camera' )
        color_sensor.open( profile_of( color_sensor, rs.stream.color ) )
        color_sensor.close()

        depth_sensor = sensor_named( new_dev, 'Stereo Module' )
        depth_sensor.open( profile_of( depth_sensor, rs.stream.depth ) )
        depth_sensor.close()
