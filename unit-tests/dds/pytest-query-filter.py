# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import logging
import json
import pyrealdds as dds
from rspy import test, config_file
import rspy.log
from pytest_check import check

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.dds,
]

dds.debug( log.isEnabledFor( logging.DEBUG ) )

device_info = dds.message.device_info()
device_info.topic_root = 'server/device'

if rspy.log.nested is not None:
    # Start the server participant
    participant = dds.participant()
    participant.init( config_file.get_domain_from_config_file_or_default(), 'server' )

    # Create the server
    device_info.name = 'Some device'
    s1 = dds.depth_stream_server( 'Depth', 'sensor' )
    s1.init_profiles( [
        dds.video_stream_profile( 27, dds.video_encoding.z16, 100, 100 )
        ], 0 )

    decimation_json_str = """
    {
        "name": "Decimation Filter",
        "options": [
            ["Toggle", 0, 0, 1, 1, 0, "Activate filter: 0:disable filter, 1:enable filter", ["int"]],
            ["Magnitude", 2, 1, 8, 1, 2, "How many pixels will be grouped into 1", ["int", "read-only"]]
        ],
        "stream-name": "Depth"
    }
    """
    decimation_json = json.loads( decimation_json_str )
    s1.init_embedded_filters( [
        dds.decimation_embedded_filter.from_json( decimation_json)
        ])
    server = dds.device_server( participant, device_info.topic_root )
    server.init( [s1], [
        dds.option.from_json( ['IP Address', '1.2.3.4', None, 'IP', ['optional', 'IPv4']] )
        ], {} )

else:
    @pytest.fixture(scope='module')
    def remote_server():
        with test.remote.fork( script=__file__, nested_indent=None ) as remote:
            yield remote

    ###############################################################################################################
    # The client is a device from which we send controls
    #

    def test_query_embedded_filters_in_sensor(remote_server):
        # Start the client participant
        participant = dds.participant()
        participant.init( config_file.get_domain_from_config_file_or_default(), 'client' )

        # Wait for the device
        device_info.name = 'Device1'
        device = dds.device( participant, device_info )
        device.wait_until_ready()

        # Query embedded filters in the sensor
        reply = device.send_control( {
                'id': 'query-filter',
                'name': 'Decimation Filter',
                'stream-name': 'Depth'
            }, True )  # Wait for reply
        log.info( f'reply: {reply}' )
        values = reply.get( 'options' )
        check.is_true( values )
        check.equal( len(values), 2 )

        device = None
