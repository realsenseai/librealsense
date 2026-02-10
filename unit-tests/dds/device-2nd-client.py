# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

import pyrealdds as dds
from rspy import log, test, config

dds.debug( log.is_debug_on(), log.nested )


participant = dds.participant()
# in some cases we need a bigger buffer for our messages, to avoid missing messages sent
settings = { "device" :
                 { "notification" :
                       { "history" :
                             { "depth" : 60 }
                         }
                   }
             }
participant.init( config.get_domain_from_config_file(), f'client-{log.nested.strip()}', settings )


info = dds.message.device_info()
info.name = "2nd Device"
info.topic_root = "realdds/device/topic-root"


def test_second_device():
    global device
    device = dds.device( participant, info )
    device.wait_until_ready()  # If no device is available before timeout, this will throw

def close_device():
    global device
    device = None


# From here down, we're in "interactive" mode (see test-device-init.py)
# ...
