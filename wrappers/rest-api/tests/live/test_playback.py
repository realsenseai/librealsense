# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Live record/playback round trip against a connected camera (WP3.4).

Records a few seconds of depth to a temporary .db3, loads it back as a playback device,
drives the transport (play, pause, step, seek, speed, repeat) and unloads it.
"""

import time

import pytest

import main  # noqa: F401  # selects the repo-built pyrealsense2 (playback speed/stop bindings)
from app.models.sensor_streaming import SensorStreamConfig
from app.models.stream import Resolution


def _depth_sensor(manager, device_id):
    for sensor in manager.get_sensors(device_id):
        for profile in sensor.supported_stream_profiles:
            if profile.stream_type.lower() == "depth":
                return sensor, profile
    pytest.skip(f"{device_id} has no depth stream")


def _depth_config(sensor, profile) -> SensorStreamConfig:
    default = profile.default
    width, height = default.resolution if default else profile.resolutions[0]
    return SensorStreamConfig(stream_type="depth",
                              format=default.format if default else profile.formats[0],
                              resolution=Resolution(width=width, height=height),
                              framerate=default.fps if default else profile.fps[0])


def _wait(predicate, timeout=5.0, step=0.1):
    deadline = time.time() + timeout
    while time.time() < deadline:
        value = predicate()
        if value:
            return value
        time.sleep(step)
    return predicate()


class TestLivePlayback:
    @pytest.fixture
    def manager(self):
        from app.services.socketio import sio
        from app.services.rs_manager import RealSenseManager
        manager = RealSenseManager(sio)
        assert manager.get_devices(), "RealSenseManager sees no devices"
        return manager

    def test_record_then_play_back(self, manager, tmp_path):
        device = next(d for d in manager.get_devices() if not d.is_playback)
        sensor, profile = _depth_sensor(manager, device.device_id)
        config = _depth_config(sensor, profile)
        clip = tmp_path / "clip.db3"

        manager.start_sensor(device.device_id, sensor.sensor_id, [config])
        try:
            time.sleep(1.0)
            status = manager.start_recording(device.device_id, str(clip))
            assert status.recording and status.file == str(clip)
            time.sleep(3.0)
            paused = manager.set_recording_paused(device.device_id, True)
            assert paused.paused
            manager.set_recording_paused(device.device_id, False)
            stopped = manager.stop_recording(device.device_id)
            assert not stopped.recording
        finally:
            manager.stop_sensor(device.device_id, sensor.sensor_id)
        assert clip.stat().st_size > 0

        info = manager.load_playback(str(clip))
        pid = info.device_id
        assert info.is_playback and pid != device.device_id
        try:
            status = manager.get_playback_status(pid)
            assert status.duration_ns > 1_000_000_000, "expected a clip of at least a second"

            psensor, pprofile = _depth_sensor(manager, pid)
            manager.start_sensor(pid, psensor.sensor_id, [_depth_config(psensor, pprofile)])
            playing = manager.playback_control(pid, "play", None)
            assert playing.state in ("playing", "paused")
            advanced = _wait(lambda: manager.get_playback_status(pid).position_ns > 0, timeout=5.0)
            assert advanced, "playback position did not advance"

            paused = manager.playback_control(pid, "pause", None)
            assert paused.state == "paused"
            position = paused.position_ns

            stepped = manager.playback_control(pid, "step", 1)
            assert stepped.position_ns >= position

            sought = manager.playback_control(pid, "seek", 0)
            assert sought.position_ns <= stepped.position_ns

            assert manager.playback_control(pid, "speed", 2.0).speed == 2.0
            assert manager.playback_control(pid, "repeat", 1).repeat is True
            assert manager.playback_control(pid, "stop", None).state in ("stopped", "paused", "playing")
        finally:
            manager.unload_playback(pid)
        assert pid not in {d.device_id for d in manager.get_devices()}
