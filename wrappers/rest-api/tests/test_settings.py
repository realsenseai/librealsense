# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import json

import pytest

from app.core.errors import RealSenseError
from app.services.settings import SettingsStore


def test_missing_file_yields_defaults(tmp_path):
    store = SettingsStore(tmp_path / "settings.json")
    assert store.get().viewer.metric_system is True
    assert store.get().console.max_entries == 1000


def test_update_merges_one_key_and_persists(tmp_path):
    path = tmp_path / "settings.json"
    updated = SettingsStore(path).update({"viewer": {"metric_system": False}, "console": {"max_entries": 50}})
    assert updated.viewer.metric_system is False
    assert updated.console.max_entries == 50
    assert updated.record.file_save_mode == "auto"  # untouched group keeps its default

    reloaded = SettingsStore(path).get()
    assert reloaded.viewer.metric_system is False
    assert json.loads(path.read_text())["console"]["max_entries"] == 50


def test_invalid_value_is_refused_and_nothing_changes(tmp_path):
    store = SettingsStore(tmp_path / "settings.json")
    with pytest.raises(RealSenseError) as exc:
        store.update({"console": {"log_severity": "loud"}})
    assert exc.value.status_code == 422
    assert store.get().console.log_severity == "info"


def test_damaged_file_falls_back_to_defaults(tmp_path):
    path = tmp_path / "settings.json"
    path.write_text("{not json")
    assert SettingsStore(path).get().context.dds_domain == 0


def test_grid_overlay_prefs_are_bounded(tmp_path):
    store = SettingsStore(tmp_path / "s.json")
    assert store.get().viewer.grid_horizontal_lines == 1
    assert store.get().viewer.grid_line_color == "#ffffff"
    updated = store.update({"viewer": {"grid_horizontal_lines": 3, "grid_line_color": "#00ff00"}})
    assert (updated.viewer.grid_horizontal_lines, updated.viewer.grid_line_color) == (3, "#00ff00")
    with pytest.raises(RealSenseError):
        store.update({"viewer": {"grid_vertical_lines": 6}})
    with pytest.raises(RealSenseError):
        store.update({"viewer": {"grid_line_color": "green"}})
