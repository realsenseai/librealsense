# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Tests for rspy/pytest/collection.py (filter_and_sort_items).

Verifies the collection-phase logic that runs after pytest discovers tests:
- Context gating: @pytest.mark.context("nightly") skips unless --context matches
- --live filtering: skips tests without device/device_each markers
- Priority sorting: @pytest.mark.priority(N) controls execution order
- Device grouping: tests on the same device run together to minimize hub recycling
"""

import pytest
from unittest.mock import MagicMock
from rspy.pytest.collection import filter_and_sort_items
from helpers import make_mock_item, make_mock_config


class TestContextGating:
    """@pytest.mark.context() should skip tests unless --context or -m matches."""

    def test_skipped_when_context_not_provided(self):
        item = make_mock_item(markers=[pytest.mark.context("nightly")])
        filter_and_sort_items(make_mock_config(context=""), [item])

        item.add_marker.assert_called_once()
        assert item.add_marker.call_args[0][0].name == "skip"

    def test_runs_when_context_matches(self):
        item = make_mock_item(markers=[pytest.mark.context("nightly")])
        filter_and_sort_items(make_mock_config(context="nightly"), [item])

        item.add_marker.assert_not_called()

    def test_runs_when_m_flag_matches(self):
        item = make_mock_item(markers=[pytest.mark.context("nightly")])
        filter_and_sort_items(make_mock_config(markexpr="nightly"), [item])

        item.add_marker.assert_not_called()

    def test_multiple_context_values(self):
        """--context 'nightly weekly' should satisfy @context('nightly')."""
        item = make_mock_item(markers=[pytest.mark.context("nightly")])
        filter_and_sort_items(make_mock_config(context="nightly weekly"), [item])

        item.add_marker.assert_not_called()

    def test_wrong_context_still_skips(self):
        """--context 'weekly' should NOT satisfy @context('nightly')."""
        item = make_mock_item(markers=[pytest.mark.context("nightly")])
        filter_and_sort_items(make_mock_config(context="weekly"), [item])

        item.add_marker.assert_called_once()

    def test_no_context_marker_never_skipped(self):
        item = make_mock_item(markers=[])
        filter_and_sort_items(make_mock_config(), [item])

        item.add_marker.assert_not_called()


class TestLiveFiltering:
    """--live should skip tests that have no device/device_each markers."""

    def _device_marker(self, name):
        m = MagicMock()
        m.name = name
        m.args = ("D455",)
        return m

    def test_skips_non_device_tests(self):
        item = make_mock_item()
        filter_and_sort_items(make_mock_config(live=True), [item])

        item.add_marker.assert_called_once()
        assert item.add_marker.call_args[0][0].name == "skip"

    def test_keeps_device_tests(self):
        item = make_mock_item(markers=[self._device_marker("device")])
        filter_and_sort_items(make_mock_config(live=True), [item])

        item.add_marker.assert_not_called()

    def test_keeps_device_each_tests(self):
        item = make_mock_item(markers=[self._device_marker("device_each")])
        filter_and_sort_items(make_mock_config(live=True), [item])

        item.add_marker.assert_not_called()

    def test_no_live_flag_keeps_everything(self):
        item = make_mock_item()
        filter_and_sort_items(make_mock_config(live=False), [item])

        item.add_marker.assert_not_called()


class TestNotLiveFiltering:
    """--not-live should skip tests that HAVE device/device_each markers."""

    def _device_marker(self, name):
        m = MagicMock()
        m.name = name
        m.args = ("D455",)
        return m

    def test_skips_device_tests(self):
        item = make_mock_item(markers=[self._device_marker("device")])
        filter_and_sort_items(make_mock_config(not_live=True), [item])

        item.add_marker.assert_called_once()
        assert item.add_marker.call_args[0][0].name == "skip"

    def test_skips_device_each_tests(self):
        item = make_mock_item(markers=[self._device_marker("device_each")])
        filter_and_sort_items(make_mock_config(not_live=True), [item])

        item.add_marker.assert_called_once()
        assert item.add_marker.call_args[0][0].name == "skip"

    def test_keeps_non_device_tests(self):
        item = make_mock_item()
        filter_and_sort_items(make_mock_config(not_live=True), [item])

        item.add_marker.assert_not_called()


class TestPrioritySorting:
    """@pytest.mark.priority(N) should sort tests — lower values run first."""

    def test_priority_ordering(self):
        items = [
            make_mock_item("test_low", markers=[pytest.mark.priority(100)]),
            make_mock_item("test_default"),
            make_mock_item("test_high", markers=[pytest.mark.priority(900)]),
            make_mock_item("test_first", markers=[pytest.mark.priority(1)]),
        ]
        filter_and_sort_items(make_mock_config(), items)

        names = [i.name for i in items]
        assert names[0] == "test_first"
        assert names[1] == "test_low"
        assert names.index("test_default") < names.index("test_high")

    def test_default_priority_is_500(self):
        items = [
            make_mock_item("test_no_prio"),
            make_mock_item("test_below", markers=[pytest.mark.priority(499)]),
            make_mock_item("test_above", markers=[pytest.mark.priority(501)]),
        ]
        filter_and_sort_items(make_mock_config(), items)

        names = [i.name for i in items]
        assert names[0] == "test_below"
        assert names.index("test_no_prio") < names.index("test_above")


class TestDeviceGrouping:
    """Tests should be grouped by (module, device_serial) so hub recycling is minimized."""

    def test_grouped_by_module_and_device(self):
        items = [
            make_mock_item("test_a[D455-111]", module_name="mod_frames", device_serial="111"),
            make_mock_item("test_a[D435-222]", module_name="mod_frames", device_serial="222"),
            make_mock_item("test_b[D455-111]", module_name="mod_frames", device_serial="111"),
            make_mock_item("test_b[D435-222]", module_name="mod_frames", device_serial="222"),
        ]
        filter_and_sort_items(make_mock_config(), items)

        names = [i.name for i in items]
        d455 = [i for i, n in enumerate(names) if "D455" in n]
        d435 = [i for i, n in enumerate(names) if "D435" in n]
        assert d455 == [0, 1] or d455 == [2, 3]
        assert d435 == [0, 1] or d435 == [2, 3]
        assert set(d455) & set(d435) == set()
