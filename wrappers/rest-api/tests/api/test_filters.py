# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from .conftest import client

FILTERS = "/api/v1/devices/device1/sensors/device1-sensor-0/filters/"


def test_defaults_follow_the_legacy_viewer(setup_mock_managers):
    filters = client.get(FILTERS).json()
    # HDR Merge needs sequence ids, which the mock depth sensor does not offer.
    assert set(filters) == {"Decimation Filter", "Threshold Filter", "Spatial Filter", "Temporal Filter", "Hole Filling Filter"}
    on = {name for name, f in filters.items() if f["enabled"]}
    assert on == {"Decimation Filter", "Spatial Filter", "Temporal Filter"}
    assert filters["Hole Filling Filter"]["default_enabled"] is False


def test_decimation_starts_off_on_a_color_sensor(setup_mock_managers):
    rs_manager = setup_mock_managers["rs_manager"]
    color = rs_manager.devices["device1"].sensors[1]
    color.get_recommended_filters = lambda: rs_manager.devices["device1"].sensors[0].get_recommended_filters()[:1]
    filters = client.get("/api/v1/devices/device1/sensors/device1-sensor-1/filters/").json()
    assert filters["Decimation Filter"]["enabled"] is False


def test_performance_mode_starts_every_filter_off(setup_mock_managers):
    setup_mock_managers["rs_manager"].settings.update({"post_processing": {"performance_mode": True}})
    filters = client.get(FILTERS).json()
    assert not any(f["enabled"] for f in filters.values())


def test_enable_choice_is_remembered_for_the_next_run(setup_mock_managers):
    rs_manager = setup_mock_managers["rs_manager"]
    assert client.put(f"{FILTERS}Hole Filling Filter/enabled/", json={"value": True}).status_code == 200
    assert client.put(f"{FILTERS}Spatial Filter/enabled/", json={"value": False}).status_code == 200
    assert rs_manager.settings.get().post_processing.filter_state == {
        "device1/0": {"Hole Filling Filter": True, "Spatial Filter": False}
    }

    rs_manager.processing_blocks.clear()  # as after a restart or a re-plug
    filters = client.get(FILTERS).json()
    assert filters["Hole Filling Filter"]["enabled"] is True
    assert filters["Spatial Filter"]["enabled"] is False
    assert filters["Temporal Filter"]["enabled"] is True  # untouched: still the default


def test_d405_threshold_starts_at_short_range(setup_mock_managers):
    rs_manager = setup_mock_managers["rs_manager"]
    from ..mocks.pyrealsense_mock import camera_info, option
    dev = rs_manager.devices["device1"]
    dev._info[camera_info.product_id] = "0B5B"
    filters = client.get(FILTERS).json()
    by_id = {o["option_id"]: o for o in filters["Threshold Filter"]["options"]}
    assert (by_id["min_distance"]["current_value"], by_id["max_distance"]["current_value"]) == (0.05, 4.0)
    assert dev.sensors[0].get_recommended_filters  # sanity: still the mock
    del dev._info[camera_info.product_id]
    assert option.min_distance  # the mock exposes the option enum


def test_option_changes_pushed_by_the_sdk_are_forwarded(setup_mock_managers):
    rs_manager = setup_mock_managers["rs_manager"]
    emitted = []
    rs_manager._emit_socket_event = lambda ev, payload: emitted.append((ev, payload))
    dev = rs_manager.devices["device1"]
    rs_manager._watch_option_changes("device1", dev)

    class _Changed:
        def __init__(self, opt, value):
            self.id, self.value = opt, value

    from ..mocks.pyrealsense_mock import option
    dev.sensors[0]._options_changed_callback([_Changed(option.laser_power, 150.0)])

    assert emitted == [("options_changed", {
        "device_id": "device1", "sensor_id": "device1-sensor-0",
        "options": [{"option_id": "laser_power", "current_value": 150.0}],
    })]
