# Model a device whose hub port was left powered by a skipped/crashed teardown. Patch
# any_port_powered -> True at import (before module_device_setup runs) so the conftest recycle
# decision takes the "already powered" branch: setup recycles the device clean (recycle=True)
# instead of reusing a possibly-bad state.
import rspy.devices as _devices
_devices.any_port_powered = lambda serials: True

import pytest


@pytest.mark.device("D455")
def test_d455(module_device_setup):
    assert module_device_setup == '111'
