# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
from app.core.errors import RealSenseError
from app.services import advanced_mode


class _Struct:
    """Stands in for a control group: a mapping view over its fields, like the bindings."""
    def __init__(self, **fields):
        self._fields = fields

    def keys(self):
        return list(self._fields)

    def __getitem__(self, field):
        return self._fields[field]

    def __setitem__(self, field, value):
        current = self._fields[field]
        self._fields[field] = int(round(value)) if isinstance(current, int) else float(value)


class _FakeAM:
    """Three groups: depth_control and ae_control have ranges, hdad reports its value for
    all three modes, as firmware does for a group whose min/max it does not report."""
    def __init__(self, enabled=True):
        self._enabled = enabled
        self.written = {}
        self.groups = {
            "depth_control": [_Struct(deepSeaSecondPeakThreshold=325),
                              _Struct(deepSeaSecondPeakThreshold=0),
                              _Struct(deepSeaSecondPeakThreshold=1023)],
            "ae_control": [_Struct(meanIntensitySetPoint=1000),
                           _Struct(meanIntensitySetPoint=0),
                           _Struct(meanIntensitySetPoint=4095)],
            "hdad": [_Struct(lambdaAD=800.0), _Struct(lambdaAD=800.0), _Struct(lambdaAD=800.0)],
        }

    def is_enabled(self):
        return self._enabled

    def get_all_controls(self):
        return _FakeControls(self)

    # mapping view over the groups: am[group] reads mode 0, am[group, mode] any mode,
    # am[group] = struct writes it
    def __getitem__(self, key):
        group, mode = key if isinstance(key, tuple) else (key, 0)
        return self.groups[group][mode]

    def __setitem__(self, group, values):
        self.written[group] = values
        self.groups[group][0] = values


class _FakeControls:
    """What get_all_controls() answers: every group, each as [values, mins, maxes]."""
    def __init__(self, am):
        self._am = am

    def keys(self):
        return list(self._am.groups)

    def __getitem__(self, group):
        return self._am.groups[group]


class _Unsupported:
    """The wrapper for a device without advanced mode: asking it raises."""
    def is_enabled(self):
        raise RuntimeError("Device does not support advanced mode")


class _FakeDevice:
    def __init__(self, name="RealSense D455"):
        self._name = name

    def supports(self, _info):
        return True

    def get_info(self, _info):
        return self._name


@pytest.fixture
def device_for(monkeypatch):
    """Hand the service our fake wrapper for whatever device it is given."""
    def install(am, name="RealSense D455"):
        monkeypatch.setattr(advanced_mode.rs, "rs400_advanced_mode", lambda _dev: am)
        return _FakeDevice(name)
    return install


def _by_field(groups):
    return {o.option_id: o for opts in groups.values() for o in opts}


def test_reports_a_field_with_its_range(device_for):
    groups = advanced_mode.controls(device_for(_FakeAM()))
    assert "deepSeaSecondPeakThreshold" in {o.option_id for o in groups["depth_control"]}
    o = _by_field(groups)["deepSeaSecondPeakThreshold"]
    assert (o.current_value, o.min_value, o.max_value, o.step) == (325, 0, 1023, 1)


def test_field_without_range_reports_value_for_min_and_max(device_for):
    o = _by_field(advanced_mode.controls(device_for(_FakeAM())))["lambdaAD"]
    assert (o.current_value, o.min_value, o.max_value) == (800.0, 800.0, 800.0)
    assert o.step is None  # a float field states no step


def test_reports_every_group_the_device_has(device_for):
    # Which of them are worth drawing - the AE setpoint is blocked on D457 and D500 - is
    # the viewer's call, so nothing is left out here.
    for name in ("RealSense D455", "RealSense D457", "RealSense D555"):
        assert set(advanced_mode.controls(device_for(_FakeAM(), name))) == {
            "depth_control", "ae_control", "hdad"
        }


def test_set_patches_the_field_and_writes_its_group_back(device_for):
    am = _FakeAM()
    applied = advanced_mode.set_control(
        device_for(am), "depth_control", "deepSeaSecondPeakThreshold", 500.4
    )
    assert am.written["depth_control"]["deepSeaSecondPeakThreshold"] == 500  # int-coerced
    assert (applied.option_id, applied.current_value) == ("deepSeaSecondPeakThreshold", 500)


def test_set_unknown_field_raises(device_for):
    with pytest.raises(KeyError):
        advanced_mode.set_control(device_for(_FakeAM()), "depth_control", "nope", 1)


def test_set_unknown_group_raises(device_for):
    with pytest.raises(KeyError):
        advanced_mode.set_control(device_for(_FakeAM()), "nope", "deepSeaSecondPeakThreshold", 1)


def test_status_reports_supported_and_enabled(device_for):
    assert advanced_mode.status(device_for(_FakeAM())) == {"supported": True, "enabled": True}
    assert advanced_mode.status(device_for(_FakeAM(enabled=False))) == {
        "supported": True, "enabled": False
    }
    assert advanced_mode.status(device_for(_Unsupported())) == {
        "supported": False, "enabled": False
    }


def test_controls_refused_while_advanced_mode_is_off(device_for):
    dev = device_for(_FakeAM(enabled=False))
    with pytest.raises(RealSenseError) as exc:
        advanced_mode.controls(dev)
    assert exc.value.status_code == 409
    with pytest.raises(RealSenseError) as exc:
        advanced_mode.set_control(dev, "depth_control", "deepSeaSecondPeakThreshold", 1)
    assert exc.value.status_code == 409
