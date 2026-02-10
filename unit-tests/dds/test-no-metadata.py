# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#test:donotrun:!dds

import pyrealdds as dds
from rspy import log, test, config
dds.debug( log.is_debug_on() )

participant = dds.participant()
participant.init( config.get_domain_from_config_file(), "test-no-metadata" )

# set up a server device
import d435i
device_server = dds.device_server( participant, d435i.device_info.topic_root )
color_stream = dds.color_stream_server( "Color",  "RGB Camera" )
#color_stream.enable_metadata()  # not there in d435i by default
color_stream.init_profiles( d435i.color_stream_profiles(), 0 )
color_stream.init_options( [] )
color_stream.set_intrinsics( d435i.color_stream_intrinsics() )
device_server.init( [color_stream], [], {} )

# set up the client device and keep all its streams
device = dds.device( participant, d435i.device_info )
device.wait_until_ready()  # this will throw if something's wrong
test.check( device.is_ready() )

def on_metadata_available( device, md ):
    log.d( f'-----> {md}')

metadata_subscription = device.on_metadata_available( on_metadata_available )


#############################################################################################
with test.closure( "publish_metadata should be impossible" ):
    md = { 'stream-name' : 'Color', 'invalid-metadata' : True }
    test.check_throws( lambda:
        device_server.publish_metadata( md ),
        RuntimeError, "device 'realsense/D435I_036522070660' has no stream with enabled metadata" )


#############################################################################################
test.print_results_and_exit()
