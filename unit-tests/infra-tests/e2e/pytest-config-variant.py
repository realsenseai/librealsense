# A D455 stands in for a camera with a 2C variant ("111-2c", named "D455 Dual RGB"): the same physical
# camera behind a second device entry. Setup must enable the variant's port, switch the camera to 2C and hand
# the test the REAL serial; teardown must switch it back to 3C before the port is disabled.
import types
import conftest
import rspy.devices as _devices
from rspy import d585_config

_variant = types.SimpleNamespace(serial_number='111-2c', name='D455 Dual RGB', real_serial='111', base=object(), port=1)
_real_get = _devices.get
_devices.get = lambda sn: _variant if sn == '111-2c' else _real_get(sn)
_devices.by_spec = lambda pattern, ignored: iter(['111-2c'] if 'Dual RGB' in pattern else [])


def _ensure(serial, key):
    conftest._tracking.setdefault("ensure_calls", []).append([serial, key])
    conftest._save_tracking()


d585_config.ensure = _ensure

import pytest


@pytest.mark.device_each("Dual RGB")
def test_variant(module_device_setup):
    assert module_device_setup == '111'
