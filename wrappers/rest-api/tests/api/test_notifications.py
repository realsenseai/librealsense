# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from .conftest import client


class _Notification:
    category = "notification_category.hardware_event"
    severity = "log_severity.info"
    description = "Laser turned on"
    serialized_data = '{"Event":"laser"}'
    timestamp = 1234.5


def test_sdk_notifications_reach_clients_and_the_console(setup_mock_managers):
    rs_manager = setup_mock_managers["rs_manager"]
    emitted = []
    rs_manager._emit_socket_event = lambda ev, payload: emitted.append((ev, payload))
    dev = rs_manager.devices["device1"]
    rs_manager._watch_notifications("device1", dev)

    dev.sensors[0]._notifications_callback(_Notification())

    assert ("notification", {
        "device_id": "device1", "sensor_id": "device1-sensor-0", "category": "hardware_event", "severity": "info",
        "description": "Laser turned on", "serialized_data": '{"Event":"laser"}', "timestamp": 1234.5,
    }) in emitted
    entries = client.get("/api/v1/logs/").json()
    assert entries[-1]["source"] == "notification" and entries[-1]["category"] == "hardware_event"
