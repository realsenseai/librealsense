# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import time
import pytest
import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy.pytest.device_helpers import require_min_fw_version
from rspy.timer import Timer
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D555", "D585"),
    pytest.mark.device_exclude("D585S"),
]

OFF, DOWNSCALE_HDR, HIGH_RES = 0, 1, 2
WRITABLE_FIELDS = ('mode', 'downscale_ratio', 'high_res_mode', 'high_res_offset', 'smoothing_offset_mm',
                   'completion_offset_mm')
READ_ONLY_FIELDS = ('min_z_mm', 'improved_min_z_mm')
ALL_FIELDS = WRITABLE_FIELDS + READ_ONLY_FIELDS  # wire order, right after the 6-byte dpp_header


def _find_icr_filter(sensor):
    """Return the embedded filter exposing Improved Close Range Control on this sensor, or None - the
    composite option lives on one of the sensor's embedded filters, its own independent options
    registry, not on the sensor itself (see rs2::embedded_filter)."""
    for embedded_filter in sensor.query_embedded_filters():
        if rs.composite_option_id.icr_control in embedded_filter.get_supported_composite_options():
            return embedded_filter
    return None


def _field_from_raw(raw, field):
    offset = 6 + 4 * ALL_FIELDS.index(field)
    return int.from_bytes(bytes(raw)[offset:offset + 4], 'little', signed=True)


def _set_fields(embedded_filter, option_id, **fields):
    """Read-modify-write: the header and every other field are carried over as the device just reported them."""
    cfg = embedded_filter.get_icr_control(option_id)
    for name, value in fields.items():
        setattr(cfg, name, value)
    embedded_filter.set_icr_control(option_id, cfg)
    return embedded_filter.get_icr_control(option_id)


@pytest.fixture
def icr(test_device):
    """Yields (embedded filter, option id, range, depth sensor); restores the writable fields on exit."""
    dev, ctx = test_device
    require_min_fw_version(dev, rsutils.version(7, 62, 0, 0), "Improved Close Range 8-parameter control")
    depth_sensor = dev.first_depth_sensor()
    embedded_filter = _find_icr_filter(depth_sensor)
    if embedded_filter is None:
        pytest.skip("Improved Close Range Control composite option not supported on this device")

    option_id = rs.composite_option_id.icr_control
    try:
        original_raw = embedded_filter.get_composite_option(option_id)
    except RuntimeError as e:
        # Registered but not actually functional on this device/FW is a real, expected outcome -
        # get_supported_composite_options() only reflects static registration, never a live
        # capability check.
        pytest.skip(f"Improved Close Range Control registered but not functional on this device/FW: {e}")

    yield embedded_filter, option_id, embedded_filter.get_icr_control_range(option_id), depth_sensor

    # MinZ / Improved MinZ are FW-computed telemetry - only the writable fields are restored and compared
    embedded_filter.set_composite_option(option_id, original_raw)
    restored = embedded_filter.get_icr_control(option_id)
    for field in WRITABLE_FIELDS:
        assert getattr(restored, field) == _field_from_raw(original_raw, field), f"failed to restore {field}"


def test_icr_control_header(icr):
    embedded_filter, option_id, _, _ = icr
    header = embedded_filter.get_icr_control(option_id).header
    assert header.param_count == 8


def test_icr_mode_is_a_single_tri_state(icr):
    embedded_filter, option_id, range, _ = icr
    assert (range.min.mode, range.max.mode) == (OFF, HIGH_RES)
    for mode in (DOWNSCALE_HDR, HIGH_RES, OFF):
        assert _set_fields(embedded_filter, option_id, mode=mode).mode == mode


@pytest.mark.parametrize("field,values", [
    ('downscale_ratio', (2, 1)),
    ('high_res_mode', (1, 2, 0)),
    ('high_res_offset', (0, 64, 256)),
    ('smoothing_offset_mm', (0, 300, 10000)),
    ('completion_offset_mm', (0, 200, 10000)),
])
def test_icr_writable_field_round_trip(icr, field, values):
    embedded_filter, option_id, range, _ = icr
    for value in values:
        assert getattr(range.min, field) <= value <= getattr(range.max, field)
        assert getattr(_set_fields(embedded_filter, option_id, **{field: value}), field) == value


@pytest.mark.parametrize("field", ALL_FIELDS)
def test_icr_out_of_range_rejected_locally(icr, field):
    """Every param, read-only MinZ telemetry included, is range-checked before anything is sent."""
    embedded_filter, option_id, range, _ = icr
    before = embedded_filter.get_composite_option(option_id)
    for bad_value in (getattr(range.min, field) - 1, getattr(range.max, field) + 1):
        cfg = embedded_filter.get_icr_control(option_id)
        setattr(cfg, field, bad_value)
        with pytest.raises(RuntimeError):
            embedded_filter.set_icr_control(option_id, cfg)
    assert embedded_filter.get_composite_option(option_id) == before


