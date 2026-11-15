# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Runs the following for a mock D555 and for a mock D455 (control):
# 1. A server process pretends to be the camera on DDS, with a depth stream and a motion (IMU) stream.
# 2. LibRS finds it, opens its Motion sensor and starts streaming.
# 3. The server publishes a known sample (accel 1,2,3 and gyro 0.1,0.2,0.3), repeating until a frame arrives
#    because DDS drops samples sent before the reader connects.
# 4. The test reads the combined-motion frame LibRS produced.
# 5. D555: X and Z are negated, as on USB. D455: values are unchanged.

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

# D555 firmware streams the IMU in its sensor frame, while USB aligns it to the depth frame with diag(-1,1,-1).
# LibRS must apply the same alignment to a D555 over DDS, and leave other devices untouched.
D555_SERIAL = "123456789014"
D455_SERIAL = "123456789015"
SERVERS = { D555_SERIAL: "RealSense D555", D455_SERIAL: "RealSense D455" }

WIDTH = 1280
HEIGHT = 800

ACCEL = ( 1.0, 2.0, 3.0 )
GYRO = ( 0.1, 0.2, 0.3 )


if rspy.log.nested is not None:
    ###############################################################################################################
    # The server is a pair of mock DDS devices, each with a single motion stream
    #
    import pyrealdds as dds

    dds.debug( log.isEnabledFor( logging.DEBUG ), rspy.log.nested )

    participant = dds.participant()
    participant.init( config_file.get_domain_from_config_file_or_default(), "imu-alignment-server" )

    device_infos = dict()
    device_servers = dict()
    motion_streams = dict()

    def build_server( serial, name ):
        info = dds.message.device_info.from_json( {
            "name": name,
            "serial": serial,
            "product-line": "D500",
            "topic-root": "realdds/" + name.split()[-1] + "_" + serial
        } )
        # The proxy of a D500 device requires a depth sensor
        i = dds.video_intrinsics()
        i.width = WIDTH
        i.height = HEIGHT
        i.focal_length.x = 650.0
        i.focal_length.y = 650.0
        i.principal_point.x = WIDTH / 2
        i.principal_point.y = HEIGHT / 2
        i.distortion.model = dds.distortion_model.brown
        i.distortion.coeffs = [0.0, 0.0, 0.0, 0.0, 0.0]
        depth = dds.depth_stream_server( "Depth", "Stereo Module" )
        depth.init_profiles( [dds.video_stream_profile( 15, dds.video_encoding.z16, WIDTH, HEIGHT )], 0 )
        depth.init_options( [] )
        depth.set_intrinsics( set( [i] ) )

        motion = dds.motion_stream_server( "Motion", "Motion Module" )
        motion.init_profiles( [dds.motion_stream_profile( 200 )], 0 )
        motion.init_options( [] )
        intr = dds.motion_intrinsics()
        intr.data = [[1.0,0.0,0.0,0.0],[0.0,1.0,0.0,0.0],[0.0,0.0,1.0,0.0]]
        intr.noise_variances = [0.0,0.0,0.0]
        intr.bias_variances = [0.0,0.0,0.0]
        motion.set_gyro_intrinsics( intr )
        motion.set_accel_intrinsics( intr )

        server = dds.device_server( participant, info.topic_root )
        server.init( [depth, motion], [], {} )
        server.on_control( lambda srv, id, control, reply: True )  # accept open-streams etc.
        device_infos[serial] = info
        device_servers[serial] = server
        motion_streams[serial] = motion

    for serial, name in SERVERS.items():
        build_server( serial, name )

    def broadcast():
        for serial, server in device_servers.items():
            server.broadcast( device_infos[serial] )

    def publish_imu( serial, accel, gyro ):
        stream = motion_streams[serial]
        if not stream.is_streaming():
            stream.start_streaming()
        imu = dds.message.imu()
        imu.accel_data = accel
        imu.gyro_data = gyro
        imu.timestamp = dds.now()
        stream.publish_motion( imu )

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
            remote.run( 'broadcast()' )
            try:
                yield remote, context
            finally:
                del context

    def wait_for_device( context, serial, timeout=10 ):
        timer = Timer( timeout )
        timer.start()
        while True:
            for dev in context.query_devices( rs.only_sw_devices ):
                if dev.get_info( rs.camera_info.serial_number ) == serial:
                    return dev
            if timer.has_expired():
                raise TimeoutError( f"timed out waiting for mock device {serial}" )
            time.sleep( 0.5 )

    def receive_motion( remote, queue, serial, timeout=10 ):
        """DDS drops samples published before the reader matches, so keep publishing until one arrives."""
        timer = Timer( timeout )
        timer.start()
        while not timer.has_expired():
            remote.run( f'publish_imu( "{serial}", {ACCEL}, {GYRO} )' )
            f = queue.poll_for_frame()
            if f:
                return f.as_motion_frame().get_combined_motion_data()
            time.sleep( 0.05 )
        raise TimeoutError( f"timed out waiting for a motion frame from {serial}" )

    #############################################################################################
    #
    @pytest.mark.parametrize( "serial, sign",
                              [ pytest.param( D555_SERIAL, -1, id="D555" ),
                                pytest.param( D455_SERIAL, 1, id="D455" ) ] )
    def test_imu_axes_over_dds( remote_and_context, serial, sign ):
        """D555 X and Z (accel and gyro) are flipped to match USB; other devices pass through unchanged."""
        remote, context = remote_and_context
        dev = wait_for_device( context, serial )
        sensor = next( s for s in dev.query_sensors() if s.get_info( rs.camera_info.name ) == 'Motion Module' )
        profile = next( p for p in sensor.get_stream_profiles() if p.stream_type() == rs.stream.motion )
        sensor.open( profile )
        queue = rs.frame_queue( 100 )
        sensor.start( queue )
        try:
            motion = receive_motion( remote, queue, serial )
        finally:
            sensor.stop()
            sensor.close()

        accel = motion.linear_acceleration
        gyro = motion.angular_velocity
        assert ( accel.x, accel.y, accel.z ) == pytest.approx( ( sign * ACCEL[0], ACCEL[1], sign * ACCEL[2] ), abs=1e-5 )
        assert ( gyro.x, gyro.y, gyro.z ) == pytest.approx( ( sign * GYRO[0], GYRO[1], sign * GYRO[2] ), abs=1e-5 )
