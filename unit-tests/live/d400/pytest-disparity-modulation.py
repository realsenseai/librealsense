# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

# This test checks that A Factor of Disparity can be changed

import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [pytest.mark.device("D400*")]


def _test_amp_factor(am_device, input_factor_values: list):
    """
    This function set new A Factor value to advance mode device
    :am_device: advance mode device
    :input_factor_values: list of A Factor values
    """
    amp_factor = am_device.get_amp_factor()
    output_factor_values = []

    for factor_value in input_factor_values:
        amp_factor.a_factor = factor_value
        am_device.set_amp_factor(amp_factor)
        output_factor_values.append(am_device.get_amp_factor().a_factor)

    assert output_factor_values == pytest.approx(input_factor_values, abs=1e-6)


def test_verify_set_get_disparity_modulation(test_device):
    dev, _ = test_device
    advance_mode_device = rs.rs400_advanced_mode(dev)

    if advance_mode_device:
        a_factor_values = [0.05, 0.01]
        _test_amp_factor(advance_mode_device, a_factor_values)
    else:
        log.debug('Depth sensor or advanced mode not found.')
        pytest.fail()
