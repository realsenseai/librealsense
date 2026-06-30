# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import re, logging
import pyrealdds as dds
from rspy import config_file
from pytest_check import check

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.dds,
]

dds.debug( logging.getLogger(__name__).isEnabledFor( logging.DEBUG ) )

@pytest.fixture(scope='module')
def server_and_client():
    participant = dds.participant()
    participant.init( config_file.get_domain_from_config_file_or_default(), "test-no-metadata" )

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
    check.is_true( device.is_ready() )

    def on_metadata_available( device, md ):
        log.debug( f'-----> {md}')

    metadata_subscription = device.on_metadata_available( on_metadata_available )
    try:
        yield device_server
    finally:
        del metadata_subscription
        del device
        del device_server
        del participant

def test_publish_metadata_should_be_impossible(server_and_client):
    device_server = server_and_client
    md = { 'stream-name' : 'Color', 'invalid-metadata' : True }
    with pytest.raises( RuntimeError,
            match=re.escape( "device 'realsense/D435I_036522070660' has no stream with enabled metadata" ) ):
        device_server.publish_metadata( md )
