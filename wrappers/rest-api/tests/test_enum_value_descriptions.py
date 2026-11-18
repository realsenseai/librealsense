# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from collections import namedtuple
from app.services import options

Range = namedtuple("Range", ["min", "max", "step", "default"])


class _FakeObj:
    """Stand-in for an rs sensor/filter: maps int value -> description, None if unlabelled."""
    def __init__(self, descs):
        self._descs = descs
        self.probes = 0

    def get_option_value_description(self, opt, val):
        self.probes += 1
        return self._descs.get(int(val))


_harvest = options._value_descriptions


def test_reports_a_label_for_every_value():
    obj = _FakeObj({0: "Custom", 1: "Default", 2: "Hand", 3: "High Accuracy"})
    assert _harvest(obj, object(), Range(0, 3, 1.0, 0)) == {
        "0": "Custom", "1": "Default", "2": "Hand", "3": "High Accuracy"
    }


def test_reports_the_labels_it_found_when_they_run_out():
    # The viewer decides what a partial set means; the backend does not discard it.
    obj = _FakeObj({0: "Off", 1: "On"})
    assert _harvest(obj, object(), Range(0, 2, 1.0, 0)) == {"0": "Off", "1": "On"}


def test_unlabelled_option_stops_at_the_first_value():
    obj = _FakeObj({})  # a slider: exposure-sized range, no labels at all
    assert _harvest(obj, object(), Range(1, 165000, 1.0, 8500)) == {}
    assert obj.probes == 1


def test_non_integer_step_is_not_probed():
    obj = _FakeObj({0: "a"})
    assert _harvest(obj, object(), Range(0.0, 1.0, 0.1, 0.0)) == {}
    assert obj.probes == 0


def test_non_integer_bounds_is_not_probed():
    obj = _FakeObj({0: "a"})
    assert _harvest(obj, object(), Range(0.5, 3.5, 1.0, 0.5)) == {}
    assert obj.probes == 0
