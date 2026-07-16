# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# There is currently an issue with D555, sometimes the domain id in the configuration resets to 0.
# We want this test to run first and restore the domain, so other tests will be able to detect the camera.

import pytest
import pyrealsense2 as rs
import pyrsutils as rsutils
from pytest_check import check
from rspy import config_file, devices
from rspy.snippets import is_dds_dev
from time import sleep
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.priority(0),
    pytest.mark.device_each("D555"),
    pytest.mark.context("dds"),
]


# Make sure D555 is detected on CI machines (DDS connection)
# To run locally with other devices use `--device` flag
def test_detect_D555(module_device_setup):
    ctx = rs.context({"dds": {"enabled": True}})
    devs = ctx.query_devices()
    dev_found = False
    if len(devs) > 0:
        dev = devs[0]
        dev_found = is_dds_dev(dev)
    check.is_true(dev_found)

    # Sometime, due to yet unresolved issue, camera domain resets back to 0.
    # All the unit tests expect a certain domain (from config) so we will try to find it and update the domain
    domain_from_config = config_file.get_domain_from_config_file_or_default()
    if not dev_found and domain_from_config != 0:  # If device not previously found and domains might differ
        log.debug("Domain from configuration is %s trying to detect device on domain 0", domain_from_config)
        ctx = rs.context({"dds": {"enabled": True, "domain": 0}})
        sleep(devices.MAX_ENUMERATION_TIME)  # Patch - when device is on domain 0, it was manually added to devices._device_by_sn and Unify hub did not wait for it to powerup properly.
        domain_0_devs = ctx.query_devices()
        dev = None
        if domain_0_devs:
            dev = domain_0_devs[0]

            # Get device configuration
            get_eth_config_opcode = 0xBB
            set_eth_config_opcode = 0xBA
            current_values_param = 1
            raw_command = rs.debug_protocol(dev).build_command(get_eth_config_opcode, current_values_param)
            raw_result = rs.debug_protocol(dev).send_and_receive_raw_data(raw_command)
            if raw_result[0] != get_eth_config_opcode:
                log.error("Failed to get D555 current configuration")
            else:
                # Update configuration domain and set to device
                config = rsutils.eth_config(raw_result[4:])
                config.dds.domain_id = domain_from_config

                raw_command = rs.debug_protocol(dev).build_command(set_eth_config_opcode, 0, 0, 0, 0, config.build_command())
                raw_result = rs.debug_protocol(dev).send_and_receive_raw_data(raw_command)
                if raw_result[0] != set_eth_config_opcode:
                    log.error("Failed to set D555 domain")
                else:
                    log.info("Successfully restored D555 to domain %s", config.dds.domain_id)
                    dev.hardware_reset()
    else:
        log.debug("Device already detected on configured domain %s, no need to restore", domain_from_config)
