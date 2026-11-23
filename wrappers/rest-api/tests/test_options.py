# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from collections import namedtuple

import pytest

from app.services import options

Range = namedtuple("Range", ["min", "max", "step", "default"])
OptionValue = namedtuple("OptionValue", ["id", "value", "range"])


class _Opt:
    def __init__(self, name):
        self.name = name


class _Sensor:
    """A sensor with an option it cannot read now, as a temperature while not streaming"""

    def get_supported_option_values(self):
        return [OptionValue(_Opt("exposure"), 8500.0, Range(1, 165000, 1, 8500)),
                OptionValue(_Opt("asic_temperature"), None, Range(-40, 125, 0, 0))]

    def get_option_description(self, opt):
        return opt.name

    def get_option_value_description(self, opt, val):
        return None

    def is_option_read_only(self, opt):
        return False

    def get_supported_options(self):
        return [o.id for o in self.get_supported_option_values()]

    def get_option_value(self, opt):
        return next(o for o in self.get_supported_option_values() if o.id.name == opt.name)

    def get_option_range(self, opt):
        return self.get_option_value(opt).range


def test_an_option_it_cannot_read_now_is_listed_without_a_value():
    assert {k: o.current_value for k, o in options.all_options(_Sensor()).items()} == {
        "exposure": 8500.0, "asic_temperature": None}


def test_one_option_it_cannot_read_now_answers_without_a_value():
    assert options.option(_Sensor(), "asic_temperature").current_value is None
    assert options.option(_Sensor(), "exposure").current_value == 8500.0


def test_one_option_it_does_not_support_raises():
    with pytest.raises(KeyError):  # answered 404
        options.option(_Sensor(), "nope")


class _SensorWithoutRange(_Sensor):
    """One whose range the SDK could not read: the options list has no range for it, its own read raises"""

    def get_supported_option_values(self):
        return [OptionValue(_Opt("ambient_light"), 1.0, None)]

    def get_option_range(self, opt):
        raise RuntimeError("failed to query range")


def test_an_option_without_a_range_is_listed_with_null_range():
    o = options.all_options(_SensorWithoutRange())["ambient_light"]
    assert (o.current_value, o.min_value, o.max_value, o.step, o.default_value) == (1.0, None, None, None, None)
    assert options.option(_SensorWithoutRange(), "ambient_light") == o
