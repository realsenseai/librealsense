# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Tests object-detection streaming through the LibRS API:
#   - Server creates a device with an object_detection_stream_server and publishes detection JSON
#   - Client uses pyrealsense2 (LibRS) to receive and parse the frames via the object_detection_frame API

import pytest
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
    import json as json_module

    dds.debug( log.isEnabledFor( logging.DEBUG ), rspy.log.nested )

    # Start server participant
    participant = dds.participant()
    participant.init( config_file.get_domain_from_config_file_or_default(), 'server' )

    # Create OD device server
    device_info = dds.message.device_info.from_json( {
        "name": "Test OD Device",
        "topic-root": "realsense/test-librs-object-detection",
        "product-line": "D400"
    } )

    od = dds.object_detection_stream_server( 'Object Detection', 'Inference Sensor' )
    od.init_profiles( [dds.inference_stream_profile( 30 )], 0 )
    od.init_options( [] )

    depth = dds.depth_stream_server( 'Depth', 'Stereo Module' )  # LibRS expects a depth sensor
    depth.init_profiles( [dds.video_stream_profile( 30, dds.video_encoding.z16, 640, 480 )], 0 )
    depth.init_options( [] )

    color = dds.color_stream_server( 'Color', 'RGB Camera' )
    color.init_profiles( [dds.video_stream_profile( 30, dds.video_encoding.rgb, 640, 480 )], 0 )
    color.init_options( [] )
    color_i = dds.video_intrinsics()
    color_i.width = 640
    color_i.height = 480
    color_i.principal_point.x = 320.0
    color_i.principal_point.y = 240.0
    color_i.focal_length.x = 600.0
    color_i.focal_length.y = 600.0
    color_i.distortion.model = dds.distortion_model.none
    color_i.distortion.coeffs = [0.0, 0.0, 0.0, 0.0, 0.0]
    color.set_intrinsics( set( [color_i] ) )

    server = dds.device_server( participant, device_info.topic_root )
    server.on_control( lambda srv, id, control, reply: True )  # accept open-streams and other controls
    server.init( [od, depth, color], [], {} )

    # Broadcast device
    server.broadcast( device_info )

    def start_od_streaming():
        od.start_streaming()

    def stop_od_streaming():
        od.stop_streaming()

    def publish_two_detections():
        payload = {
            "frame_id": 1,
            "number_of_detections": 2,
            "detections": [
                { "class_id": 0, "confidence": 85, "x1": 10,  "y1": 20,  "x2": 100, "y2": 200, "distance": 0.0 },
                { "class_id": 1, "confidence": 70, "x1": 150, "y1": 50,  "x2": 300, "y2": 250, "distance": 0.0 }
            ],
            "source_frame_id": 42,
            "version": 1
        }
        od.publish_inference( json_module.dumps( payload ) )

    def publish_single_detection():
        payload = {
            "frame_id": 2,
            "number_of_detections": 1,
            "detections": [
                { "class_id": 0, "confidence": 92, "x1": 50, "y1": 60, "x2": 200, "y2": 300, "distance": 0.0 }
            ],
            "source_frame_id": 43,
            "version": 1
        }
        od.publish_inference( json_module.dumps( payload ) )

    def publish_zero_detections():
        payload = {
            "frame_id": 3,
            "number_of_detections": 0,
            "detections": [],
            "source_frame_id": 44,
            "version": 1
        }
        od.publish_inference( json_module.dumps( payload ) )

