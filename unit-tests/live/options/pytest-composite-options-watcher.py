# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
from rspy.timer import Timer
import time
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D585", "D555"),
    pytest.mark.device_exclude("D585S"),
    pytest.mark.context("nightly"),
]

# Composite options to watch, in order of preference: (id, getter, setter, enable field).
# Temporal Filter DPP is a USB/GMSL composite; on DDS only the Improved Close Range control is one.
COMPOSITES = [
    (rs.composite_option_id.temporal_filter_dpp, "get_temporal_filter_dpp_config", "set_temporal_filter_dpp_config", "enabled"),
    (rs.composite_option_id.icr_control, "get_icr_control", "set_icr_control", "enable"),
]


class Watched:
    def __init__(self, embedded_filter, option_id, getter, setter, field):
        self.filter, self.id, self.field = embedded_filter, option_id, field
        self._get, self._set = getattr(embedded_filter, getter), getattr(embedded_filter, setter)
        self.original = self._get(option_id)
        self.changed = []

    def toggle(self):
        cfg = self._get(self.id)
        setattr(cfg, self.field, 1 - getattr(cfg, self.field))
        self._set(self.id, cfg)

    def restore(self):
        self._set(self.id, self.original)

    def wait_for_change(self, timeout=5):
        timer = Timer(timeout)
        timer.start()
        while not self.changed and not timer.has_expired():
            time.sleep(0.1)


@pytest.fixture
def watched(test_device):
    """A readable composite option of an embedded filter, with an options-changed callback collecting the changed
    composite ids. The original value is restored at teardown."""
    dev, ctx = test_device
    watched = None
    for embedded_filter in dev.first_depth_sensor().query_embedded_filters():
        for option_id, getter, setter, field in COMPOSITES:
            if option_id not in embedded_filter.get_supported_composite_options():
                continue
            try:
                watched = Watched(embedded_filter, option_id, getter, setter, field)
                break
            except RuntimeError as e:  # registered but not readable on this FW
                log.info(f"{option_id} not readable: {e}")
        if watched:
            break
    if watched is None:
        pytest.skip("No readable composite option on this device")

    watched.filter.on_options_changed(lambda opt_list: watched.changed.extend(opt_list.get_changed_composite_options()))
    time.sleep(1.5)  # let the options-watcher establish its baseline before making changes
    yield watched

    watched.restore()
    time.sleep(2)  # let the restore settle, so it is not reported to the next test's watcher


def test_composite_change_is_reported(watched):
    watched.toggle()

    watched.wait_for_change()
    assert watched.changed and set(watched.changed) == {watched.id}


def test_no_sporadic_composite_changes(watched):
    time.sleep(3)
    assert not watched.changed

    # Positive control: the watcher must still be polling, otherwise the quiet period above proves nothing
    watched.toggle()
    watched.wait_for_change()
    assert set(watched.changed) == {watched.id}
