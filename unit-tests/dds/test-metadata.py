# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023-4 RealSense, Inc. All Rights Reserved.

#test:donotrun:!dds
#test:retries 2

# We disable under Linux for now, pending feedback from FastDDS team:
# Having two participants in the same process ("client" and "librs" below) usually works, but in this case the former
# is from pyrealdds and the latter from pyrealsense2. The two somehow interfere so the server doesn't even see the
# latter and we have a problem where the broadcaster does not work.
#test:donotrun:linux

import pyrealdds as dds
from rspy import log, test, config
import d435i


with test.remote.fork( nested_indent='  S' ) as remote:
    if remote is None:  # we're the server fork

        dds.debug( log.is_debug_on(), log.nested )

        participant = dds.participant()
        participant.init( config.get_domain_from_config_file(), "server" )

        # set up a server device with a single color stream
        device_server = dds.device_server( participant, d435i.device_info.topic_root )

        depth_stream = dds.depth_stream_server( 'Depth', 'Depth Module' ) # Depth sensor is expected
        depth_stream.enable_metadata()  # not there in d435i by default
        depth_stream.init_profiles( d435i.depth_stream_profiles(), 0 )
        depth_stream.init_options( [] )

        def on_control( server, id, control, reply ):
            # the control has already been output to debug by the calling code, as will the reply
            return True  # otherwise the control will be flagged as error

        device_server.on_control( on_control )
        device_server.init( [depth_stream], [], {} )


        def broadcast():
            global device_server
            device_server.broadcast( d435i.device_info )


        def new_image( width, height, bpp, timestamp_as_ns = None ):
            i = dds.message.image()
            i.width = width
            i.height = height
            i.data = bytearray( width * height * bpp )
            if timestamp_as_ns is not None:
                i.timestamp = dds.time.from_ns( timestamp_as_ns )
            return i


        def publish_image( img, timestamp ):
            img.timestamp = timestamp
            depth_stream.publish_image( img )


        # From here down, we're in "interactive" mode (see test-metadata.py)
        # ...
        raise StopIteration()


    ###############################################################################################################
    # The client
    #

    from time import sleep
    from rspy.timer import Timer

    dds.debug( log.is_debug_on(), 'C  ' )
    log.nested = 'C  '

    participant = dds.participant()
    participant.init( config.get_domain_from_config_file(), "client" )

    # set up the client device and keep all its streams - this is connected directly and we can get notifications on it!
    device_direct = dds.device( participant, d435i.device_info )
    device_direct.wait_until_ready()
    test.check( device_direct.is_ready(), on_fail=test.ABORT )
    for stream_direct in device_direct.streams():
        pass  # should be only one
    topic_name = 'rt/' + d435i.device_info.topic_root + '_' + stream_direct.name()

    import threading
    image_received = threading.Event()
    image_times = []
    def on_image_available( stream, image_buffer, image_time, sample ):
        log.d( f'----> image time {image_time} {sample}')
        image_times.append( image_time )
        image_received.set()

    stream_direct.on_data_available( on_image_available )
    stream_direct.open( topic_name, dds.subscriber( participant ) )
    stream_direct.start_streaming()

    def detect_image():
        image_times.clear()
        image_received.clear()

    def wait_for_image( timeout=1, count=None ):
        timer = Timer( timeout )
        while not timer.has_expired():
            if not image_received.wait( timer.time_left() ):
                raise TimeoutError( 'timeout waiting for image' )
            if count is None  or  count <= len(image_times):
                return
            image_received.clear()
        if count is None:
            raise TimeoutError( 'timeout waiting for image' )
        raise TimeoutError( f'timeout waiting for {count} images; {len(image_times)} received' )

    class image_expected:
        def __init__( self, timeout=1, count=None ):
            self._timeout = timeout
            self._count = count
        def __enter__( self ):
            detect_image()
        def __exit__( self, type, value, traceback ):
            if type is None:  # If an exception is thrown, don't do anything
                wait_for_image( timeout=self._timeout, count=self._count )


    metadata_received = threading.Event()
    metadata_content = []

    def on_metadata_available( device, md ):
        log.d( f'----> metadata[{len(metadata_content)}]= {md}')
        metadata_content.append( md )
        metadata_received.set()

    metadata_subscription = device_direct.on_metadata_available( on_metadata_available )

    def detect_metadata():
        metadata_content.clear()
        metadata_received.clear()

    def wait_for_metadata( timeout=1, count=None ):
        timer = Timer( timeout )
        while not timer.has_expired():
            if not metadata_received.wait( timer.time_left() ):
                raise TimeoutError( 'timeout waiting for metadata' )
            if count is None  or  count <= len(metadata_content):
                return
            metadata_received.clear()
        if count is None:
            raise TimeoutError( 'timeout waiting for metadata' )
        raise TimeoutError( f'timeout waiting for {count} metadata; {len(metadata_content)} received' )

    class metadata_expected:
        def __init__( self, expected_md=None, timeout=1, count=None ):
            self._md = expected_md
            self._timeout = timeout
            self._count = count
        def __enter__( self ):
            detect_metadata()
        def __exit__( self, type, value, traceback ):
            if type is None:  # If an exception is thrown, don't do anything
                wait_for_metadata( timeout=self._timeout, count=self._count )
                if self._md is not None:
                    if test.check( len(metadata_content), description = 'Expected metadata but got none' ):
                        test.check_equal( metadata_content[0], self._md )


    #############################################################################################
    #
    with test.closure( "No librs syncer; direct from server" ):
        md = { 'stream-name' : 'Depth', 'invalid-metadata' : True }
        with metadata_expected( md ):
            remote.run( f'device_server.publish_metadata( {md} )' )
    #
    #############################################################################################
    #
    with test.closure( "Broadcast the device" ):  # otherwise librs won't see it
        remote.run( 'broadcast()' )
    #
    #############################################################################################
    #
    with test.closure( "Initialize librs device", on_fail=test.ABORT ):
        from rspy import librs as rs
        if log.is_debug_on():
            rs.log_to_console( rs.log_severity.debug )
        context = rs.context( { 'dds': { 'enabled': True, 'domain': config.get_domain_from_config_file(), 'participant': 'librs' }} )
        device = rs.wait_for_devices( context, rs.only_sw_devices, n=1. )
        sensors = device.sensors
        test.check_equal( len(sensors), 1, on_fail=test.RAISE )
        sensor = sensors[0]
        test.check_equal( sensor.get_info( rs.camera_info.name ), 'Depth Module', on_fail=test.RAISE )
        del sensors
        profile = rs.video_stream_profile( sensor.get_stream_profiles()[0] )  # take the first one
        log.d( f'using profile {profile}')
        encoding = dds.video_encoding.from_rs2( profile.format() )
        YUYV_BPP = 2 # the camera is actually sending us in YUYV format, and in LibRS we convert it to profile.format
        remote.run( f'img = new_image( {profile.width()}, {profile.height()}, {YUYV_BPP} )', on_fail='abort' )
        sensor.open( [profile] )
        queue = rs.frame_queue( 100 )
        sensor.start( queue )
    #
    #############################################################################################
    #
    with test.closure( "Metadata alone should not come out" ):
        with metadata_expected( count=20 ):
            for i in range(20):
                md = { 'stream-name' : 'Depth', 'header' : { 'i' : i }, 'metadata' : {} }
                remote.run( f'device_server.publish_metadata( {md} )' )
        sleep( 0.25 )  # plus some extra for librs...
        test.check_false( queue.poll_for_frame() )  # we didn't send any images, shouldn't get any frames!
    #
    #############################################################################################
    #
    with test.closure( 'MD after an image, without frame-number' ):
        timestamp = dds.now()
        remote.run( f'depth_stream.start_streaming( dds.video_encoding( "{encoding}" ), img.width, img.height )' )
        # It will take the image a lot longer to get anywhere than the metadata
        with image_expected():
            remote.run( f'publish_image( img, dds.time.from_ns( {timestamp.to_ns()} ))' )
        sleep( 0.25 )  # plus some extra for librs...
        test.check_false( queue.poll_for_frame() )  # the image should still be pending in the syncer
        with metadata_expected():
            md = {
                'stream-name' : 'Depth',
                'header' : {
                    'timestamp' : timestamp.to_ns()
                    },
                'metadata': {
                    'Temperature' : 0xbaad
                    }
            }
            remote.run( f'device_server.publish_metadata( {md} )' )
        f = queue.wait_for_frame( 250 )  # A frame should now be available
        log.d( '---->', f )
        if test.check( f ) and test.check_equal( f.get_frame_number(), 1 ):     # first frame so far!
            test.check_approx_abs( f.get_timestamp() * 1e-3, image_times[0].to_double(), 1e-6 )  # frames are in ms
            test.check_false( f.supports_frame_metadata( rs.frame_metadata_value.actual_fps ) )
            if test.check( f.supports_frame_metadata( rs.frame_metadata_value.temperature ) ):
                test.check_equal( f.get_frame_metadata( rs.frame_metadata_value.temperature ), 0xbaad )
        test.check_false( queue.poll_for_frame() )  # the image should still be pending in the syncer
    #
    #############################################################################################
    #
    with test.closure( 'Image after MD, with frame-number' ):
        timestamp = dds.now()
        with metadata_expected():
            md = {
                'stream-name' : 'Depth',
                'header' : {
                    'frame-number' : 1234,
                    'timestamp' : timestamp.to_ns()
                    },
                'metadata': {
                    'Temperature' : 0xf00d
                    }
            }
            remote.run( f'device_server.publish_metadata( {md} )' )
        sleep( 0.25 )
        test.check_false( queue.poll_for_frame() )
        with image_expected():
            remote.run( f'publish_image( img, dds.time.from_ns( {timestamp.to_ns()} ))' )
        f = queue.wait_for_frame( 250 )
        log.d( '---->', f )
        if test.check( f ) and test.check_equal( f.get_frame_number(), 1234 ):
            test.check_approx_abs( f.get_timestamp() * 1e-3, image_times[0].to_double(), 1e-6 )  # frames are in ms
            test.check_false( f.supports_frame_metadata( rs.frame_metadata_value.white_balance ) )
            if test.check( f.supports_frame_metadata( rs.frame_metadata_value.temperature ) ):
                test.check_equal( f.get_frame_metadata( rs.frame_metadata_value.temperature ), 0xf00d )
        test.check_false( queue.poll_for_frame() )  # the image should still be pending in the syncer
    #
    #############################################################################################
    #
    with test.closure( "Stop streaming" ):
        remote.run( 'depth_stream.stop_streaming()', on_fail='log' )
    #
    #############################################################################################
    #
    with test.closure( "Metadata without a stream name is ignored" ):
        pass


test.print_results()
