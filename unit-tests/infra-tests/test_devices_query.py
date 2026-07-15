# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import types

from rspy import devices


class FakePyrsDevice:
    def __init__(self, serial_number, name):
        self.serial_number = serial_number
        self.name = name

    def supports(self, _info):
        return True

    def get_info(self, info):
        if info == "serial_number":
            return self.serial_number
        if info == "name":
            return self.name
        raise AssertionError(f"unexpected camera info: {info}")


def test_query_discovers_domain_0_d555_without_hub(monkeypatch):
    d555 = FakePyrsDevice("123", "Intel RealSense D555")
    context_settings = []

    class FakeContext:
        def __init__(self, settings):
            self.settings = settings

        def query_devices(self, *_args):
            if self.settings["dds"].get("domain") == 0:
                return [d555]
            return []

    fake_rs = types.SimpleNamespace(
        camera_info=types.SimpleNamespace(
            serial_number="serial_number",
            firmware_update_id="firmware_update_id",
            name="name",
        ),
        product_line=types.SimpleNamespace(sw_only=1, any=2),
    )

    def make_context(settings):
        context_settings.append(settings)
        return FakeContext(settings)

    fake_rs.context = make_context
    monkeypatch.setattr(devices, "rs", fake_rs)
    monkeypatch.setattr(devices, "hub", None)
    monkeypatch.setattr(devices, "_context", None)
    monkeypatch.setattr(devices, "_device_by_sn", {})
    monkeypatch.setattr(devices, "init_hub", lambda: None)
    monkeypatch.setattr(devices.time, "sleep", lambda _seconds: None)
    monkeypatch.setattr(
        devices,
        "Device",
        lambda sn, dev: types.SimpleNamespace(serial_number=sn, handle=dev, port=None),
    )

    devices.query(monitor_changes=False, disable_dds=False)

    assert context_settings == [
        {"dds": {"enabled": True}},
        {"dds": {"enabled": True, "domain": 0}},
    ]
    assert devices.get("123").handle is d555
