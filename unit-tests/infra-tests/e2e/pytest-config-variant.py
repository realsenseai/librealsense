# A D455 stands in for a camera that can switch to 2C. Every test on it also runs in 2C (param "111-2c"), after
# all the 3C runs, unless it excludes "Dual RGB". Every setup puts the camera in the mode it was selected for;
# it switches back to 3C once, after the last 2C run.
import conftest
from rspy import d585_config

d585_config.switchable_to = lambda pattern: ['111'] if 'Dual RGB' in pattern else []
d585_config.can_toggle = lambda device: device.name == 'D455'
d585_config.two_c_name = lambda device: 'D455 Dual RGB'


def _ensure(serial, key):
    conftest._tracking.setdefault("ensure_calls", []).append([serial, key])
    conftest._save_tracking()


d585_config.ensure = _ensure

import pytest


@pytest.mark.device_each("D455")
def test_runs_in_both_modes(module_device_setup):
    assert module_device_setup == '111'


@pytest.mark.device_each("D455")
@pytest.mark.device_exclude("Dual RGB")
def test_excluded_from_2c(module_device_setup):
    assert module_device_setup == '111'


@pytest.mark.device_each("D435")
def test_other_camera_untouched(module_device_setup):
    assert module_device_setup == '222'
