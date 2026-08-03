# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import math
import pytest
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
    pytest.mark.device_type_exclude("DDS"),  # DDS devices do not implement RS2_EXTENSION_GLOBAL_TIMER
]


def test_device_time_is_positive(test_device):
    dev, _ = test_device
    t_ms = dev.get_device_time_ms()
    log.info(f"device time = {t_ms} ms")
    assert math.isfinite(t_ms)
    assert t_ms > 0.0
