# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

import pyrealdds as dds
from rspy import log, test, config
import pyrealsense2 as rs

dds.debug( log.is_debug_on(), log.nested )


participant = dds.participant()
participant.init( config.get_domain_from_config_file(), "formats-conversion-server" )

device_info = dds.message.device_info.from_json({
    "name": "formats-conversion-device",
    "topic-root": "root_123",
    "product-line": "D400"
})

# Used to created a device_server per test case, but it currently creates problems when creating a second device while
# the first did not yet close. Changing to one device_server with different sensor per test case.

def create_server():
    stream_servers = []

    # Y8I
    profile = dds.video_stream_profile( 30, dds.video_encoding( "Y8I" ), 1280, 720 )
    stream_server = dds.ir_stream_server( "Y8I-stream", "Y8I-sensor" ) # Stream name is assumed to contain only index after '_', cannot use "Y8I_stream"
    stream_server.init_profiles( [ profile ], 0 )
    stream_servers.append( stream_server )

    # Y12I
    profile = dds.video_stream_profile( 30, dds.video_encoding( "Y12I" ), 1280, 720 )
    stream_server = dds.ir_stream_server( "Y12I-stream", "Y12I-sensor" )
    stream_server.init_profiles( [ profile ], 0 )
    stream_servers.append( stream_server )

    # Y8
    profile = dds.video_stream_profile( 30, dds.video_encoding.y8, 1280, 720 )
    stream_server = dds.ir_stream_server( "Y8-stream", "Y8-sensor" )
    stream_server.init_profiles( [ profile ], 0 )
    stream_servers.append( stream_server )

    # YUYV
    profile = dds.video_stream_profile( 30, dds.video_encoding.yuyv, 1280, 720 )
    stream_server = dds.color_stream_server( "YUYV-stream", "YUYV-sensor" )
    stream_server.init_profiles( [ profile ], 0 )
    stream_servers.append( stream_server )

    # UYVY
    profile = dds.video_stream_profile( 30, dds.video_encoding.uyvy, 1280, 720 )
    stream_server = dds.color_stream_server( "UYVY-stream", "UYVY-sensor" )
    stream_server.init_profiles( [ profile ], 0 )
    stream_servers.append( stream_server )

    # Z16
    profile = dds.video_stream_profile( 30, dds.video_encoding.z16, 1280, 720 )
    stream_server = dds.depth_stream_server( "Z16-stream", "Z16-sensor" )
    stream_server.init_profiles( [ profile ], 0 )
    stream_servers.append( stream_server )

    # Motion
    profile = dds.motion_stream_profile( 30 )
    stream_server = dds.motion_stream_server( "motion-stream", "motion-sensor" )
    stream_server.init_profiles( [ profile ], 0 )
    stream_servers.append( stream_server )

    # multiple motion profiles
    profiles = []
    profiles.append( dds.motion_stream_profile( 63 ) )
    profiles.append( dds.motion_stream_profile( 200 ) )
    profiles.append( dds.motion_stream_profile( 250 ) )
    profiles.append( dds.motion_stream_profile( 400 ) )
    stream_server = dds.motion_stream_server( "multiple-motion-stream", "multiple-motion-sensor" )
    stream_server.init_profiles( profiles, 0 )
    stream_servers.append( stream_server )

    # multiple color profiles
    profiles = []
    profiles.append( dds.video_stream_profile( 5, dds.video_encoding.yuyv, 1280, 720 ) )
    profiles.append( dds.video_stream_profile( 15, dds.video_encoding.yuyv, 1280, 720 ) )
    profiles.append( dds.video_stream_profile( 30, dds.video_encoding.yuyv, 1280, 720 ) )
    stream_server = dds.color_stream_server( "multiple-color-stream", "multiple-color-sensor" )
    stream_server.init_profiles( profiles, 0 )
    stream_servers.append( stream_server )

    # multiple depth profiles
    profiles = []
    profiles.append( dds.video_stream_profile( 5, dds.video_encoding.z16, 1280, 720 ) )
    profiles.append( dds.video_stream_profile( 10, dds.video_encoding.z16, 1280, 720 ) )
    profiles.append( dds.video_stream_profile( 15, dds.video_encoding.z16, 1280, 720 ) )
    profiles.append( dds.video_stream_profile( 20, dds.video_encoding.z16, 1280, 720 ) )
    profiles.append( dds.video_stream_profile( 30, dds.video_encoding.z16, 1280, 720 ) )
    stream_server = dds.depth_stream_server( "multiple-depth-stream", "multiple-depth-sensor" )
    stream_server.init_profiles( profiles, 0 )
    stream_servers.append( stream_server )

    global dev_server
    dev_server = dds.device_server( participant, device_info.topic_root )
    dev_server.init( stream_servers, [], {} )
    dev_server.broadcast( device_info )


def close_server():
    global dev_server
    dev_server = None


# From here down, we're in "interactive" mode (see test-device-init.py)
# ...
