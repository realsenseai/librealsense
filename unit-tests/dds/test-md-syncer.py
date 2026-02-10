# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#test:donotrun:!dds
#test:retries 2

import pyrealdds as dds
from rspy import log, test, config
from time import sleep
from rspy.stopwatch import Stopwatch
import threading

dds.debug( log.is_debug_on() )


participant = dds.participant()
participant.init( config.get_domain_from_config_file(), "client" )


def image_id( image ):
    """
    We use timestamps as keys for the syncer.
    To make them readable, approachable, and simple, we simply encode our "counter" (0-x) as the
    timestamp seconds.
    """
    return image.timestamp.seconds

def md_id( metadata ):
    """
    Same for metadata, which is expressed as a float X.Y; our Y is always 0 and X is the counter
    """
    return int(metadata['timestamp'])

def time_stamp( id ):
    """
    Opposite of image_id().
    """
    return dds.time.from_double( id )


from collections import namedtuple
frame = namedtuple( 'frame', 'image md' )
received_frames = []

def on_frame_ready( image, metadata ):
    log.d( f'{image_id(image):-<4}> {dds.now()} [{threading.get_native_id()}] frame ready: {image=} {metadata=}' )
    test.check( last_image() is None  or  image_id( last_image() ) < image_id( image ) )
    received_frames.append( frame( image, metadata ))

def last_frame():
    if len(received_frames):
        return received_frames[-1]
    return None

def last_image():
   f = last_frame()
   return f and f.image

def last_metadata():
   f = last_frame()
   return f and f.md

dropped_metadata = []
def on_metadata_dropped( key, metadata ):
    log.d( f' drop {dds.now()} [{threading.get_native_id()}] metadata dropped: {key=} {metadata=}' )
    dropped_metadata.append( key )


def new_syncer( on_frame_ready=on_frame_ready, on_metadata_dropped=on_metadata_dropped ):
    syncer = dds.metadata_syncer()
    syncer.on_frame_ready( on_frame_ready )
    syncer.on_metadata_dropped( on_metadata_dropped )
    global dropped_metadata, received_frames
    dropped_metadata = []
    received_frames = []
    return syncer


def new_image_( id, width, height, bpp, timestamp=None ):
    i = dds.message.image()
    i.width = width
    i.step = width * bpp
    i.height = height
    i.data = bytearray( width * height * bpp )
    i.timestamp = timestamp or time_stamp( id )
    return i
def new_image( id, timestamp=None ):
    return new_image_( id, 2, 2, 3, timestamp )


def new_metadata( id, timestamp=None ):
    timestamp = timestamp or time_stamp( id )
    md = {
        'timestamp' : timestamp.to_double(),
    }
    return md


with test.closure( 'Enqueue 1 image -> nothing should come out' ):
    syncer = new_syncer()
    syncer.enqueue_frame( 1, new_image( 1 ) )
    test.check_equal( last_frame(), None )
    test.check_equal( len(dropped_metadata), 0 )

with test.closure( 'Enqueue 1 metadata -> nothing should come out' ):
    syncer = new_syncer()
    syncer.enqueue_metadata( 1, new_metadata( 1 ) )
    test.check_equal( last_frame(), None )
    test.check_equal( len(dropped_metadata), 0 )

with test.closure( 'Enqueue many metadata -> nothing out; should get drops' ):
    syncer = new_syncer()
    for i in range( dds.metadata_syncer.max_md_queue_size*2 ):
        syncer.enqueue_metadata( i, new_metadata( i ) )
    test.check_equal( last_frame(), None )
    test.check_equal( len(dropped_metadata), dds.metadata_syncer.max_md_queue_size )

with test.closure( 'Enqueue many images -> some out (no md); no drops' ):
    syncer = new_syncer()
    for i in range( dds.metadata_syncer.max_frame_queue_size*3 ):
        syncer.enqueue_frame( i, new_image( i ) )
    if test.check_equal( len(received_frames), i - dds.metadata_syncer.max_frame_queue_size + 1 ):
        # Some images are still queued in the syncer
        test.check_equal( image_id( last_image() ), i - dds.metadata_syncer.max_frame_queue_size )
        test.check_equal( last_metadata(), None )  # Note: not {}, but None!

