# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2020 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device("D400*"),
    pytest.mark.device_each("D555"),
]


def check_min_max_throw(sensor):
    options_to_check = [rs.option.exposure, rs.option.enable_auto_exposure]
    for option in options_to_check:
        if not sensor.supports(option):
            continue
        option_range = sensor.get_option_range(option)
        # below min
        with pytest.raises(RuntimeError, match="out of range value for argument"):
            sensor.set_option(option, option_range.min - 1)
        # above max
        with pytest.raises(RuntimeError, match="out of range value for argument"):
            sensor.set_option(option, option_range.max + 1)


def test_options_out_of_range_throwing_exception(device_in_service_mode):
    dev, ctx = device_in_service_mode
    sensors = dev.query_sensors()
    for sensor in sensors:
        check_min_max_throw(sensor)
