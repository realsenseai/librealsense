# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import logging
from rspy import test, config_file
import d435i
import d405
from rspy import librs as rs
from time import sleep
import os.path

log = logging.getLogger(__name__)
log.nested = 'C  '

pytestmark = [
    pytest.mark.dds,
    pytest.mark.flaky( retries=2 ),
]

if log.isEnabledFor( logging.DEBUG ):
    rs.log_to_console( rs.log_severity.debug )

cwd = os.path.dirname(os.path.realpath(__file__))
remote_script = os.path.join( cwd, 'device-broadcaster.py' )

@pytest.fixture(scope='module')
def remote():
    with test.remote( remote_script, nested_indent="  S" ) as r:
        r.wait_until_ready()
        yield r

def test_connections( remote ):
    #
    #############################################################################################
    #
    # Start two devices
    remote.run( 'instance = broadcast_device( d435i, d435i.device_info )' )
    remote.run( 'instance2 = broadcast_device( d405, d405.device_info )' )

    # Create context after remote device is ready to test discovery on start-up
    context = rs.context( {
        'dds': {
            'enabled': True,
            'domain': config_file.get_domain_from_config_file_or_default()
            },
        'device-mask': rs.only_sw_devices
        } )
    # The DDS devices take time to be recognized and we just created the context; we
    # should not see them yet!
    # Wait for them
    rs.wait_for_devices( context, n=2. )
    #
    #############################################################################################
    #
    # Start a third
    remote.run( 'instance3 = broadcast_device( d455, d455.device_info )' )
    rs.wait_for_devices( context, n=3. )
    #
    #############################################################################################
    #
    # Close the first
    rs._devices_updated.clear()
    remote.run( 'close_server( instance )' )
    remote.run( 'instance = None', timeout=1 )
    rs.wait_for_devices( context, n=2. )
    #
    #############################################################################################
    #
    # Add a fourth
    remote.run( 'instance4 = broadcast_device( d435i, d435i.device_info )' )
    rs.wait_for_devices( context, n=3. )
    #
    #############################################################################################
    #
    # Close the second
    remote.run( 'close_server( instance2 )' )
    remote.run( 'instance2 = None', timeout=1 )
    rs.wait_for_devices( context, n=2. )
    #
    #############################################################################################
    #
    # Close the third
    remote.run( 'close_server( instance3 )' )
    remote.run( 'instance3 = None', timeout=1 )
    rs.wait_for_devices( context, n=1. )
    #
    #############################################################################################
    #
    # Close the last
    remote.run( 'close_server( instance4 )' )
    remote.run( 'instance4 = None', timeout=1 )
    rs.wait_for_devices( context, n=0. )
    #
    #############################################################################################

    context = None
