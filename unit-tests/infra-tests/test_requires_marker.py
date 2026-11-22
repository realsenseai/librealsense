# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Tests for the ``requires(fw=, option=, sensor=)`` marker: FW version parsing, the per-camera evaluation done by
module_device_setup before it powers the camera (unmet_requirement), and the collection-time marker check.
"""

import sys
import types
import pytest
from rspy import devices
from rspy.pytest.device_helpers import parse_fw_version, unmet_requirement
from rspy.pytest.collection import assert_requires_markers_valid


@pytest.fixture(autouse=True)
def fake_pyrealsense2(monkeypatch):
    rs = types.SimpleNamespace(camera_info=types.SimpleNamespace(firmware_version='fw', name='name'))
    monkeypatch.setitem(sys.modules, 'pyrealsense2', rs)


@pytest.fixture(autouse=True)
def inventory(monkeypatch):
    """The cameras devices.by_spec resolves fw= specs against, exactly as for device_each."""
    monkeypatch.setattr(devices, '_device_by_sn', {})
    return devices._device_by_sn


class FakeSensor:
    def __init__(self, name, options):
        self._name, self._options = name, options

    def get_info(self, info):
        return self._name

    def supports(self, option):
        return option in self._options


class FakeHandle:
    def __init__(self, fw, sensors, recovery=False):
        self._fw, self._sensors, self._recovery = fw, sensors, recovery

    def is_in_recovery_mode(self):
        return self._recovery

    def supports(self, info):
        return self._fw is not None

    def get_info(self, info):
        return self._fw

    def query_sensors(self):
        return [FakeSensor(n, o) for n, o in self._sensors.items()]


def camera(name='D555', fw='7.60.47006.17770', recovery=False):
    """A camera added to the rspy.devices inventory"""
    sensors = {'Stereo Module': {'emitter'}, 'Motion Module': {'gyro'}}
    dev = types.SimpleNamespace(name=name, serial_number=f"sn-{name}", product_line=name[:2] + "00",
                                handle=FakeHandle(fw, sensors, recovery))
    devices._device_by_sn[dev.serial_number] = dev
    return dev


def unmet(dev, **flags):
    return unmet_requirement(dev, [pytest.mark.requires(**flags).mark])


class TestParseFwVersion:

    @pytest.mark.parametrize("text, expected", [
        ("7.62", (7, 62, 0, 0)),
        ("7.62.40621", (7, 62, 40621, 0)),
        ("7.62.40621.11153", (7, 62, 40621, 11153)),
    ])
    def test_short_and_full(self, text, expected):
        assert parse_fw_version(text) == expected

    @pytest.mark.parametrize("text", ["7", "7.62.1.2.3", "7.62a", "", "v7.62"])
    def test_malformed(self, text):
        with pytest.raises(ValueError):
            parse_fw_version(text)


class TestUnmetRequirement:

    def test_no_markers_never_reads_the_device(self):
        assert unmet_requirement(types.SimpleNamespace(name='D555'), []) is None

    def test_fw_below_minimum(self):
        assert unmet(camera(), fw="7.62") == "D555 FW 7.60.47006.17770 is below the required 7.62"

    @pytest.mark.parametrize("minimum", ["7.60", "7.60.47006", "7.60.47006.17770"])
    def test_fw_at_short_or_full_minimum(self, minimum):
        assert unmet(camera(), fw=minimum) is None

    @pytest.mark.parametrize("name", ["D585", "D585 Prototype", "D585F"])
    def test_fw_per_camera_matches_like_device_each(self, name):
        minimums = {"D585S": None, "D585": "7.62"}
        assert unmet(camera(name, fw="7.61.0.0"), fw=minimums) == f"{name} FW 7.61.0.0 is below the required 7.62"

    def test_fw_per_camera_first_match_wins(self):
        assert unmet(camera('D585S', fw="7.50.0.0"), fw={"D585S": None, "D585": "7.62"}) is None
        assert unmet(camera('D585S', fw="7.50.0.0"), fw={"D585": "7.62", "D585S": None}) is not None

    def test_fw_per_camera_product_line_and_unmatched(self):
        minimums = {"D400*": "5.17"}
        assert unmet(camera('D455', fw="5.16.0.10"), fw=minimums) == "D455 FW 5.16.0.10 is below the required 5.17"
        assert unmet(camera('D555', fw="1.0"), fw=minimums) is None   # no spec matches -> no minimum

    def test_option(self):
        assert unmet(camera(), option='gyro') is None
        assert unmet(camera(), option='accel') == "D555 has no accel option"

    def test_option_any_of(self):
        assert unmet(camera(), option=['accel', 'gyro']) is None
        assert unmet(camera(), option=('accel', 'hdr')) == "D555 has no accel or hdr option"

    def test_sensor(self):
        assert unmet(camera(), sensor='Motion Module') is None
        assert unmet(camera(), sensor='Perception') == "D555 has no Perception sensor"

    def test_every_given_flag_must_hold(self):
        assert unmet(camera(), fw="7.60", option='gyro', sensor='Motion Module') is None
        assert unmet(camera(), fw="7.60", option='accel', sensor='Motion Module') == "D555 has no accel option"

    def test_markers_stack(self):
        markers = [pytest.mark.requires(fw="7.60").mark, pytest.mark.requires(sensor='Perception').mark]
        assert unmet_requirement(camera(), markers) == "D555 has no Perception sensor"

    @pytest.mark.parametrize("kind", ["recovery", "no FW info", "not enumerated"])
    def test_unknown_camera_is_not_skipped(self, kind):
        dev = {"recovery": lambda: camera(recovery=True), "no FW info": lambda: camera(fw=None),
               "not enumerated": lambda: None}[kind]()
        assert unmet(dev, fw="99", sensor='Perception') is None


def item_with(marker, module_level=True):
    mark = marker.mark
    module = types.SimpleNamespace(own_markers=[mark] if module_level else [])
    return types.SimpleNamespace(nodeid="live/pytest-x.py::test_x", getparent=lambda cls: module,
                                 iter_markers=lambda name: iter([mark]))


class TestRequiresMarkerValidation:

    @pytest.mark.parametrize("marker", [
        pytest.mark.requires(fw="7.62"),
        pytest.mark.requires(fw={"D585S": None, "D585": "7.62.40621.11153", "D400*": "5.17"}, option='gyro'),
        pytest.mark.requires(sensor='Motion Module'),
    ])
    def test_valid(self, marker):
        assert_requires_markers_valid([item_with(marker)])

    @pytest.mark.parametrize("marker, module_level", [
        (pytest.mark.requires(fw="7.62"), False),            # function-level: module_device_setup can't see it
        (pytest.mark.requires("7.62"), True),                # positional
        (pytest.mark.requires(firmware="7.62"), True),       # unknown flag
        (pytest.mark.requires(fw="7.62b"), True),            # bad version
        (pytest.mark.requires(fw={"D555": "7"}), True),      # bad per-camera version
        (pytest.mark.requires(fw={"D585": "7.62", "D585S": None}), True),   # "D585" matches D585S first
        (pytest.mark.requires(fw={"d585": "7.62", "D585S": None}), True),   # name match ignores case
        (pytest.mark.requires(fw={"D500*": "7.60", "D585": "7.62"}), True),  # the D500 line matches D585 first
    ])
    def test_invalid(self, marker, module_level):
        with pytest.raises(pytest.UsageError, match="bad requires marker"):
            assert_requires_markers_valid([item_with(marker, module_level)])
