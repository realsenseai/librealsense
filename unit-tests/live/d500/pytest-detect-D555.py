# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# There is currently an issue with D555, sometimes the domain id in the configuration resets to 0.
# We want this test to run first and restore the domain, so other tests will be able to detect the camera.

import pytest
import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy import config_file, devices
from rspy.snippets import is_dds_dev
from time import monotonic, sleep
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.priority(0),
    pytest.mark.device_each("D555"),
    pytest.mark.context("dds"),
]


def _find_d555_dds(ctx):
    try:
        devs = ctx.query_devices()
    except RuntimeError as error:
        log.error("Failed to query devices: %s", error)
        return None
    for dev in devs:
        try:
            name = dev.get_info(rs.camera_info.name) if dev.supports(rs.camera_info.name) else ""
            if "D555" in name and is_dds_dev(dev):
                return dev
        except RuntimeError as error:
            log.error("Failed to inspect device: %s", error)
    return None


def _wait_for_d555_dds(ctx, timeout=devices.MAX_ENUMERATION_TIME):
    end_time = monotonic() + timeout
    while True:
        dev = _find_d555_dds(ctx)
        if dev:
            return dev
        remaining = end_time - monotonic()
        if remaining <= 0:
            return None
        sleep(min(1, remaining))


# Make sure D555 is detected on CI machines (DDS connection)
# To run locally with other devices use `--device` flag
def test_detect_D555(module_device_setup):
    domain_from_config = config_file.get_domain_from_config_file_or_default()
    configured_ctx = rs.context({"dds": {"enabled": True, "domain": domain_from_config}})
    dev = _wait_for_d555_dds(configured_ctx)
    if dev:
        log.debug("Device detected on configured domain %s, no need to restore", domain_from_config)
        return

    assert domain_from_config != 0, "D555 DDS device was not found on configured domain 0"

    # Sometimes, due to a yet unresolved issue, the camera domain resets to 0.
    log.debug("D555 was not found on domain %s; trying domain 0", domain_from_config)
    domain_0_ctx = rs.context({"dds": {"enabled": True, "domain": 0}})
    dev = _wait_for_d555_dds(domain_0_ctx)
    assert dev is not None, "D555 DDS device was not found on domain 0"

    get_eth_config_opcode = 0xBB
    set_eth_config_opcode = 0xBA
    current_values_param = 1
    protocol = rs.debug_protocol(dev)
    raw_command = protocol.build_command(get_eth_config_opcode, current_values_param)
    raw_result = protocol.send_and_receive_raw_data(raw_command)
    assert raw_result and raw_result[0] == get_eth_config_opcode, \
        "Failed to get D555 current configuration"

    config = rsutils.eth_config(raw_result[4:])
    config.dds.domain_id = domain_from_config
    raw_command = protocol.build_command(
        set_eth_config_opcode, 0, 0, 0, 0, config.build_command()
    )
    raw_result = protocol.send_and_receive_raw_data(raw_command)
    assert raw_result and raw_result[0] == set_eth_config_opcode, "Failed to set D555 domain"

    log.info("Successfully restored D555 to domain %s", config.dds.domain_id)
    dev.hardware_reset()
    assert _wait_for_d555_dds(configured_ctx) is not None, \
        "D555 did not reappear on the configured domain after reset"
