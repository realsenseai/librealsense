# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

import pyrealdds as server
from rspy import log, test, config_file


server.debug( log.is_debug_on(), log.nested )


#############################################################################################
#
test.start( "participant-init" )

participant = server.participant()
test.check( not participant )

participant.init( config_file.get_domain_from_config_file_or_default(), "test-participant-server" )

test.check( participant )
test.check( participant.is_valid() )

test.finish()
#
#############################################################################################
test.print_results_and_exit()
