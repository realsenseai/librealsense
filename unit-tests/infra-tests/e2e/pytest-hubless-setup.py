# Model a hub-less bench (e.g. Jetson). Patch devices.hub to None at import -- this runs during
# collection, before module_device_setup -- so the conftest recycle decision takes the no-hub
# branch: teardown-disable is a no-op there, so setup recycles via enable_only(recycle=True)
# (which falls back to hardware_reset on a real hub-less machine).
import rspy.devices as _devices
_devices.hub = None

import pytest


@pytest.mark.device("D455")
def test_d455(module_device_setup):
    assert module_device_setup == '111'