with test.closure( 'Enqueue image+metadata -> out' ):
    syncer = new_syncer()
    syncer.enqueue_frame( 1, new_image( 1 ) )
    test.check_equal( last_frame(), None )
    test.check_equal( len(dropped_metadata), 0 )
    syncer.enqueue_metadata( 1, new_metadata( 1 ) )
    if test.check_equal( len(received_frames), 1 ):
        test.check_equal( image_id( last_image() ), 1 )
        test.check_equal( md_id( last_metadata() ), 1 )
    test.check_equal( len(dropped_metadata), 0 )

with test.closure( 'Enqueue metadata+image -> out' ):
    syncer = new_syncer()
    syncer.enqueue_metadata( 1, new_metadata( 1 ) )
    test.check_equal( last_frame(), None )
    test.check_equal( len(dropped_metadata), 0 )
    syncer.enqueue_frame( 1, new_image( 1 ) )
    if test.check_equal( len(received_frames), 1 ):
        test.check_equal( image_id( last_image() ), 1 )
        test.check_equal( md_id( last_metadata() ), 1 )
    test.check_equal( len(dropped_metadata), 0 )

with test.closure( 'Enqueue 1 image after some metadata -> drops + match' ):
    syncer = new_syncer()
    for i in range(4):
        syncer.enqueue_metadata( i, new_metadata( i ) )
    test.check_equal( last_frame(), None )
    test.check_equal( len(dropped_metadata), 0 )
    syncer.enqueue_frame( 2, new_image( 2 ) )
    if test.check_equal( len(received_frames), 1 ):
        test.check_equal( image_id( last_image() ), 2 )
        test.check_equal( md_id( last_metadata() ), 2 )
    test.check_equal( len(dropped_metadata), 2 )  # 0 and 1

with test.closure( 'Enqueue 1 image then earlier metadata -> nothing out; metadata is dropped' ):
    syncer = new_syncer()
    syncer.enqueue_frame( 1, new_image( 1 ) )
    test.check_equal( last_frame(), None )
    test.check_equal( len(dropped_metadata), 0 )
    syncer.enqueue_metadata( 0, new_metadata( 0 ) )
    test.check_equal( last_frame(), None )
    test.check_equal( len(dropped_metadata), 1 )  # not going to be any frame for it

with test.closure( 'Enqueue 1 image then later metadata -> image out w/o md' ):
    syncer = new_syncer()
    syncer.enqueue_frame( 1, new_image( 1 ) )
    test.check_equal( last_frame(), None )
    test.check_equal( len(dropped_metadata), 0 )
    syncer.enqueue_metadata( 2, new_metadata( 2 ) )
    if test.check( len(received_frames), 1 ):
        test.check_equal( image_id( last_image() ), 1 )
        test.check_equal( last_metadata(), None )
    test.check_equal( len(dropped_metadata), 0 )

with test.closure( 'Enqueue 2 images then matching metadata for the second -> both out' ):
    syncer = new_syncer()
    syncer.enqueue_frame( 1, new_image( 1 ) )
    test.check_equal( last_frame(), None )
    test.check_equal( len(dropped_metadata), 0 )
    syncer.enqueue_frame( 2, new_image( 2 ) )
    test.check_equal( last_frame(), None )
    test.check_equal( len(dropped_metadata), 0 )
    syncer.enqueue_metadata( 2, new_metadata( 2 ) )
    if test.check_equal( len(received_frames), 2 ):
        test.check_equal( image_id( last_image() ), 2 )
        test.check_equal( md_id( last_metadata() ), 2 )
    test.check_equal( len(dropped_metadata), 0 )

