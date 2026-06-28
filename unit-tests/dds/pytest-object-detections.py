# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Tests object-detection streaming at the realdds level:
#   - Server creates a device with an object_detection_stream_server and publishes detection JSON
#   - Client connects directly via pyrealdds, receives the string data, and verifies all fields

import pytest
import logging
import threading
import json as json_module
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

info = dds.message.device_info()
info.name = "Test OD Device"
info.topic_root = "realsense/test-object-detections"

DETECTIONS_JSON = {
    "frame_id": 1,
    "number_of_detections": 2,
    "detections": [
        { "class_id": 0, "confidence": 85, "x1": 10,  "y1": 20,  "x2": 100, "y2": 200, "distance": 1.5  },
        { "class_id": 1, "confidence": 70, "x1": 150, "y1": 50,  "x2": 300, "y2": 250, "distance": 2.3  }
    ],
    "source_frame_id": 42,
    "version": 1
}

ZERO_DETECTIONS_JSON = {
    "frame_id": 2,
    "number_of_detections": 0,
    "detections": [],
    "source_frame_id": 43,
    "version": 1
}

if rspy.log.nested is not None:
    participant = dds.participant()
    participant.init( config_file.get_domain_from_config_file_or_default(), 'server' )

    od = dds.object_detection_stream_server( 'Object Detection', 'Inference Sensor' )
    od.init_profiles( [dds.inference_stream_profile( 30 )], 0 )
    od.init_options( [] )

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

    server = dds.device_server( participant, info.topic_root )
    server.init( [od, color], [], {} )
    server.broadcast( info )

    def start_od_streaming():
        od.start_streaming()

    def stop_od_streaming():
        od.stop_streaming()

    def publish_detection():
        od.publish_inference( json_module.dumps( DETECTIONS_JSON ) )

    def publish_zero_detections():
        od.publish_inference( json_module.dumps( ZERO_DETECTIONS_JSON ) )

else:
    ###############################################################################################################
    # Client
    #
    log.nested = 'C  '

    from rspy.timer import Timer

    dds.debug( log.isEnabledFor( logging.DEBUG ), 'C  ' )

    @pytest.fixture(scope='module')
    def remote_and_streams():
        with test.remote.fork( script=__file__, nested_indent=None ) as remote:
            participant = dds.participant()
            participant.init( config_file.get_domain_from_config_file_or_default(), 'client' )

            device = dds.device( participant, info )
            device.wait_until_ready()
            check.is_true( device.is_ready() )

            assert device.n_streams() == 2
            od_stream = None
            color_stream = None
            for s in device.streams():
                if s.type_string() == 'object_detection':
                    od_stream = s
                elif s.type_string() == 'color':
                    color_stream = s
            assert od_stream is not None, 'no object_detection stream found'
            assert color_stream is not None, 'no color stream found'
            assert color_stream.type_string() != od_stream.type_string(), 'color stream type should differ from object_detection'

            received_event = threading.Event()
            received_data = []

            def on_detection_data( stream, json_str, sample ):
                received_data.append( json_module.loads( json_str ) )
                received_event.set()

            od_stream.on_data_available( on_detection_data )
            topic_name = 'rt/' + info.topic_root + '_' + od_stream.name()
            od_stream.open( topic_name, dds.subscriber( participant ) )
            od_stream.start_streaming()
            remote.run( 'start_od_streaming()' )

            try:
                yield remote, received_event, received_data
            finally:
                remote.run( 'stop_od_streaming()' )
                od_stream.stop_streaming()
                od_stream.close()
                device = None
                participant = None

    #
    #############################################################################################
    #
    def test_receive_detection_frame_verify_fields(remote_and_streams):
        remote, received_event, received_data = remote_and_streams
        received_event.clear()
        received_data.clear()
        remote.run( 'publish_detection()' )
        if check.is_true( received_event.wait( 1.0 ), msg='timeout waiting for detection frame' ):
            d = received_data[0]
            check.equal( d['number_of_detections'], 2 )
            check.equal( d['source_frame_id'], 42 )
            check.equal( d['frame_id'], 1 )
            dets = d['detections']
            if check.equal( len( dets ), 2 ):
                # First detection
                check.equal( dets[0]['class_id'],   0  )
                check.equal( dets[0]['confidence'], 85 )
                check.equal( dets[0]['x1'],         10 )
                check.equal( dets[0]['y1'],         20 )
                check.equal( dets[0]['x2'],         100 )
                check.equal( dets[0]['y2'],         200 )
                check.almost_equal( dets[0]['distance'], 1.5, abs=0.001 )
                # Second detection
                check.equal( dets[1]['class_id'],   1  )
                check.equal( dets[1]['confidence'], 70 )
                check.equal( dets[1]['x1'],         150 )
                check.equal( dets[1]['y1'],         50  )
                check.equal( dets[1]['x2'],         300 )
                check.equal( dets[1]['y2'],         250 )
                check.almost_equal( dets[1]['distance'], 2.3, abs=0.001 )

    #
    #############################################################################################
    #
    def test_zero_detection_frame_transmitted_correctly(remote_and_streams):
        remote, received_event, received_data = remote_and_streams
        received_event.clear()
        received_data.clear()
        remote.run( 'publish_zero_detections()' )
        if check.is_true( received_event.wait( 1.0 ), msg='timeout waiting for zero-detection frame' ):
            d = received_data[0]
            check.equal( d['number_of_detections'], 0 )
            check.equal( len( d['detections'] ), 0 )

    #
    #############################################################################################
    #
    def test_out_of_bounds_access_raises_index_error(remote_and_streams):
        remote, received_event, received_data = remote_and_streams
        received_event.clear()
        received_data.clear()
        remote.run( 'publish_detection()' )
        if check.is_true( received_event.wait( 1.0 ), msg='timeout waiting for detection frame' ):
            dets = received_data[0]['detections']
            count = len( dets )
            if check.equal( count, 2 ):
                # index == count is out of range
                check.raises( IndexError, lambda: dets[count] )
                # large positive index is also out of range
                check.raises( IndexError, lambda: dets[999] )
                # Python treats negative indices as wrap-around, but -3 exceeds a 2-element list
                check.raises( IndexError, lambda: dets[-3] )
