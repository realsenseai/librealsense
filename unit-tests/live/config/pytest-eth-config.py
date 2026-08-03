# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense Inc. All Rights Reserved.

# Currently only D555 supports DDS configuration natively

import pytest
import pyrealsense2 as rs
import pyrsutils as rsutils
from pytest_check import check
import time
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D555"), # Currently only D555 supports DDS configuration natively
    pytest.mark.priority(2), # Run early (before streaming/fps tests) so the teardown heal lands first
]

get_eth_config_opcode = 0xBB
set_eth_config_opcode = 0xBA

default_values_param = 0
current_values_param = 1

# Safe in-range link timeout values used by the toggle below: both within the 2000-30000 and divisible by 100 as validated by eth_config::validate()
LINK_TIMEOUT_BASELINE = 8000
LINK_TIMEOUT_ALT      = 10000

    
def get_eth_config(dev, get_default_config=False):
    raw_command = rs.debug_protocol(dev).build_command(get_eth_config_opcode, default_values_param if get_default_config else current_values_param)
    raw_result = rs.debug_protocol(dev).send_and_receive_raw_data(raw_command)
    assert raw_result[0] == get_eth_config_opcode
    return rsutils.eth_config(raw_result[4:])

def set_eth_config(dev, config):
    raw_command = rs.debug_protocol(dev).build_command(set_eth_config_opcode, 0, 0, 0, 0, config.build_command())
    raw_result = rs.debug_protocol(dev).send_and_receive_raw_data(raw_command)
    assert raw_result[0] == set_eth_config_opcode
    time.sleep(1) # Give device time to write into flash


_module_orig = {}

# Restores original configuration once, at end of module
@pytest.fixture(scope="module", autouse=True)
def _module_setup_teardown():
    yield
    if 'orig' in _module_orig and 'dev' in _module_orig:
        try:
            # Overwrite streaming-critical fields with known-good values before writing to flash, healing
            # a unit left throttled (bad MTU/transmission_delay/link.timeout) by a prior interrupted run.
            _module_orig['orig'].link.mtu = 9000
            _module_orig['orig'].transmission_delay = 0
            if _module_orig['orig'].link.timeout != LINK_TIMEOUT_BASELINE:
                _module_orig['orig'].link.timeout = LINK_TIMEOUT_BASELINE
            set_eth_config(_module_orig['dev'], _module_orig['orig'])
        except Exception as e:
            log.warning(f"Error restoring config: {e}")
        _module_orig.clear()

@pytest.fixture(autouse=True)
def _test_setup_teardown(test_device):
    dev, _ = test_device
    if 'orig' not in _module_orig:
        _module_orig['orig'] = get_eth_config(dev)
        _module_orig['new'] = get_eth_config(dev) # Get another config object to keep original config intact
        _module_orig['dev'] = dev
    yield dev, _module_orig['orig'], _module_orig['new']


def test_dds_support(_test_setup_teardown):
    """Tested implicitly in _test_setup_teardown fixture. get_eth_config in would have thrown if not supported"""
    pass


def test_link_timeout_configuration(_test_setup_teardown):
    dev, orig_config, new_config = _test_setup_teardown

    # Toggle between two safe in-range values (avoid doubling - it can overflow the 2000-30000 range).
    new_config.link.timeout = LINK_TIMEOUT_BASELINE if orig_config.link.timeout != LINK_TIMEOUT_BASELINE else LINK_TIMEOUT_ALT
    set_eth_config(dev, new_config)
    updated_config = get_eth_config(dev)
    check.is_true(updated_config.link.timeout == new_config.link.timeout)

    if new_config.header.version >= 5:
        log.info("version >=5")
        new_config.link.timeout = 1000
        with pytest.raises(ValueError, match="Link timeout should be 2000-30000. Current 1000"):
            set_eth_config(dev, new_config)

        new_config.link.timeout = 31000
        with pytest.raises(ValueError, match="Link timeout should be 2000-30000. Current 31000"):
            set_eth_config(dev, new_config)

        new_config.link.timeout = 2345
        with pytest.raises(ValueError, match="Link timeout must be divisible by 100. Current 2345"):
            set_eth_config(dev, new_config)

    new_config.link.timeout = orig_config.link.timeout # Restore field that might fail other tests, depending header version.

def test_mtu_configuration(_test_setup_teardown):
    dev, orig_config, new_config = _test_setup_teardown

    new_config.link.mtu = 8500  # close to default 9000 so an interrupted run leaves the unit streamable
    if new_config.header.version == 3:
        with pytest.raises(ValueError, match="Camera FW supports only MTU 9000."):
            set_eth_config(dev, new_config)
    else:
        set_eth_config(dev, new_config)
        updated_config = get_eth_config(dev)
        check.is_true(updated_config.link.mtu == 8500)

        new_config.link.mtu = 0
        with pytest.raises(ValueError, match=r"MTU size should be 500-9000\. Current 0"):
            set_eth_config(dev, new_config)

        new_config.link.mtu = 1234
        with pytest.raises(ValueError, match=r"MTU size must be divisible by 500\. Current 1234"):
            set_eth_config(dev, new_config)

    new_config.link.mtu = orig_config.link.mtu # Restore field that might fail other tests, depending header version.

