# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Tests for save_failure_snapshot dedup in unit-tests/live/image-quality/iq_helper.py.

On hub-less multi-device rigs (e.g. Jetson with D457 on MIPI + D436 on USB), the
same image-quality test runs once per parametrized device. The dedup that
prevents repeated snapshots for the same failing test must key on (test_file,
device_name) so that both devices' failures still produce their own snapshot --
not just the first one to fail.
"""

import os
import sys
import pytest
from unittest.mock import MagicMock

# iq_helper imports cv2, pyrealsense2, and rspy at module load. If any of them
# isn't available, skip the whole file cleanly (importorskip raises Skipped at
# module level, which pytest handles as a module skip rather than a collection error).
pytest.importorskip("cv2")
pytest.importorskip("pyrealsense2")

# iq_helper lives outside the standard rspy import path; add its directory.
_IQ_DIR = os.path.normpath(os.path.join(
    os.path.dirname(os.path.abspath(__file__)), '..', 'live', 'image-quality'))
if _IQ_DIR not in sys.path:
    sys.path.insert(0, _IQ_DIR)

iq_helper = pytest.importorskip("iq_helper")


def _make_pipeline(device_full_name):
    """Build a MagicMock that mimics the pipeline.get_active_profile().get_device().get_info() chain."""
    pipeline = MagicMock()
    pipeline.get_active_profile.return_value.get_device.return_value.get_info.return_value = device_full_name
    return pipeline


def _make_image():
    """A non-None stand-in for the annotated image; the helper only checks it's not None."""
    return MagicMock(name="annotated_image")


class TestSaveFailureSnapshotDedup:

    @pytest.fixture(autouse=True)
    def imwrite_mock(self, monkeypatch):
        """Mock only iq_helper's cv2.imwrite for the duration of each test, and reset the
        module-level dedup state. Using monkeypatch (rather than mutating sys.modules['cv2'])
        keeps the real cv2 intact for any other test in the same pytest session."""
        iq_helper._snapshot_saved.clear()
        mock = MagicMock()
        monkeypatch.setattr(iq_helper.cv2, 'imwrite', mock)
        yield mock
        iq_helper._snapshot_saved.clear()

    def test_same_test_different_devices_both_saved(self, imwrite_mock):
        """The Jetson scenario: D457 and D436 both fail same test -> two snapshots."""
        iq_helper.save_failure_snapshot(
            'test_basic_color.py',
            _make_pipeline('Intel RealSense D457'),
            annotated_image=_make_image())
        iq_helper.save_failure_snapshot(
            'test_basic_color.py',
            _make_pipeline('Intel RealSense D436'),
            annotated_image=_make_image())

        saved_paths = [c.args[0] for c in imwrite_mock.call_args_list]
        assert len(saved_paths) == 2, f"expected 2 snapshots, got: {saved_paths}"
        assert any('D457' in os.path.basename(p) for p in saved_paths), saved_paths
        assert any('D436' in os.path.basename(p) for p in saved_paths), saved_paths

    def test_same_test_same_device_deduped(self, imwrite_mock):
        """Repeated failure on the same (test, device) pair only saves once."""
        iq_helper.save_failure_snapshot(
            'test_basic_color.py',
            _make_pipeline('Intel RealSense D457'),
            annotated_image=_make_image())
        iq_helper.save_failure_snapshot(
            'test_basic_color.py',
            _make_pipeline('Intel RealSense D457'),
            annotated_image=_make_image())

        assert imwrite_mock.call_count == 1

    def test_different_tests_independent_dedup(self, imwrite_mock):
        """Different test files dedupe independently even when sharing a device."""
        iq_helper.save_failure_snapshot(
            'test_basic_color.py',
            _make_pipeline('Intel RealSense D436'),
            annotated_image=_make_image())
        iq_helper.save_failure_snapshot(
            'test_basic_depth.py',
            _make_pipeline('Intel RealSense D436'),
            annotated_image=_make_image())

        assert imwrite_mock.call_count == 2

    def test_no_device_name_falls_back_to_test_name(self, imwrite_mock):
        """When pipeline can't yield a device name, filename omits the device suffix."""
        bad_pipeline = MagicMock()
        bad_pipeline.get_active_profile.side_effect = RuntimeError("no profile")

        iq_helper.save_failure_snapshot(
            'test_basic_color.py', bad_pipeline, annotated_image=_make_image())

        assert imwrite_mock.call_count == 1
        saved_path = imwrite_mock.call_args.args[0]
        basename = os.path.basename(saved_path)
        assert basename == 'test_basic_color.png', \
            f"expected device-less filename, got: {basename}"