with test.closure( 'Enqueue during callback' ):
    def enqueue_during_callback( image, metadata ):
        on_frame_ready( image, metadata )
        id = image_id( image ) + 1
        syncer.enqueue_frame( id, new_image( id ))
        # Above used to generate a 'RuntimeError: device or resource busy' so the 2nd frame didn't
        # come out - that's what we're testing: the syncer shouldn't be locked inside the callback!
    syncer = new_syncer( on_frame_ready=enqueue_during_callback )
    syncer.enqueue_frame( 1, new_image( 1 ) )
    test.check_equal( last_frame(), None )
    test.check_equal( len(dropped_metadata), 0 )
    syncer.enqueue_metadata( 1, new_metadata( 1 ) )
    if test.check( last_frame() ):
        test.check_equal( image_id( last_image() ), 1 )
        test.check_equal( md_id( last_metadata() ), 1 )
        test.check_equal( len(dropped_metadata), 0 )
        # If the enqueue succeeded, a matching metadata will generate a callback
        syncer.enqueue_metadata( 2, new_metadata( 2 ) )
        test.check_equal( image_id( last_image() ), 2 )
        test.check_equal( md_id( last_metadata() ), 2 )
        test.check_equal( len(dropped_metadata), 0 )


def two_threads( callback_time, time_between_frames=0.033, n=10, order='metadata-first' ):
    """
    Generate frames periodically. Enqueue images and metadata from two separate threads just as in
    a DDS client (thread per reader).

    :param order: either 'image-first' or 'metadata-first'; signifies the order in which we want to
                  receive messages, to try and control which thread generates the image-metadata
                  match & callback

    Note that, in the client, it's likely that the metadata is received first because it's a much-
    smaller message. I.e., the MD will be received, cached, and when the image arrives the match
    will be made and the callback will be called from the image thread. While this shouldn't block
    the syncer, it does block the image-receiving thread so, if the callback takes long enough,
    we'll see additional metadata being received and cached. If we specify 'image-first' then the
    opposite happens: the MD thread will block and we'll see images arriving during the callback,
    meaning images will build up and eventually being freed without metadata meaning the callback
    will be called from two different threads, possibly concurrently!
    """
    have_image = threading.Event()
    have_md = threading.Event()
    have_image.clear()
    have_md.clear()
    images = []
    mds = []

    def frame_handler( image, metadata ):
        # This is our callback
        nonlocal callback_time
        on_frame_ready( image, metadata )  # for reporting
        sleep( callback_time )
        log.d( f'<{image_id(image):->4} {dds.now()} [{threading.get_native_id()}]' )

    syncer = new_syncer( on_frame_ready=frame_handler )

    def generator_thread():
        # This thread generates the frames
        nonlocal time_between_frames, n
        threadid = threading.get_native_id()
        for i in range(n):
            timestamp = time_stamp( i )
            log.d( f'{i:>5} {dds.now()} [{threadid}] generate @ {timestamp}' )
            image = new_image( i, timestamp )
            md = new_metadata( i, timestamp )
            images.append( image )
            mds.append( md )
            have_image.set()
            have_md.set()
            sleep( time_between_frames )

    metadata_first = order == 'metadata-first'
    image_first = order == 'image-first'

    def image_thread():
        # This thread enqueues the images
        threadid = threading.get_native_id()
        nonlocal syncer, n, metadata_first
        for i in range(n):
            have_image.wait()
            image = images.pop( 0 )
            if not images:
                have_image.clear()
            if metadata_first:
                sleep( 0.005 )  # delay a bit, to try and control the narrative
            idstr = f'i{image_id(image)}'
            log.d( f'{idstr:>5} {dds.now()} [{threadid}] enqueue {image}' )
            syncer.enqueue_frame( i, image )

    def md_thread():
        # This thread enqueues the metadata
        threadid = threading.get_native_id()
        nonlocal syncer, n, image_first
        for i in range(n):
            have_md.wait()
            md = mds.pop( 0 )
            if not mds:
                have_md.clear()
            if image_first:
                sleep( 0.005 )  # delay a bit, to try and control the narrative
            idstr = f'm{md_id(md)}'
            log.d( f'{idstr:>5} {dds.now()} [{threadid}] enqueue {md}' )
            syncer.enqueue_metadata( i, md )

    f_th = threading.Thread( target=image_thread )
    m_th = threading.Thread( target=md_thread )

    if metadata_first:
        m_th.start()
        f_th.start()
    else:
        f_th.start()
        m_th.start()
    generator_thread()
    f_th.join()
    m_th.join()