def test_transmission_delay_configuration(_test_setup_teardown):
    dev, orig_config, new_config = _test_setup_teardown

    new_config.transmission_delay = 3  # close to default 0 so an interrupted run leaves the unit streamable
    if new_config.header.version == 3:
        with pytest.raises(ValueError, match="Camera FW does not support transmission delay."):
            set_eth_config(dev, new_config)
    else:
        set_eth_config(dev, new_config)
        updated_config = get_eth_config(dev)
        check.is_true(updated_config.transmission_delay == 3)

        new_config.transmission_delay = 222
        with pytest.raises(ValueError, match=r"Transmission delay should be 0-144\. Current 222"):
            set_eth_config(dev, new_config)

        new_config.transmission_delay = 100
        with pytest.raises(ValueError, match=r"Transmission delay must be divisible by 3\. Current 100"):
            set_eth_config(dev, new_config)

    new_config.transmission_delay = orig_config.transmission_delay # Restore field that might fail other tests, depending header version.


def test_udp_ttl_configuration(_test_setup_teardown):
    dev, orig_config, new_config = _test_setup_teardown

    new_config.udp_ttl = 128
    if new_config.header.version < 5:
        with pytest.raises(ValueError, match="Camera FW does not support changing UDP TTL value."):
            set_eth_config(dev, new_config)
    else:
        set_eth_config(dev, new_config)
        updated_config = get_eth_config(dev)
        check.is_true(updated_config.udp_ttl == 128)

        new_config.udp_ttl = 300
        with pytest.raises(ValueError, match=r"UDP TTL should be 1-255 \(or 0 for system default\)\. Current 300"):
            set_eth_config(dev, new_config)

    new_config.udp_ttl = orig_config.udp_ttl # Restore field that might fail other tests, depending header version.


def test_configuration_failures(_test_setup_teardown): # Failures depending on version tested separately
    dev, orig_config, new_config = _test_setup_teardown

    new_config.header.version = 2
    with pytest.raises(ValueError, match="Unrecognized Eth config table version 2"):
        set_eth_config(dev, new_config)
    new_config.header.version = orig_config.header.version

    new_config.configured.ip = rsutils.ip_address(0, 0, 0, 0)
    with pytest.raises(ValueError, match=r"Invalid configured IP address 0\.0\.0\.0"):
        set_eth_config(dev, new_config)
    new_config.configured.ip = orig_config.configured.ip

    new_config.configured.netmask = rsutils.ip_address(0, 0, 0, 0)
    with pytest.raises(ValueError, match=r"Invalid configured network mask 0\.0\.0\.0"):
        set_eth_config(dev, new_config)
    new_config.configured.netmask = orig_config.configured.netmask

    # Don't set valid domain_id, it might cause DDS devices to loose connection (in case of reset/power loss).
    new_config.dds.domain_id = -1
    with pytest.raises(ValueError, match=r"Domain ID should be in 0-232 range\. Current -1"):
        set_eth_config(dev, new_config)
    new_config.dds.domain_id = 233
    with pytest.raises(ValueError, match=r"Domain ID should be in 0-232 range\. Current 233"):
        set_eth_config(dev, new_config)
    new_config.dds.domain_id = orig_config.dds.domain_id


def test_python_wrapper_functionality(_test_setup_teardown):
    dev, _, _ = _test_setup_teardown

    eth_device = rs.eth_config_device(dev)
    orig_link_timeout = eth_device.get_link_timeout()
    new_link_timeout = LINK_TIMEOUT_BASELINE if orig_link_timeout != LINK_TIMEOUT_BASELINE else LINK_TIMEOUT_ALT
    eth_device.set_link_timeout( new_link_timeout )
    updated_link_timeout = eth_device.get_link_timeout()
    check.is_true(updated_link_timeout == new_link_timeout)

    orig_ip, orig_actual_ip = eth_device.get_ip_address()
    eth_device.set_ip_address(rs.ip_address(127, 0, 0, 1))
    new_ip, new_actual_ip = eth_device.get_ip_address()
    check.is_true(new_ip == rs.ip_address(127, 0, 0, 1))

    orig_link_priority = eth_device.get_link_priority()
    priority_to_set = rs.link_priority.usb_first if orig_link_priority != rs.link_priority.usb_first else rs.link_priority.eth_first
    eth_device.set_link_priority(priority_to_set)
    new_link_priority = eth_device.get_link_priority()
    check.is_true(new_link_priority == priority_to_set)
