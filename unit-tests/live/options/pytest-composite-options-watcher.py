# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
from rspy.timer import Timer
import time
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D585"),
    pytest.mark.device_exclude("D585S"),
    pytest.mark.device_type_exclude("DDS"),  # Temporal Filter DPP is a USB/GMSL composite option
    pytest.mark.context("nightly"),
]

TEMPORAL = rs.composite_option_id.temporal_filter_dpp


@pytest.fixture
def watched_filter(test_device):
    """The embedded filter holding Temporal Filter DPP, with an options-changed callback collecting the changed
    composite ids. The original value is restored at teardown."""
    dev, ctx = test_device
    embedded_filter = next((f for f in dev.first_depth_sensor().query_embedded_filters()
                            if TEMPORAL in f.get_supported_composite_options()), None)
    if embedded_filter is None:
        pytest.skip("Temporal Filter DPP composite option not supported on this device")
    original = embedded_filter.get_temporal_filter_dpp_config(TEMPORAL)

    changed = []
    embedded_filter.on_options_changed(lambda opt_list: changed.extend(opt_list.get_changed_composite_options()))
    time.sleep(1.5)  # let the options-watcher establish its baseline before making changes
    yield embedded_filter, original, changed

    embedded_filter.set_temporal_filter_dpp_config(TEMPORAL, original)
    time.sleep(2)  # let the restore settle, so it is not reported to the next test's watcher


def wait_for_change(changed, timeout=5):
    timer = Timer(timeout)
    timer.start()
    while not changed and not timer.has_expired():
        time.sleep(0.1)


def test_composite_change_is_reported(watched_filter):
    embedded_filter, original, changed = watched_filter

    cfg = embedded_filter.get_temporal_filter_dpp_config(TEMPORAL)
    cfg.enabled = 1 - cfg.enabled
    embedded_filter.set_temporal_filter_dpp_config(TEMPORAL, cfg)

    wait_for_change(changed)
    assert changed and set(changed) == {TEMPORAL}


def test_no_sporadic_composite_changes(watched_filter):
    _, _, changed = watched_filter

    time.sleep(3)
    assert not changed