with test.closure( 'Two threads, slow callback on MD thread -> parallel callbacks; MD doesn\'t arrive in time -> drops' ):

    two_threads( callback_time=0.1,  # longer than time-between-frames, on purpose!
                 order='image-first' )

    if test.check_equal( len(received_frames), 10 ):
        test.check_equal( image_id( last_image() ), 9 )
        test.check_equal( md_id( last_metadata() ), 9 )
    test.check( len(dropped_metadata) > 0 )

with test.closure( 'Two threads, slow callback on image thread -> metadata builds up with eventual drops' ):

    two_threads( callback_time=0.1,  # longer than time-between-frames, on purpose!
                 n=dds.metadata_syncer.max_md_queue_size*2,
                 order='metadata-first' )

    if test.check_equal( len(received_frames), dds.metadata_syncer.max_md_queue_size*2 ):
        test.check_equal( image_id( last_image() ), dds.metadata_syncer.max_md_queue_size*2 - 1 )
        test.check_equal( md_id( last_metadata() ), dds.metadata_syncer.max_md_queue_size*2 - 1 )
    test.check( len(dropped_metadata) > 0 )

with test.closure( 'Two threads, fast callback -> no drops, no parallel callbacks' ):

    two_threads( callback_time=0.01 )  # shorter than time-between-frames

    if test.check_equal( len(received_frames), 10 ):
        test.check_equal( image_id( last_image() ), 9 )
        test.check_equal( md_id( last_metadata() ), 9 )
    test.check_equal( len(dropped_metadata), 0 )

with test.closure( 'Two threads, slow callback -> different callbacks interleaved' ):
    """
    Test correct handling of interleaved callbacks (incorrect handling can trigger an exception)
    Based on machine and scheduler several scenarios that test this can happen:
    Scenario 1:
        Thread A enqueues frame0
        Thread A enqueues frame1
        Thread B enqueues metadata1
            handle_frame_without_metadata( frame0 ) calls user callback in thread B context
        while the callback is handled thread A enqueues frame2
            handle_match( frame1, metadata1 ) calls user callback in thread A context
    Scenario 2:
        Thread A enqueues frame0
        Thread B enqueues metadata1
            handle_frame_without_metadata( frame0 ) calls user callback in thread B context
        while the callback is handled thread A enqueues frame1
            handle_match( frame1, metadata1 ) calls user callback in thread A context
        Thread A enqueues frame2
    Scenario 3:
        Thread A enqueues frame0
        Thread A enqueues frame1
        Thread A enqueues frame2
            handle_frame_without_metadata( frame0 ) calls user callback in thread A context
        while the callback is handled thread B enqueues metadata1
            handle_match( frame1, metadata1 ) calls user callback in thread B context
            
    There is a small chance that frame1 will be appended to received_frames before frame0 so remove checks that depend on order.
    """

    def frame_callback( image, metadata ):
        # Not using on_frame_ready function to avoid last_image < image_id check defined there.
        log.d( f'{image_id(image):-<4}> {dds.now()} [{threading.get_native_id()}] frame ready: {image=} {metadata=}' )
        received_frames.append( frame( image, metadata ))
        sleep( 0.1 )
        log.d( f'<{image_id(image):->4} {dds.now()} [{threading.get_native_id()}]' )

    syncer = new_syncer( on_frame_ready=frame_callback )

    def frame_thread():
        threadid = threading.get_native_id()
        for i in range( 3 ):
            image = new_image( i, time_stamp( i ) )
            idstr = f'i{image_id(image)}'
            log.d( f'{idstr:>5} {dds.now()} [{threadid}] enqueue {image}' )
            syncer.enqueue_frame( i, image )
            sleep( 0.05 )

    threadA = threading.Thread( target=frame_thread )
    threadA.start()
    sleep( 0.12 ) # Between 2nd and 3rd enqueue_frame
    md = new_metadata( 1, time_stamp( 1 ) )
    syncer.enqueue_metadata( 1, md )
    threadA.join()

    test.check_equal( len(received_frames), 2 )
    test.check_equal( len(dropped_metadata), 0 )


test.print_results_and_exit()
