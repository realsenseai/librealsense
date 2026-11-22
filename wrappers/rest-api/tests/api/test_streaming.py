# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from .conftest import STREAM_CONFIG, client


def test_start_stream(setup_mock_managers):
    response = client.post("/api/v1/devices/device1/stream/start", json=STREAM_CONFIG)
    assert response.status_code == 200
    setup_mock_managers["rs_manager"].cameras["device1"].pipeline.start.assert_called_once()


def test_start_stream_defaults_to_any(setup_mock_managers):
    response = client.post("/api/v1/devices/device1/stream/start", json=[{"stream": "depth"}])
    assert response.status_code == 200


def test_start_stream_rejects_unknown_stream(setup_mock_managers):
    config = [{**STREAM_CONFIG[0], "stream": "infrared-1"}]
    response = client.post("/api/v1/devices/device1/stream/start", json=config)
    assert response.status_code == 422


def test_start_stream_sdk_refusal_is_400(setup_mock_managers):
    def refuse(configs):
        raise RuntimeError("start() cannot be called before stop()")
    setup_mock_managers["rs_manager"].cameras["device1"].start_stream = refuse
    response = client.post("/api/v1/devices/device1/stream/start", json=STREAM_CONFIG)
    assert response.status_code == 400
    assert response.json()["detail"] == "start() cannot be called before stop()"


def test_stop_stream_unknown_device_is_404(setup_mock_managers):
    response = client.post("/api/v1/devices/unknown/stream/stop")
    assert response.status_code == 404


def test_stop_stream(setup_mock_managers):
    client.post("/api/v1/devices/device1/stream/start", json=STREAM_CONFIG)
    response = client.post("/api/v1/devices/device1/stream/stop")
    assert response.status_code == 200


def test_sensor_reports_the_profiles_it_streams(setup_mock_managers):
    def depth_sensor():
        return next(s for s in client.get("/api/v1/devices/device1/sensors/").json()
                    if any(p["stream_type"] == "depth" for p in s["stream_profiles"]))

    sensor = depth_sensor()
    assert sensor["active_streams"] == []
    profile = next(p for p in sensor["stream_profiles"] if p["stream_type"] == "depth")
    url = f"/api/v1/devices/device1/sensors/{sensor['sensor_index']}"
    opened = {k: v for k, v in profile.items() if k != "default"}  # the SDK's to list, not the client's to send
    assert client.post(f"{url}/open", json={"profiles": [opened]}).status_code == 200
    assert client.post(f"{url}/start").status_code == 200
    assert depth_sensor()["active_streams"] == [profile]
    assert client.post(f"{url}/stop").status_code == 200
    assert client.post(f"{url}/close").status_code == 200
    assert depth_sensor()["active_streams"] == []


def test_extrinsics_between_the_device_streams(setup_mock_managers):
    response = client.get("/api/v1/devices/device1/extrinsics")
    assert response.status_code == 200
    assert {(e["stream_type"], e["stream_index"]) for e in response.json()} >= {("depth", 0)}


def test_stopping_a_sensor_the_pipeline_runs_stops_it(setup_mock_managers):
    sensor = setup_mock_managers["rs_manager"].cameras["device1"].dev.sensors[0]
    sensor._active = sensor.get_stream_profiles()[:1]  # opened by the pipeline, not the sensor API
    assert client.post("/api/v1/devices/device1/sensors/0/stop").status_code == 200
    assert client.post("/api/v1/devices/device1/sensors/0/close").status_code == 200
    assert not sensor.get_active_streams()


def test_every_start_and_close_tells_the_clients(setup_mock_managers):
    sensor = next(s for s in client.get("/api/v1/devices/device1/sensors/").json()
                  if any(p["stream_type"] == "depth" for p in s["stream_profiles"]))
    profile = next(p for p in sensor["stream_profiles"] if p["stream_type"] == "depth")
    url = f"/api/v1/devices/device1/sensors/{sensor['sensor_index']}"
    with client.websocket_connect("/api/v1/events") as events:
        for step in ("open", "start", "stop", "close"):
            client.post(f"{url}/{step}", json={"profiles": [profile]} if step == "open" else None)
        changes = [events.receive_json() for _ in range(2)]
    assert [(m["event"], m["device_id"], m["sensors"][sensor["sensor_index"]]) for m in changes] == [
        ("streaming_changed", "device1", [profile]), ("streaming_changed", "device1", [])]


def test_a_refused_open_leaves_the_other_owner_streaming(setup_mock_managers):
    sensor = setup_mock_managers["rs_manager"].cameras["device1"].dev.sensors[0]
    running = sensor.get_stream_profiles()[:1]
    sensor._active = running  # another client holds it

    def refuse(profiles):
        raise RuntimeError("open() failed. UVC device is already opened!")
    sensor.open = refuse
    row = client.get("/api/v1/devices/device1/sensors/").json()[0]["stream_profiles"][0]
    response = client.post("/api/v1/devices/device1/sensors/0/open", json={"profiles": [row]})
    assert response.status_code == 400  # the SDK's error
    assert sensor.get_active_streams() == running
