# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#test:donotrun:!dds
##test:retries 2

from rspy import log, test, config_file
import pyrealdds as dds
import json
dds.debug( log.is_debug_on() )

device_info = dds.message.device_info()
device_info.topic_root = 'server/device'

with test.remote.fork( nested_indent=None ) as remote:
    if remote is None:  # we're the fork

        with test.closure( 'Start the server participant' ):
            participant = dds.participant()
            participant.init( config_file.get_domain_from_config_file(), 'server' )

        with test.closure( 'Create the server' ):
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

        raise StopIteration()  # exit the 'with' statement


    ###############################################################################################################
    # The client is a device from which we send controls
    #

    with test.closure( 'Start the client participant' ):
        participant = dds.participant()
        participant.init( config_file.get_domain_from_config_file(), 'client' )

    with test.closure( 'Wait for the device' ):
        device_info.name = 'Device1'
        device = dds.device( participant, device_info )
        device.wait_until_ready()

    with test.closure( 'Query embedded filters in the sensor', on_fail=test.RAISE ):
        reply = device.send_control( {
                'id': 'query-filter',
                'name': 'Decimation Filter',
                'stream-name': 'Depth'
            }, True )  # Wait for reply
        test.info( 'reply', reply )
        values = reply.get( 'options' )
        test.check( values )
        test.check_equal( len(values), 2 )

    device = None

test.print_results()
