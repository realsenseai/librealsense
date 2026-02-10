# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

import pyrealdds as dds
from rspy import log, test, config_file
import d435i
import d405
import d455

dds.debug( log.is_debug_on(), log.nested )


participant = dds.participant()
participant.init( config_file.get_domain_from_config_file(), "device-broadcaster" )

# These are the servers currently broadcast
servers = dict()


def broadcast_device( camera, device_info ):
    """
    E.g.:
        instance = broadcast_device( d435i, d435i.device_info )
    """
    global servers
    instance = device_info.serial
    if not instance:
        raise RuntimeError( "serial-number must be filled out" )
    server = camera.build( participant )
    servers[instance] = {
        'info' : device_info,
        'server' : server
        }
    server.broadcast( device_info )
    return instance


def close_server( instance ):
    """
    Close the instance returned by broadcast_device()
    """
    global servers
    server = servers[instance]["server"]  # throws if does not exist
    server.broadcast_disconnect()
    del servers[instance]


# From here down, we're in "interactive" mode (see test-device-init.py)
# ...
