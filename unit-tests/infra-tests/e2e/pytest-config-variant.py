# A D455 stands in for a camera that can switch to 2C (2C name "D455 Dual RGB"). Selecting it by both names
# runs the module twice on the same camera: once in 3C (param "111") and once in 2C (param "111-2c").
# Every setup puts the camera in the mode it was selected for; the 2C module switches back at teardown.
import conftest
from rspy import d585_config

d585_config.switchable_to = lambda pattern: ['111'] if 'Dual RGB' in pattern else []
d585_config.can_toggle = lambda device: device.name == 'D455'


def _ensure(serial, key):
    conftest._tracking.setdefault("ensure_calls", []).append([serial, key])
    conftest._save_tracking()


d585_config.ensure = _ensure

import pytest


@pytest.mark.device_each("D455", "D455 Dual RGB")
def test_both_modes(module_device_setup):
    assert module_device_setup == '111'


@pytest.mark.device_each("D435")
def test_other_camera_untouched(module_device_setup):
    assert module_device_setup == '222'
