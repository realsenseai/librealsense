# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from collections import namedtuple

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


def test_an_option_without_a_value_is_left_out():
    assert {k: o.current_value for k, o in options.all_options(_Sensor()).items()} == {"exposure": 8500.0}
