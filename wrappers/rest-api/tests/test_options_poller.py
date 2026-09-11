# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import threading

from app.services.options_poller import OptionsPoller
from tests.mocks.pyrealsense_mock import create_mock_device, option


def _poller(devices):
    emitted = []
    lock = threading.Lock()
    poller = OptionsPoller(lambda: devices.items(), lambda _d: lock, lambda ev, p: emitted.append((ev, p)))
    return poller, emitted


def test_first_poll_only_learns_and_later_polls_report_what_moved():
    dev = create_mock_device("d1", "cam")
    poller, emitted = _poller({"d1": dev})

    poller.poll_once()
    assert emitted == []

    dev.sensors[0]._options[option.laser_power] = 90
    poller.poll_once()
    assert emitted == [("options_changed", {
        "device_id": "d1", "sensor_id": "d1-sensor-0",
        "options": [{"option_id": "laser_power", "current_value": 90}],
    })]

    poller.poll_once()
    assert len(emitted) == 1  # nothing moved since


def test_values_the_server_wrote_are_not_reported():
    dev = create_mock_device("d1", "cam")
    poller, emitted = _poller({"d1": dev})
    poller.poll_once()

    dev.sensors[0]._options[option.laser_power] = 90
    poller.note_written("d1", "d1-sensor-0", "laser_power", 90)
    poller.poll_once()
    assert emitted == []


def test_a_sensor_that_fails_to_read_is_skipped_not_fatal():
    dev = create_mock_device("d1", "cam")
    poller, emitted = _poller({"d1": dev})
    poller.poll_once()
    dev.sensors[1].get_supported_options = lambda: (_ for _ in ()).throw(RuntimeError("busy"))
    dev.sensors[0]._options[option.laser_power] = 90
    poller.poll_once()
    assert [p["sensor_id"] for _, p in emitted] == ["d1-sensor-0"]


def test_paused_keeps_the_poller_off_the_device_until_released():
    dev = create_mock_device("d1", "cam")
    poller, emitted = _poller({"d1": dev})
    poller.poll_once()
    dev.sensors[0]._options[option.laser_power] = 90

    started = threading.Event()
    with poller.paused():
        t = threading.Thread(target=lambda: (started.set(), poller.poll_once()))
        t.start()
        started.wait(1)
        t.join(0.3)
        assert t.is_alive() and emitted == []  # blocked behind the pause
    t.join(2)
    assert not t.is_alive() and len(emitted) == 1


def test_the_device_lock_is_taken_per_option_not_per_sweep():
    dev = create_mock_device("d1", "cam")
    acquisitions = []

    class CountingLock:
        def __enter__(self):
            acquisitions.append(1)
        def __exit__(self, *exc):
            return False

    poller = OptionsPoller(lambda: {"d1": dev}.items(), lambda _d: CountingLock(), lambda ev, p: None)
    poller.poll_once()
    options = sum(len(s.get_supported_options()) for s in dev.sensors)
    assert len(acquisitions) == options > 1


def test_forget_drops_a_device_so_a_replug_starts_fresh():
    dev = create_mock_device("d1", "cam")
    poller, emitted = _poller({"d1": dev})
    poller.poll_once()
    poller.forget("d1")
    dev.sensors[0]._options[option.laser_power] = 90
    poller.poll_once()
    assert emitted == []
