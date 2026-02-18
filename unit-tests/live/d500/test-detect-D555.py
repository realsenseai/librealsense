# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# There is currently an issue with D555, sometimes the domain id in the configuration resets to 0.
# Note - currently there is an issue with D555 domain, we want test-detect-D555 to run first, test-fw-update second, test-metadata third, updating priorities
# test:priority 0
# test:device D555
# test:donotrun:!dds

import pyrealsense2 as rs
from rspy import test, log, config_file
import pyrsutils as rsutils
import time

# Make sure D555 is detected on CI machines (DDS connection)
# To run locally with other devices use `--device` flag

if log.is_debug_on():
    rs.log_to_console( rs.log_severity.debug )

dev_found = False

with test.closure( "Detect D555 DDS device" ):
    ctx = rs.context({ "dds" : { "enabled" : True } })
    devs = ctx.query_devices()
    if len(devs) > 0:
        dev = devs[0]
        dev_found = dev.supports(rs.camera_info.connection_type) and dev.get_info(rs.camera_info.connection_type) == "DDS"
    test.check( dev_found )

with test.closure("restore d555 domain if was reset to 0"):
    # Sometime, due to yet unresolved issue, camera domain resets back to 0.
    # All the unit tests expect a certain domain (from config) so we will try to find it and update the domain
    domain_from_config = config_file.get_domain_from_config_file_or_default()
    if not dev_found and domain_from_config != 0: # If device not previously found and domains might differ
        log.d("Domain from configuration is", domain_from_config, " trying to detect device on domain 0")
        ctx = rs.context({ "dds" : { "enabled" : True, "domain" : 0 } })
        devices = ctx.query_devices()
        dev = None
        if devices:
            dev = devices[0]
            
            # Get device configuration
            get_eth_config_opcode = 0xBB
            set_eth_config_opcode = 0xBA
            current_values_param = 1
            raw_command = rs.debug_protocol(dev).build_command(get_eth_config_opcode, current_values_param)
            raw_result = rs.debug_protocol(dev).send_and_receive_raw_data(raw_command)
            if raw_result[0] != get_eth_config_opcode:
                log.e(f'Failed to get D555 current configuration')
            else:
                # Update configuration domain and set to device
                config = rsutils.eth_config( raw_result[4:] )
                config.dds.domain_id = domain_from_config

                raw_command = rs.debug_protocol(dev).build_command(set_eth_config_opcode, 0, 0, 0, 0, config.build_command())
                raw_result = rs.debug_protocol(dev).send_and_receive_raw_data(raw_command)
                if raw_result[0] != set_eth_config_opcode:
                    log.e(f'Failed to set D555 domain')
                else:                
                    log.i('Successfully restored D555 to domain', config.dds.domain_id)
                    dev.hardware_reset()
    else:
        log.d("Device already detected on configured domain", domain_from_config, ", no need to restore")

test.print_results_and_exit()