else:
    ###############################################################################################################
    # The client is LibRS
    #
    from rspy import librs as rs
    if log.isEnabledFor( logging.DEBUG ):
        rs.log_to_console( rs.log_severity.debug )

    @pytest.fixture(scope='module')
    def remote_and_streaming():
        with test.remote.fork( script=__file__, nested_indent=None ) as remote:
            # Initialize LibRS context
            context = rs.context( {
                'dds': {
                    'enabled': True,
                    'domain': config_file.get_domain_from_config_file_or_default()
                }
            } )

            # Find device and sensors
            devs = rs.wait_for_devices( context, rs.only_sw_devices, n=1 )
            dev = next( (d for d in devs if d.get_info( rs.camera_info.name ) == 'Test OD Device'), None )
            assert dev is not None, 'Test OD Device not found among SW devices'
            sensors = dev.query_sensors()
            assert len( sensors ) == 3
            sensor = next( (s for s in sensors if s.get_info( rs.camera_info.name ) == 'Inference Sensor'), None )
            assert sensor is not None, 'Inference Sensor not found'
            color_sensor = next( (s for s in sensors if s.get_info( rs.camera_info.name ) == 'RGB Camera'), None )
            assert color_sensor is not None, 'RGB Camera sensor not found'

            # Find object-detection stream profile
            profiles = sensor.get_stream_profiles()
            od_profiles = [p for p in profiles if p.stream_type() == rs.stream.object_detection]
            assert len( od_profiles ) == 1
            od_profile = od_profiles[0]
            assert od_profile.fps() == 30

            # Open sensor and start streaming
            sensor.open( [od_profile] )
            queue = rs.frame_queue( 100 )
            sensor.start( queue )

            remote.run( 'start_od_streaming()' )

            try:
                yield remote, sensor, color_sensor, queue
            finally:
                remote.run( 'stop_od_streaming()', on_fail='log' )
                sensor.stop()
                sensor.close()
                del queue
                del sensor
                del color_sensor
                del dev
                del context

    #
    #############################################################################################
    #
    def test_two_detections(remote_and_streaming):
        """Receive frame with two detections and verify fields."""
        remote, _, _, queue = remote_and_streaming
        remote.run( 'publish_two_detections()' )
        f = queue.wait_for_frame( 500 )
        if check.is_true( f, msg='no frame received' ):
            odf = f.as_object_detection_frame()
            if check.is_true( odf, msg='frame is not an object_detection_frame' ):
                check.equal( odf.get_frame_number(), 1 )
                count = odf.get_detection_count()
                if check.equal( count, 2 ):
                    det0 = odf.get_detection( 0 )
                    check.equal( det0.class_id,       0  )
                    check.equal( det0.score,          85 )
                    check.equal( det0.top_left_x,     10 )
                    check.equal( det0.top_left_y,     20 )
                    check.equal( det0.bottom_right_x, 100 )
                    check.equal( det0.bottom_right_y, 200 )

                    det1 = odf.get_detection( 1 )
                    check.equal( det1.class_id,       1  )
                    check.equal( det1.score,          70 )
                    check.equal( det1.top_left_x,     150 )
                    check.equal( det1.top_left_y,     50  )
                    check.equal( det1.bottom_right_x, 300 )
                    check.equal( det1.bottom_right_y, 250 )

    #
    #############################################################################################
    #
    def test_single_detection(remote_and_streaming):
        remote, _, _, queue = remote_and_streaming
        remote.run( 'publish_single_detection()' )
        f = queue.wait_for_frame( 500 )
        if check.is_true( f, msg='no frame received' ):
            odf = f.as_object_detection_frame()
            if check.is_true( odf, msg='frame is not an object_detection_frame' ):
                check.equal( odf.get_frame_number(), 2 )
                if check.equal( odf.get_detection_count(), 1 ):
                    det = odf.get_detection( 0 )
                    check.equal( det.class_id, 0  )
                    check.equal( det.score,    92 )
                    check.equal( det.top_left_x,     50  )
                    check.equal( det.top_left_y,     60  )
                    check.equal( det.bottom_right_x, 200 )
                    check.equal( det.bottom_right_y, 300 )

    #
    #############################################################################################
    #
    def test_zero_detections(remote_and_streaming):
        remote, _, _, queue = remote_and_streaming
        remote.run( 'publish_zero_detections()' )
        f = queue.wait_for_frame( 500 )
        if check.is_true( f, msg='no frame received' ):
            odf = f.as_object_detection_frame()
            if check.is_true( odf, msg='frame is not an object_detection_frame' ):
                check.equal( odf.get_detection_count(), 0 )

    #
    #############################################################################################
    #
    def test_sensor_downcast(remote_and_streaming):
        _, sensor, _, _ = remote_and_streaming
        check.is_true( sensor.is_inference_sensor(), msg='sensor should be an inference_sensor' )
        check.is_true( sensor.is_object_detection_sensor(), msg='sensor should be an object_detection_sensor' )
        inference_s = sensor.as_inference_sensor()
        check.is_true( inference_s, msg='as_inference_sensor() should return truthy' )
        od_s = sensor.as_object_detection_sensor()
        check.is_true( od_s, msg='as_object_detection_sensor() should return truthy' )

    #
    #############################################################################################
    #
    def test_out_of_bounds_detection_index_raises(remote_and_streaming):
        remote, _, _, queue = remote_and_streaming
        remote.run( 'publish_two_detections()' )
        f = queue.wait_for_frame( 500 )
        if check.is_true( f, msg='no frame received' ):
            odf = f.as_object_detection_frame()
            if check.is_true( odf, msg='frame is not an object_detection_frame' ):
                count = odf.get_detection_count()
                if check.equal( count, 2 ):
                    # index == count is out of range; exception surfaces as RuntimeError via C API
                    with check.raises( RuntimeError ):
                        odf.get_detection( count )
                    # large positive index is also out of range
                    with check.raises( RuntimeError ):
                        odf.get_detection( 999 )
                    # negative index: pybind11 rejects before the C++ body runs → TypeError
                    with check.raises( TypeError ):
                        odf.get_detection( -1 )

    #
    #############################################################################################
    #
    def test_downcast_failure_color_sensor(remote_and_streaming):
        """Downcast failure: color sensor is not an inference sensor."""
        _, _, color_sensor, _ = remote_and_streaming
        check.is_true( not color_sensor.is_inference_sensor(),
                       msg='RGB Camera should not be an inference_sensor' )
        check.is_true( not color_sensor.is_object_detection_sensor(),
                       msg='RGB Camera should not be an object_detection_sensor' )
        non_inf = color_sensor.as_inference_sensor()
        check.is_true( not non_inf, msg='as_inference_sensor() on color sensor should return falsy' )
        non_od = color_sensor.as_object_detection_sensor()
        check.is_true( not non_od, msg='as_object_detection_sensor() on color sensor should return falsy' )

    #
    #############################################################################################
    #
    def test_queue_empty_after_draining(remote_and_streaming):
        """Queue is empty after draining all published frames."""
        remote, _, _, queue = remote_and_streaming
        # No new frames published — queue should be empty
        f = queue.poll_for_frame()
        check.is_true( not f, msg='expected no frame in queue but got one' )
