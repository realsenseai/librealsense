# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import json

import pytest

from app.core.errors import RealSenseError
from app.services import hdr
from tests.mocks.pyrealsense_mock import create_mock_device, option, option_range

DEVICE_JSON = {"device": {}, "hdr-preset": {"id": "2", "iterations": "0", "items": [
    {"iterations": "1", "controls": {"depth-gain": "16", "depth-exposure": "100"}},
    {"iterations": "3", "controls": {"depth-gain": "32", "depth-exposure": "8000"}},
]}}
AUTO_JSON = {"hdr-preset": {"id": "1", "iterations": "5", "items": [
    {"iterations": "1", "controls": {"depth-ae": "1"}},
    {"iterations": "1", "controls": {"depth-ae-gain": "-4", "depth-ae-exp": "200"}},
]}}


class _Serializable:
    text = json.dumps(DEVICE_JSON)
    loaded = None

    def __init__(self, _dev):
        pass

    def serialize_json(self):
        return _Serializable.text

    def load_json(self, text):
        _Serializable.loaded = json.loads(text)


@pytest.fixture
def device(monkeypatch):
    monkeypatch.setattr(hdr.rs, "serializable_device", _Serializable)
    _Serializable.text = json.dumps(DEVICE_JSON)
    dev = create_mock_device("d1", "RealSense D555")
    depth = dev.sensors[0]
    depth._options[option.hdr_enabled] = 1
    depth._option_ranges[option.hdr_enabled] = option_range(0, 1, 0, 1)
    return dev


def test_manual_preset_is_parsed_from_the_device_json():
    preset = hdr.from_json(json.dumps(DEVICE_JSON))
    assert preset["id"] == "2" and preset["control_type_auto"] is False
    assert [(i["iterations"], i["controls"]["depth_gain"], i["controls"]["depth_exp"]) for i in preset["items"]] == [(1, 16, 100), (3, 32, 8000)]


def test_auto_preset_drops_the_marker_item_and_keeps_deltas():
    preset = hdr.from_json(json.dumps(AUTO_JSON))
    assert preset["control_type_auto"] is True and preset["iterations"] == 5
    assert preset["items"] == [{"iterations": 1, "controls": {"depth_gain": 0, "depth_exp": 0, "delta_gain": -4, "delta_exp": 200}}]


def test_to_json_roundtrips_both_modes():
    manual = hdr.from_json(json.dumps(DEVICE_JSON))
    assert hdr.from_json(hdr.to_json(manual)) == manual
    auto = hdr.from_json(json.dumps(AUTO_JSON))
    out = json.loads(hdr.to_json(auto))["hdr-preset"]
    assert out["items"][0] == {"iterations": "1", "controls": {"depth-ae": "1"}}
    assert out["items"][1]["controls"] == {"depth-ae-gain": "-4", "depth-ae-exp": "200"}


def test_status_reports_support_ranges_and_preset(device):
    s = hdr.status(device)
    assert s["supported"] is True and s["hdr_enabled"] is True
    assert s["exposure_range"] == {"min": 1, "max": 66, "step": 1, "default": 33}
    assert s["preset"]["items"][1]["controls"]["depth_exp"] == 8000


def test_status_without_the_section_is_unsupported(device):
    _Serializable.text = json.dumps({"device": {}, "parameters": {}})
    assert hdr.status(device)["supported"] is False


def test_apply_disables_hdr_first_and_loads_the_sequence(device):
    depth = device.sensors[0]
    preset = hdr.default_preset()
    hdr.apply(device, preset)
    assert depth._options[option.hdr_enabled] == 0.0
    assert _Serializable.loaded["hdr-preset"]["items"][1]["controls"] == {"depth-gain": "16", "depth-exposure": "8500"}


def test_apply_refused_without_support(device):
    _Serializable.text = "{}"
    with pytest.raises(RealSenseError) as exc:
        hdr.apply(device, hdr.default_preset())
    assert exc.value.status_code == 400
