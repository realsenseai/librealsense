# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from collections import namedtuple
from app.services.rs_manager import RealSenseManager

Range = namedtuple("Range", ["min", "max", "step", "default"])


class _FakeObj:
    """Stand-in for an rs sensor/filter: maps int value -> description."""
    def __init__(self, descs):
        self._descs = descs

    def get_option_value_description(self, opt, val):
        d = self._descs.get(int(val))
        if d is None:
            raise RuntimeError("no description")
        return d


_harvest = RealSenseManager._enum_value_descriptions


def test_full_enum_returns_all_descriptions():
    obj = _FakeObj({0: "Custom", 1: "Default", 2: "Hand", 3: "High Accuracy"})
    rng = Range(0, 3, 1.0, 0)
    assert _harvest(obj, object(), rng) == {"0": "Custom", "1": "Default", "2": "Hand", "3": "High Accuracy"}


def test_partial_enum_returns_none_early_exit():
    # value 2 has no description -> not a real enum
    obj = _FakeObj({0: "Off", 1: "On"})
    rng = Range(0, 2, 1.0, 0)
    assert _harvest(obj, object(), rng) is None


def test_wide_range_is_capped_not_probed():
    obj = _FakeObj({})  # would raise if probed
    rng = Range(1, 165000, 1.0, 8500)
    assert _harvest(obj, object(), rng) is None


def test_non_integer_step_is_not_enum():
    obj = _FakeObj({0: "a"})
    rng = Range(0.0, 1.0, 0.1, 0.0)
    assert _harvest(obj, object(), rng) is None


def test_non_integer_bounds_is_not_enum():
    obj = _FakeObj({0: "a"})
    rng = Range(0.5, 3.5, 1.0, 0.5)
    assert _harvest(obj, object(), rng) is None


def test_object_without_the_description_api_is_not_enum():
    # Older pyrealsense2 builds (and processing blocks) lack the method entirely.
    # It must read as "no enum", not blow up the caller's whole option list.
    class _NoDescribe:
        pass

    assert _harvest(_NoDescribe(), object(), Range(0, 3, 1.0, 0)) is None


def test_description_probe_error_is_not_enum():
    class _Boom:
        def get_option_value_description(self, opt, val):
            raise ValueError("not a RuntimeError")

    assert _harvest(_Boom(), object(), Range(0, 3, 1.0, 0)) is None
