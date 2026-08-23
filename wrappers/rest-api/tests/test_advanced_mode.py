# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
from app.services.advanced_mode import build_advanced_options, set_advanced_option
from app.services.rs_manager import RealSenseManager
from app.core.errors import RealSenseError


class _Struct:
    def __init__(self, **kw):
        self.__dict__.update(kw)


class _FakeAM:
    """Exposes only depth_control (int) + color_control (bool-like int) groups.
    Other GROUPS getters are absent -> getattr raises -> build skips them."""
    def __init__(self):
        self.written = {}

    def get_depth_control(self, mode):
        return _Struct(deepSeaSecondPeakThreshold={0: 325, 1: 0, 2: 1023}[mode])

    def set_depth_control(self, s):
        self.written["depth_control"] = s.deepSeaSecondPeakThreshold

    def get_color_control(self, mode):
        return _Struct(disableSADColor={0: 0, 1: 0, 2: 1}[mode])

    def set_color_control(self, s):
        self.written["color_control"] = s.disableSADColor

    def get_hdad(self, mode):
        # current works, but ranges (mode 1/2) are unsupported on this device
        if mode != 0:
            raise RuntimeError("error code=-6")
        return _Struct(lambdaAD=800.0)

    def set_hdad(self, s):
        self.written["hdad"] = s.lambdaAD


def test_build_flattens_group_field_with_range():
    opts = {o.option_id: o for o in build_advanced_options(_FakeAM())}
    o = opts["ADV_depth_control_deepSeaSecondPeakThreshold"]
    assert o.category == "Advanced Controls"
    assert o.filter_name == "Depth Control"
    assert o.name == "Deep Sea Second Peak Threshold"
    assert (o.current_value, o.min_value, o.max_value, o.step) == (325.0, 0.0, 1023.0, 1.0)


def test_bool_field_is_min0_max1_step1():
    opts = {o.option_id: o for o in build_advanced_options(_FakeAM())}
    o = opts["ADV_color_control_disableSADColor"]
    assert (o.min_value, o.max_value, o.step) == (0.0, 1.0, 1.0)  # frontend renders as checkbox


def test_set_patches_field_and_coerces_int():
    am = _FakeAM()
    assert set_advanced_option(am, "ADV_depth_control_deepSeaSecondPeakThreshold", 500.4) is True
    assert am.written["depth_control"] == 500  # int-coerced


def test_field_without_range_is_still_exposed():
    # HDAD supports current value but not min/max -> still shown, ranges None
    opts = {o.option_id: o for o in build_advanced_options(_FakeAM())}
    o = opts["ADV_hdad_lambdaAD"]
    assert o.current_value == 800.0
    assert o.min_value is None and o.max_value is None and o.step is None


def test_unknown_option_raises_404():
    with pytest.raises(RealSenseError) as exc:
        set_advanced_option(_FakeAM(), "ADV_depth_control_nope", 1)
    assert exc.value.status_code == 404


class _FakeDevice:
    sensors = [object()]


def _mgr_with_advanced_mode(am):
    """A manager whose only wired-up piece is advanced-mode lookup on device 'dev'."""
    mgr = RealSenseManager.__new__(RealSenseManager)  # bypass __init__ (no rs.context)
    mgr.devices = {"dev": _FakeDevice()}
    mgr._get_advanced_mode = staticmethod(lambda _dev: am)
    return mgr


def test_set_adv_option_while_disabled_raises_400():
    am = _FakeAM()
    am.is_enabled = lambda: False
    with pytest.raises(RealSenseError) as exc:
        _mgr_with_advanced_mode(am).set_sensor_option(
            "dev", "sensor-0", "ADV_depth_control_deepSeaSecondPeakThreshold", 500
        )
    assert exc.value.status_code == 400
    assert "disabled" in exc.value.detail
    assert am.written == {}  # never reached the SDK


def test_set_adv_option_when_unsupported_raises_400():
    with pytest.raises(RealSenseError) as exc:
        _mgr_with_advanced_mode(None).set_sensor_option(
            "dev", "sensor-0", "ADV_depth_control_deepSeaSecondPeakThreshold", 500
        )
    assert exc.value.status_code == 400
    assert "not supported" in exc.value.detail


def test_set_adv_option_when_enabled_writes_through():
    am = _FakeAM()
    am.is_enabled = lambda: True
    assert _mgr_with_advanced_mode(am).set_sensor_option(
        "dev", "sensor-0", "ADV_depth_control_deepSeaSecondPeakThreshold", 500
    ) is True
    assert am.written["depth_control"] == 500
