# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

import logging
import pyrealsense2 as rs
from pytest_check import check
from time import time

log = logging.getLogger(__name__)


fps = 30
w = 640
h = 480
bpp = 2  # bytes
pixels = bytearray( b'\x69' * ( w * h * bpp ))  # Dummy data
domain = rs.timestamp_domain.hardware_clock     # For either depth/color

global_frame_number = 0



class device:
    def __init__( self ):
        self._handle = rs.software_device()


class sensor:
    def __init__( self, sensor_name:str, dev:device = None ):
        if dev is None:
            dev = device()
        self._handle = dev._handle.add_sensor( sensor_name )
        self._q = None

    def __enter__( self ):
        return self

    def __exit__( self, *args ):
        self.stop()

    def video_stream( self, stream_name:str, type:rs.stream, format:rs.format ):
        return video_stream( self, stream_name, type, format )

    def start( self, *streams ):
        """
        """
        if self._q is not None:
            raise RuntimeError( 'already started' )
        self._profiles = []
        self._profiles_str = []
        for stream in streams:
            stream._profile = rs.video_stream_profile( self._handle.add_video_stream( stream._handle ))
            self._profiles.append( stream._profile )
            self._profiles_str.append( str(stream._profile) )
        self._q = rs.frame_queue( 100 )
        self._handle.open( self._profiles )
        self._handle.start( self._q )

    def stop( self ):
        """
        """
        if self._q is not None:
            self._handle.stop()
            self._handle.close()
            del self._q

    def set( self, key:rs.frame_metadata_value, value ):
        self._handle.set_metadata( key, value )

    def add_option( self, option:rs.option, option_range, writable=True ):
        self._handle.add_option( option, option_range, writable )

    def supports( self, option:rs.option ):
        return self._handle.supports( option )

    def get_option( self, option:rs.option ):
        return self._handle.get_option( option )

    def set_option( self, option:rs.option, value ):
        self._handle.set_option( option, value )

    def publish( self, frame ):
        if str(frame.profile) not in self._profiles_str:
            raise RuntimeError( f'frame {frame.profile} is not part of sensor {self._profiles}' )
        self._handle.on_video_frame( frame )
        received_frame = self.receive()
        check.equal( received_frame.get_frame_number(), frame.frame_number )
        return received_frame

    def receive( self ):
        """
        Looks at the syncer queue and gets the next frame from it if available, checking its contents
        against the expected frame numbers.
        """
        f = self._q.poll_for_frame()
        # NOTE: f will never be None
        if not f:
            raise RuntimeError( f'expected a frame but got none' )
        log.debug( "Got %s", f )
        return f


class video_stream:
    def __init__( self, sensor:sensor, stream_name:str, type:rs.stream, format:rs.format ):
        self._sensor = sensor
        self._handle = rs.video_stream()
        self._handle.type = type
        self._handle.uid = 0
        self._handle.width = w
        self._handle.height = h
        self._handle.bpp = bpp
        self._handle.fmt = format
        self._handle.fps = fps
    #
    def frame( self, frame_number = None, timestamp = None ):
        f = rs.software_video_frame()
        f.pixels = pixels
        f.stride = w * bpp
        f.bpp = bpp
        if frame_number is not None:
            f.frame_number = frame_number
        else:
            global global_frame_number
            global_frame_number += 1
            f.frame_number = global_frame_number
        f.timestamp = timestamp or time()
        f.domain = domain
        f.profile = self._profile
        return f
