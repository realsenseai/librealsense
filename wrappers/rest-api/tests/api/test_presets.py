# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import json

import pytest

from app.services import advanced_mode, presets
from .conftest import client

URL = "/api/v1/devices/device1/presets"


class _Serializable:
    def __init__(self, dev):
        self.dev = dev

    def serialize_json(self):
        return json.dumps({"device": {"name": "Test Device 1"}, "parameters": {"controls-laserpower": "150"}})

    def load_json(self, text):
        _Serializable.loaded = json.loads(text)


class _AM:
    def __init__(self, enabled=True):
        self.enabled = enabled

    def is_enabled(self):
        return self.enabled


@pytest.fixture
def presets_ready(setup_mock_managers, monkeypatch, tmp_path):
    rs_manager = setup_mock_managers["rs_manager"]
    rs_manager.settings.update({"paths": {"presets_folder": str(tmp_path)}})
    monkeypatch.setattr(presets.rs, "serializable_device", _Serializable)
    monkeypatch.setattr(advanced_mode.rs, "rs400_advanced_mode", lambda _dev: _AM())
    return rs_manager


def test_current_preset_downloads_as_json(presets_ready):
    response = client.get(f"{URL}/current")
    assert response.status_code == 200
    assert response.headers["content-disposition"].endswith('1 preset.json"')  # model = last word of the name
    assert response.json()["parameters"]["controls-laserpower"] == "150"


def test_presets_need_advanced_mode(setup_mock_managers, monkeypatch):
    monkeypatch.setattr(presets.rs, "serializable_device", _Serializable)
    monkeypatch.setattr(advanced_mode.rs, "rs400_advanced_mode", lambda _dev: _AM(enabled=False))
    assert client.get(f"{URL}/current").status_code == 409
    assert client.post(f"{URL}/load", json={"text": "{}"}).status_code == 409


def test_load_applies_the_json_and_sets_the_preset_to_custom(presets_ready):
    from ..mocks.pyrealsense_mock import option
    depth = presets_ready.devices["device1"].sensors[0]
    depth._options[option.visual_preset] = 3
    depth._option_ranges[option.visual_preset] = type(depth._option_ranges[option.laser_power])(0, 6, 3, 1)
    depth.get_option_value_description = lambda opt, value: "Custom" if value == 0 else "Preset"

    assert client.post(f"{URL}/load", json={"text": json.dumps({"parameters": {"x": "1"}})}).status_code == 200
    assert _Serializable.loaded == {"parameters": {"x": "1"}}
    assert depth._options[option.visual_preset] == 0


def test_save_lists_and_loads_from_the_presets_folder(presets_ready, tmp_path):
    listed = client.post(f"{URL}/save", json={"name": "Max Range"}).json()
    assert listed == [{"path": str(tmp_path / "1 Max Range.preset"), "name": "Max Range"}]
    (tmp_path / "D455 Other.preset").write_text("{}")  # another model: not offered for this camera
    assert [p["name"] for p in client.get(f"{URL}/").json()] == ["Max Range"]

    assert client.post(f"{URL}/load", json={"path": listed[0]["path"]}).status_code == 200
    assert _Serializable.loaded["parameters"]["controls-laserpower"] == "150"
    assert client.post(f"{URL}/load", json={"path": str(tmp_path / "missing.preset")}).status_code == 404


def test_upload_applies_a_preset_file(presets_ready):
    response = client.post(f"{URL}/upload", files={"file": ("mine.json", json.dumps({"parameters": {"y": "2"}}).encode(), "application/json")})
    assert response.status_code == 200
    assert _Serializable.loaded == {"parameters": {"y": "2"}}
