# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Live feature checks that assert outcomes, not just status codes: frames actually flow,
metadata carries live values, ROI round-trips, controls read, and streaming still works after
a stop/start cycle and after an on-chip calibration - the sequence a user goes through."""

import time

import pytest

import main  # noqa: F401  # repo-built pyrealsense2
from app.models.sensor_streaming import SensorStreamConfig
from app.models.stream import Resolution


def _wait(predicate, timeout=6.0, step=0.1):
    deadline = time.time() + timeout
    while time.time() < deadline:
        value = predicate()
        if value:
            return value
        time.sleep(step)
    return predicate()


def _depth_config(manager, device_id):
    for sensor in manager.get_sensors(device_id):
        for profile in sensor.supported_stream_profiles:
            if profile.stream_type.lower() == "depth":
                d = profile.default
                w, h = d.resolution if d else profile.resolutions[0]
                return sensor.sensor_id, SensorStreamConfig(stream_type="depth", format=d.format if d else profile.formats[0],
                                                             resolution=Resolution(width=w, height=h), framerate=d.fps if d else profile.fps[0])
    pytest.skip("no depth stream")


def _frames_flow(manager, device_id, timeout=6.0):
    """Depth at the image centre becomes readable and the frame number keeps moving."""
    first = _wait(lambda: manager.get_latest_metadata(device_id, "depth").get("frame_number") if manager.depth_frames.get(device_id) else None, timeout)
    if not first:
        return False
    later = _wait(lambda: (manager.get_latest_metadata(device_id, "depth").get("frame_number") or 0) > first, 3.0)
    return bool(later)


class TestLiveStreams:
    @pytest.fixture
    def manager(self):
        from app.services.socketio import sio
        from app.services.rs_manager import RealSenseManager
        m = RealSenseManager(sio)
        assert m.get_devices(), "no camera"
        yield m
        for device in m.get_devices():
            for sid in list(m.sensor_streams.get(device.device_id, {}).keys()):
                try:
                    m.stop_sensor(device.device_id, sid)
                except Exception:
                    pass

    def test_frames_flow_and_survive_stop_start_cycles(self, manager):
        device = manager.get_devices()[0]
        sensor_id, cfg = _depth_config(manager, device.device_id)
        for cycle in range(3):
            manager.start_sensor(device.device_id, sensor_id, [cfg])
            assert _frames_flow(manager, device.device_id), f"no depth frames on cycle {cycle}"
            md = manager.get_latest_metadata(device.device_id, "depth")
            assert md["hardware_fps"] > 0 and md["hardware_width"] == cfg.resolution.width  # width may be decimated by filters
            assert md.get("stats") is not None
            manager.stop_sensor(device.device_id, sensor_id)
            assert manager.get_sensor_status(device.device_id, sensor_id).is_streaming is False

    def test_depth_readout_roi_and_controls_while_streaming(self, manager):
        device = manager.get_devices()[0]
        sensor_id, cfg = _depth_config(manager, device.device_id)
        manager.start_sensor(device.device_id, sensor_id, [cfg])
        assert _frames_flow(manager, device.device_id)

        # Depth readout: something in the scene has depth
        w, h = cfg.resolution.width, cfg.resolution.height
        readings = [manager.get_depth_at_pixel(device.device_id, x, y) for x in range(w // 4, w, w // 4) for y in range(h // 4, h, h // 4)]
        assert any(r for r in readings if r), "depth_at_pixel returned nothing anywhere"

        # ROI round trip
        roi = manager.get_roi(device.device_id, sensor_id)
        assert roi["supported"] is True
        applied = manager.set_roi(device.device_id, sensor_id, 100, 80, 500, 300)
        assert (applied["min_x"], applied["min_y"], applied["max_x"], applied["max_y"]) == (100, 80, 500, 300)
        again = manager.get_roi(device.device_id, sensor_id)
        assert (again["min_x"], again["max_x"]) == (100, 500)
        manager.set_roi(device.device_id, sensor_id, roi["min_x"], roi["min_y"], roi["max_x"], roi["max_y"])

        # Controls read while streaming
        options = manager.get_sensor_options(device.device_id, sensor_id)
        assert any(o.option_id == "exposure" for o in options)

    def test_on_chip_calibration_leaves_streaming_healthy(self, manager):
        device = manager.get_devices()[0]
        sensor_id, cfg = _depth_config(manager, device.device_id)
        manager.start_sensor(device.device_id, sensor_id, [cfg])
        assert _frames_flow(manager, device.device_id)

        job = manager.start_on_chip_calibration(device.device_id, {})
        done = _wait(lambda: manager.jobs.get(job.id).info if manager.jobs.get(job.id).info.state != "running" else None, timeout=60)
        assert done is not None, "calibration job did not finish"
        assert done.state in ("done", "failed")
        if done.state == "failed":
            assert done.error  # the firmware's reason (bad scene) is reported

        # The workspace put the original stream back and it delivers frames
        assert _wait(lambda: manager.get_sensor_status(device.device_id, sensor_id).is_streaming, 10)
        assert _frames_flow(manager, device.device_id, timeout=10), "no depth frames after calibration"
