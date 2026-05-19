# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#test:donotrun:!dds
#test:retries 2

# Tests object-detection streaming through the LibRS API:
#   - Server creates a device with an object_detection_stream_server and publishes detection JSON
#   - Client uses pyrealsense2 (LibRS) to receive and parse the frames via the object_detection_frame API

from rspy import log, test, config_file
with test.remote.fork( nested_indent=None ) as remote:
    if remote is None:  # we're the fork (server)
        import pyrealdds as dds
        import json as json_module
        dds.debug( log.is_debug_on(), log.nested )

        with test.closure( 'Start server participant' ):
            participant = dds.participant()
            participant.init( config_file.get_domain_from_config_file_or_default(), 'server' )

        with test.closure( 'Create OD device server' ):
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

        with test.closure( 'Broadcast device', on_fail=test.ABORT ):
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

        raise StopIteration()  # quit the server fork


    ###############################################################################################################
    # The client is LibRS
    #
    from rspy import librs as rs
    if log.is_debug_on():
        rs.log_to_console( rs.log_severity.debug )

    with test.closure( 'Initialize LibRS context', on_fail=test.ABORT ):
        context = rs.context( {
            'dds': {
                'enabled': True,
                'domain': config_file.get_domain_from_config_file_or_default(),
                'participant': 'client'
            }
        } )

    with test.closure( 'Find device and sensors', on_fail=test.ABORT ):
        # Other DDS devices (e.g. physical cameras) may appear as SW devices; filter by name
        devs = rs.wait_for_devices( context, rs.only_sw_devices, n=1 )
        dev = next( (d for d in devs if d.get_info( rs.camera_info.name ) == 'Test OD Device'), None )
        test.check( dev is not None, 'Test OD Device not found among SW devices', on_fail=test.RAISE )
        sensors = dev.query_sensors()
        test.check_equal( len( sensors ), 3, on_fail=test.RAISE )
        sensor = next( (s for s in sensors if s.get_info( rs.camera_info.name ) == 'Inference Sensor'), None )
        test.check( sensor is not None, 'Inference Sensor not found', on_fail=test.RAISE )
        color_sensor = next( (s for s in sensors if s.get_info( rs.camera_info.name ) == 'RGB Camera'), None )
        test.check( color_sensor is not None, 'RGB Camera sensor not found', on_fail=test.RAISE )

    with test.closure( 'Find object-detection stream profile', on_fail=test.ABORT ):
        profiles = sensor.get_stream_profiles()
        od_profiles = [p for p in profiles if p.stream_type() == rs.stream.object_detection]
        test.check_equal( len( od_profiles ), 1, on_fail=test.RAISE )
        od_profile = od_profiles[0]
        test.check_equal( od_profile.fps(), 30 )

    with test.closure( 'Open sensor and start streaming', on_fail=test.ABORT ):
        sensor.open( [od_profile] )
        queue = rs.frame_queue( 100 )
        sensor.start( queue )

    remote.run( 'start_od_streaming()' )

    #############################################################################################
    with test.closure( 'Receive frame with two detections and verify fields' ):
        remote.run( 'publish_two_detections()' )
        f = queue.wait_for_frame( 500 )
        if test.check( f, 'no frame received' ):
            odf = f.as_object_detection_frame()
            if test.check( odf, 'frame is not an object_detection_frame' ):
                test.check_equal( odf.get_frame_number(), 1 )
                count = odf.get_detection_count()
                if test.check_equal( count, 2 ):
                    det0 = odf.get_detection( 0 )
                    test.check_equal( det0.class_id,      0  )
                    test.check_equal( det0.score,         85 )
                    test.check_equal( det0.top_left_x,    10 )
                    test.check_equal( det0.top_left_y,    20 )
                    test.check_equal( det0.bottom_right_x, 100 )
                    test.check_equal( det0.bottom_right_y, 200 )

                    det1 = odf.get_detection( 1 )
                    test.check_equal( det1.class_id,      1  )
                    test.check_equal( det1.score,         70 )
                    test.check_equal( det1.top_left_x,    150 )
                    test.check_equal( det1.top_left_y,    50  )
                    test.check_equal( det1.bottom_right_x, 300 )
                    test.check_equal( det1.bottom_right_y, 250 )

    #############################################################################################
    with test.closure( 'Receive frame with single detection' ):
        remote.run( 'publish_single_detection()' )
        f = queue.wait_for_frame( 500 )
        if test.check( f, 'no frame received' ):
            odf = f.as_object_detection_frame()
            if test.check( odf, 'frame is not an object_detection_frame' ):
                test.check_equal( odf.get_frame_number(), 2 )
                if test.check_equal( odf.get_detection_count(), 1 ):
                    det = odf.get_detection( 0 )
                    test.check_equal( det.class_id, 0  )
                    test.check_equal( det.score,    92 )
                    test.check_equal( det.top_left_x,     50  )
                    test.check_equal( det.top_left_y,     60  )
                    test.check_equal( det.bottom_right_x, 200 )
                    test.check_equal( det.bottom_right_y, 300 )

    #############################################################################################
    with test.closure( 'Receive frame with zero detections' ):
        remote.run( 'publish_zero_detections()' )
        f = queue.wait_for_frame( 500 )
        if test.check( f, 'no frame received' ):
            odf = f.as_object_detection_frame()
            if test.check( odf, 'frame is not an object_detection_frame' ):
                test.check_equal( odf.get_detection_count(), 0 )

    #############################################################################################
    with test.closure( 'Sensor downcast checks' ):
        test.check( sensor.is_inference_sensor(), 'sensor should be an inference_sensor' )
        test.check( sensor.is_object_detection_sensor(), 'sensor should be an object_detection_sensor' )
        inference_s = sensor.as_inference_sensor()
        test.check( inference_s, 'as_inference_sensor() should return truthy' )
        od_s = sensor.as_object_detection_sensor()
        test.check( od_s, 'as_object_detection_sensor() should return truthy' )

    #############################################################################################
    with test.closure( 'Out-of-bounds detection index raises IndexError' ):
        remote.run( 'publish_two_detections()' )
        f = queue.wait_for_frame( 500 )
        if test.check( f, 'no frame received' ):
            odf = f.as_object_detection_frame()
            if test.check( odf, 'frame is not an object_detection_frame' ):
                count = odf.get_detection_count()
                if test.check_equal( count, 2 ):
                    # index == count is out of range; exception surfaces as RuntimeError via C API
                    test.check_throws( lambda: odf.get_detection( count ), RuntimeError )
                    # large positive index is also out of range
                    test.check_throws( lambda: odf.get_detection( 999 ),   RuntimeError )
                    # negative index: pybind11 rejects before the C++ body runs → TypeError
                    test.check_throws( lambda: odf.get_detection( -1 ),    TypeError )

    #############################################################################################
    with test.closure( 'Downcast failure: color sensor is not an inference sensor' ):
        test.check( not color_sensor.is_inference_sensor(),
                    'RGB Camera should not be an inference_sensor' )
        test.check( not color_sensor.is_object_detection_sensor(),
                    'RGB Camera should not be an object_detection_sensor' )
        non_inf = color_sensor.as_inference_sensor()
        test.check( not non_inf, 'as_inference_sensor() on color sensor should return falsy' )
        non_od = color_sensor.as_object_detection_sensor()
        test.check( not non_od, 'as_object_detection_sensor() on color sensor should return falsy' )

    #############################################################################################
    with test.closure( 'Queue is empty after draining all published frames' ):
        # No new frames published — queue should be empty
        f = queue.poll_for_frame()
        test.check( not f, 'expected no frame in queue but got one' )

    #############################################################################################
    with test.closure( 'Stop streaming and clean up' ):
        remote.run( 'stop_od_streaming()', on_fail='log' )
        sensor.stop()
        sensor.close()
        del queue
        del sensor
        del color_sensor
        del dev
        del context


test.print_results()
