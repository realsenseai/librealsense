# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# requires(fw=) per camera: D455 is below its minimum and must be skipped before module_device_setup powers it;
# D401 meets its minimum; D435 has none listed. Device handles are patched in at import, before any fixture runs.
import rspy.devices as _devices

import pytest


class _Handle:
    def __init__(self, fw):
        self._fw = fw

    def is_in_recovery_mode(self):
        return False

    def supports(self, info):
        return True

    def get_info(self, info):
        return self._fw

    def query_sensors(self):
        return []


_FW = {"111": "7.61.47150.17821", "777": "7.62.0.1"}
_get = _devices.get


def _get_with_handle(sn):
    dev = _get(sn)
    if dev:
        dev.handle = _Handle(_FW.get(sn, "5.17.0.10"))
    return dev


_devices.get = _get_with_handle

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.requires(fw={"D455": "7.62", "D401": "7.62"}),
]


def test_requires(module_device_setup):
    pass
