# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from collections import namedtuple
import pytest
from app.services.rs_manager import RealSenseManager, RealSenseError

Range = namedtuple("Range", ["min", "max", "step", "default"])


class _Opt:
    def __init__(self, name):
        self.name = name


class _FakeColorizer:
    def __init__(self):
        self.written = {}

    def get_supported_options(self):
        return [_Opt("color_scheme"), _Opt("min_distance")]

    def get_option_range(self, opt):
        return Range(0, 9, 1, 0) if opt.name == "color_scheme" else Range(0.0, 16.0, 0.1, 0.0)

    def set_option(self, opt, v):
        self.written[opt.name] = v


def _mgr_with_colorizer():
    mgr = RealSenseManager.__new__(RealSenseManager)  # bypass __init__ (no rs.context)
    mgr.colorizers = {"dev": _FakeColorizer()}
    return mgr


def test_set_colorizer_option_routes_and_coerces():
    mgr = _mgr_with_colorizer()
    assert mgr._set_colorizer_option("dev", "VIZ_color_scheme", 3) is True
    assert mgr.colorizers["dev"].written["color_scheme"] == 3.0


def test_set_colorizer_option_clamps_to_range():
    mgr = _mgr_with_colorizer()
    mgr._set_colorizer_option("dev", "VIZ_min_distance", 999)
    assert mgr.colorizers["dev"].written["min_distance"] == 16.0


def test_set_colorizer_option_unknown_raises_404():
    mgr = _mgr_with_colorizer()
    with pytest.raises(RealSenseError) as exc:
        mgr._set_colorizer_option("dev", "VIZ_nope", 1)
    assert exc.value.status_code == 404
