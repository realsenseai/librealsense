# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Tests for rspy/pytest/logging_setup.py (test_log_name, _log_key).

Verifies how per-test log filenames are derived:
- Directory components are included: live/frames/pytest-depth.py → pytest-live-frames-depth.log
- With device param: live/frames/pytest-depth.py::test[D455-111] → pytest-live-frames-depth_D455-111.log
- Without device param: live/frames/pytest-depth.py::test_basic → pytest-live-frames-depth.log
- No parent dir: pytest-standalone.py::test → pytest-standalone.log
- Special characters (<, >, etc.) are sanitized to underscores
- _log_key extracts (fspath, device_id) for grouping tests into shared log files
"""

from unittest.mock import MagicMock
from rspy.pytest.logging_setup import test_log_name as derive_log_name, _log_key


class TestLogNaming:
    """derive_log_name() and _log_key() derive per-test log filenames."""

    def _item(self, fspath, name):
        item = MagicMock()
        item.fspath = fspath
        item.name = name
        return item

    def test_with_device_param(self):
        item = self._item("live/frames/pytest-depth.py", "test_x[D455-104623060005]")
        assert derive_log_name(item) == "pytest-live-frames-depth_D455-104623060005.log"

    def test_without_device_param(self):
        item = self._item("live/frames/pytest-depth.py", "test_depth_basic")
        assert derive_log_name(item) == "pytest-live-frames-depth.log"

    def test_special_chars_sanitized(self):
        item = self._item("live/frames/pytest-depth.py", "test_x[D455<special>]")
        assert derive_log_name(item) == "pytest-live-frames-depth_D455_special_.log"

    def test_no_parent_dir(self):
        """Test at root of unit-tests/ — no directory prefix added."""
        item = self._item("pytest-standalone.py", "test_basic")
        assert derive_log_name(item) == "pytest-standalone.log"

    def test_single_parent_dir(self):
        """Test one level deep — e.g. live/pytest-foo.py."""
        item = self._item("live/pytest-foo.py", "test_bar")
        assert derive_log_name(item) == "pytest-live-foo.log"

    def test_deep_nesting(self):
        """Test in hw-reset subdirectory."""
        item = self._item("live/hw-reset/pytest-sanity.py", "test_x[D455-111]")
        assert derive_log_name(item) == "pytest-live-hw-reset-sanity_D455-111.log"

    def test_absolute_path_with_unit_tests(self):
        """Absolute path — unit-tests/ marker is found and stripped."""
        item = self._item("/home/user/librealsense/unit-tests/live/frames/pytest-depth.py", "test_x")
        assert derive_log_name(item) == "pytest-live-frames-depth.log"

    def test_absolute_path_outside_tree(self):
        """Absolute path without unit-tests/ — falls back to basename only."""
        item = self._item("/tmp/other/pytest-depth.py", "test_x")
        assert derive_log_name(item) == "pytest-depth.log"

    def test_log_key_with_brackets(self):
        item = self._item("live/frames/pytest-depth.py", "test_x[D455-111]")
        assert _log_key(item) == ("live/frames/pytest-depth.py", "D455-111")

    def test_log_key_without_brackets(self):
        item = self._item("live/frames/pytest-depth.py", "test_x")
        assert _log_key(item) == ("live/frames/pytest-depth.py", None)

    def test_log_key_none(self):
        assert _log_key(None) is None
