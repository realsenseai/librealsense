# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

import pyrealdds as dds
from rspy import log, test, config_file
import flexible

dds.debug( log.is_debug_on(), log.nested )


participant = dds.participant()
participant.init( config_file.get_domain_from_config_file(), "streaming-server" )


# From here down, we're in "interactive" mode (see test-device-init.py)
# ...
