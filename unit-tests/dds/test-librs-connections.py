# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2022-4 RealSense, Inc. All Rights Reserved.

#test:donotrun:!dds
#test:retries 2

from rspy import log, test, config
log.nested = 'C  '

import d435i
import d405
from rspy import librs as rs
if log.is_debug_on():
    rs.log_to_console( rs.log_severity.debug )
from time import sleep

import os.path
cwd = os.path.dirname(os.path.realpath(__file__))
remote_script = os.path.join( cwd, 'device-broadcaster.py' )
with test.remote( remote_script, nested_indent="  S" ) as remote:
    remote.wait_until_ready()
    #
    #############################################################################################
    #
    with test.closure( "Start two devices", on_fail=test.ABORT ):
        remote.run( 'instance = broadcast_device( d435i, d435i.device_info )' )
        remote.run( 'instance2 = broadcast_device( d405, d405.device_info )' )

        # Create context after remote device is ready to test discovery on start-up
        context = rs.context( {
            'dds': {
                'enabled': True,
                'domain': config.get_domain_from_config_file(),
                'participant': 'librs'
                },
            'device-mask': rs.only_sw_devices
            } )
        # The DDS devices take time to be recognized and we just created the context; we
        # should not see them yet!
        #test.check( len( context.query_devices( rs.only_sw_devices )) != 2 )
        # Wait for them
        rs.wait_for_devices( context, n=2. )
    #
    #############################################################################################
    #
    with test.closure( "Start a third", on_fail=test.ABORT ):
        remote.run( 'instance3 = broadcast_device( d455, d455.device_info )' )
        rs.wait_for_devices( context, n=3. )
    #
    #############################################################################################
    #
    with test.closure( "Close the first", on_fail=test.ABORT ):
        rs._devices_updated.clear()
        remote.run( 'close_server( instance )' )
        remote.run( 'instance = None', timeout=1 )
        rs.wait_for_devices( context, n=2. )
    #
    #############################################################################################
    #
    with test.closure( "Add a fourth", on_fail=test.ABORT ):
        remote.run( 'instance4 = broadcast_device( d435i, d435i.device_info )' )
        rs.wait_for_devices( context, n=3. )
    #
    #############################################################################################
    #
    with test.closure( "Close the second", on_fail=test.ABORT ):
        remote.run( 'close_server( instance2 )' )
        remote.run( 'instance2 = None', timeout=1 )
        rs.wait_for_devices( context, n=2. )
    #
    #############################################################################################
    #
    with test.closure( "Close the third", on_fail=test.ABORT ):
        remote.run( 'close_server( instance3 )' )
        remote.run( 'instance3 = None', timeout=1 )
        rs.wait_for_devices( context, n=1. )
    #
    #############################################################################################
    #
    with test.closure( "Close the last", on_fail=test.ABORT ):
        remote.run( 'close_server( instance4 )' )
        remote.run( 'instance4 = None', timeout=1 )
        rs.wait_for_devices( context, n=0. )
    #
    #############################################################################################

context = None
test.print_results_and_exit()
