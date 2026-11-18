# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from collections import namedtuple
import pytest
from app.services.camera import Camera

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

    def get_option(self, opt):
        return self.written.get(opt.name, 0.0)

    def get_option_description(self, opt):
        return f"{opt.name} description"

    def get_option_value_description(self, opt, val):
        return None

    def is_option_read_only(self, opt):
        return False

    def set_option(self, opt, v):
        rng = self.get_option_range(opt)
        if not rng.min <= v <= rng.max:  # as the SDK does, rather than moving the value
            raise RuntimeError(f"value {v} is out of range [{rng.min}, {rng.max}]")
        self.written[opt.name] = v


def _camera_with_colorizer():
    camera = Camera.__new__(Camera)  # bypass __init__ (no SDK device)
    camera.colorizer = _FakeColorizer()
    return camera


def test_get_colorizer_options_reports_ranges():
    opts = {o.option_id: o for o in _camera_with_colorizer().colorizer_options()}
    assert (opts["color_scheme"].min_value, opts["color_scheme"].max_value) == (0, 9)
    assert opts["min_distance"].step == 0.1


def test_set_colorizer_option_writes_and_returns_applied():
    camera = _camera_with_colorizer()
    applied = camera.set_colorizer_option("color_scheme", 3)
    assert camera.colorizer.written["color_scheme"] == 3.0
    assert applied.current_value == 3.0


def test_set_colorizer_option_out_of_range_is_left_to_the_device():
    camera = _camera_with_colorizer()
    with pytest.raises(RuntimeError):  # the SDK's own rejection, not translated here
        camera.set_colorizer_option("min_distance", 999)
    assert "min_distance" not in camera.colorizer.written


def test_set_colorizer_option_unknown_raises():
    camera = _camera_with_colorizer()
    with pytest.raises(KeyError):  # answered 404
        camera.set_colorizer_option("nope", 1)
