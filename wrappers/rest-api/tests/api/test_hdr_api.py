# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import json

from app.services import hdr
from .conftest import client

URL = "/api/v1/devices/device1/hdr/"


class _Serializable:
    text = json.dumps({"hdr-preset": {"id": "0", "iterations": "0", "items": [
        {"iterations": "1", "controls": {"depth-gain": "16", "depth-exposure": "100"}}]}})

    def __init__(self, _dev):
        pass

    def serialize_json(self):
        return _Serializable.text

    def load_json(self, text):
        _Serializable.text = text


def test_hdr_unsupported_without_the_preset_section(setup_mock_managers, monkeypatch):
    class _Plain(_Serializable):
        def serialize_json(self):
            return "{}"
    monkeypatch.setattr(hdr.rs, "serializable_device", _Plain)
    body = client.get(URL).json()
    assert body["supported"] is False and body["exposure_range"]["max"] == 66
    assert client.put(URL, json=hdr.default_preset()).status_code == 400


def test_hdr_get_and_apply_roundtrip(setup_mock_managers, monkeypatch):
    monkeypatch.setattr(hdr.rs, "serializable_device", _Serializable)
    assert client.get(URL).json()["preset"]["items"][0]["controls"]["depth_exp"] == 100

    preset = hdr.default_preset()
    preset["items"][1]["controls"]["depth_exp"] = 5000
    applied = client.put(URL, json=preset).json()
    assert applied["supported"] is True
    assert [i["controls"]["depth_exp"] for i in applied["preset"]["items"]] == [1, 5000]