@pytest.mark.parametrize("field", READ_ONLY_FIELDS)
def test_icr_read_only_telemetry_is_never_written(icr, field):
    """An in-range SET of MinZ / Improved MinZ is accepted but the device keeps its own computed value."""
    embedded_filter, option_id, _, _ = icr
    before = getattr(embedded_filter.get_icr_control(option_id), field)
    attempted = 100 if before != 100 else 200
    assert getattr(_set_fields(embedded_filter, option_id, **{field: attempted}), field) == before


@pytest.fixture
def streaming_1280x720_30(icr):
    """Streams Depth 1280x720@30 and stops it on exit, restoring nothing else - `icr` does that"""
    _, _, _, depth_sensor = icr

    def is_1280x720_30(profile):
        video = profile.as_video_stream_profile()
        return (profile.stream_type() == rs.stream.depth and profile.format() == rs.format.z16
                and (video.width(), video.height()) == (1280, 720) and profile.fps() == 30)

    profile = next((p for p in depth_sensor.get_stream_profiles() if is_1280x720_30(p)), None)
    if profile is None:
        pytest.skip("No 1280x720@30 Z16 depth profile on this device")
    return depth_sensor, profile


def _stream(depth_sensor, profile):
    """Starts and waits for the first frame: FW builds its pipe by then, and a SET sent before it is not yet locked"""
    frames = []
    depth_sensor.open(profile)
    depth_sensor.start(lambda frame: frames.append(frame.get_frame_number()))
    timer = Timer(5)
    timer.start()
    while not frames and not timer.has_expired():
        time.sleep(0.05)
    assert frames, "no frames after start"


def _stop(depth_sensor):
    depth_sensor.stop()
    depth_sensor.close()


def test_icr_downscale_fields_settable_while_streaming(icr, streaming_1280x720_30):
    """Only topology-preserving changes are live: Smoothing / Completion and Off to/from the prebuilt mode"""
    embedded_filter, option_id, _, _ = icr
    depth_sensor, profile = streaming_1280x720_30
    _set_fields(embedded_filter, option_id, mode=DOWNSCALE_HDR, downscale_ratio=1)
    _stream(depth_sensor, profile)
    try:
        assert _set_fields(embedded_filter, option_id, smoothing_offset_mm=300).smoothing_offset_mm == 300
        assert _set_fields(embedded_filter, option_id, completion_offset_mm=200).completion_offset_mm == 200
        assert _set_fields(embedded_filter, option_id, mode=OFF).mode == OFF
        assert _set_fields(embedded_filter, option_id, mode=DOWNSCALE_HDR).mode == DOWNSCALE_HDR
    finally:
        _stop(depth_sensor)


def test_icr_high_res_fields_settable_while_streaming(icr, streaming_1280x720_30):
    embedded_filter, option_id, _, _ = icr
    depth_sensor, profile = streaming_1280x720_30
    _set_fields(embedded_filter, option_id, mode=HIGH_RES, high_res_mode=0)
    _stream(depth_sensor, profile)
    try:
        assert _set_fields(embedded_filter, option_id, high_res_mode=2, high_res_offset=100).high_res_offset == 100
        assert _set_fields(embedded_filter, option_id, high_res_mode=1).high_res_mode == 1
        assert _set_fields(embedded_filter, option_id, mode=OFF).mode == OFF
        assert _set_fields(embedded_filter, option_id, mode=HIGH_RES).mode == HIGH_RES
    finally:
        _stop(depth_sensor)


def test_icr_topology_change_rejected_while_streaming(icr, streaming_1280x720_30):
    """Design 3.6: switching Downscale <-> High-Res or x2 <-> x4 live is rejected as busy, and nothing changes"""
    embedded_filter, option_id, _, _ = icr
    depth_sensor, profile = streaming_1280x720_30
    _set_fields(embedded_filter, option_id, mode=DOWNSCALE_HDR, downscale_ratio=1)
    _stream(depth_sensor, profile)
    try:
        before = bytes(embedded_filter.get_composite_option(option_id))[6:6 + 24]  # the six writable fields
        with pytest.raises(RuntimeError):
            _set_fields(embedded_filter, option_id, downscale_ratio=2)
        with pytest.raises(RuntimeError):
            _set_fields(embedded_filter, option_id, mode=HIGH_RES)
        assert bytes(embedded_filter.get_composite_option(option_id))[6:6 + 24] == before
    finally:
        _stop(depth_sensor)


def test_icr_manual_offset_126_matches_126px_preset(icr, streaming_1280x720_30):
    """The 126 px preset and Manual at 126 are the same shift, so Improved MinZ must not change"""
    embedded_filter, option_id, _, _ = icr
    depth_sensor, profile = streaming_1280x720_30
    _set_fields(embedded_filter, option_id, mode=HIGH_RES, high_res_mode=0)
    _stream(depth_sensor, profile)
    try:
        time.sleep(1)  # FW recalculates the telemetry on the change
        preset = embedded_filter.get_icr_control(option_id).improved_min_z_mm
        _set_fields(embedded_filter, option_id, high_res_mode=2, high_res_offset=126)
        time.sleep(1)
        assert embedded_filter.get_icr_control(option_id).improved_min_z_mm == preset
    finally:
        _stop(depth_sensor)
