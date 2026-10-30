# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Improved Close Range Depth over DDS. Older FW publishes Enable, Downscale Ratio, Disparity Shift and
# Threshold, exposed as scalar options; newer FW publishes the 7 hdrd_control fields, exposed as the
# hdrd_control composite option same as over USB (field round trips in live/options/pytest-hdrd-control.py).

import time

import pytest
import pyrealsense2 as rs
import pyrsutils as rsutils
from pytest_check import check
import logging
log = logging.getLogger(__name__)


pytestmark = [
    pytest.mark.device_each( "D555" ),
    pytest.mark.device_type( "DDS" ),
    pytest.mark.context( "nightly" ),
]


# DPP Filter Bitmask bit 5 = "Improved Close Range Depth merge applied"
CLOSE_RANGE_METADATA_BIT = 1 << 5

# Spec defaults for the option-specific sections.
RATIO_INDEX_DEFAULT = 1.0    # choices ["1","2","4"], default "2" -> index 1
SHIFT_DEFAULT      = 0.0

# Minimum FW that registers the close-range feature, mirroring src/ds/d500/d500-factory.cpp.
# Below the gate the feature is not registered and get_embedded_filter() throws rather than
# returning a falsy value, so we skip up front.
MIN_FW_BY_DEVICE = {
    "D555": "7.58.39807.10573",
}

# Pick a depth profile that the close-range merge actually exercises on D555.
DEPTH_W, DEPTH_H, DEPTH_FPS = 640, 360, 30


def _skip_if_fw_below_minimum( dev ):
    name = dev.get_info( rs.camera_info.name ) if dev.supports( rs.camera_info.name ) else ""
    min_fw = next( ( v for k, v in MIN_FW_BY_DEVICE.items() if k in name ), None )
    if min_fw is None:
        return
    fw = dev.get_info( rs.camera_info.firmware_version )
    if rsutils.version( fw ) < rsutils.version( min_fw ):
        pytest.skip( f"FW {fw} below minimum {min_fw} for Improved Close Range Depth on {name}" )


HDRD = rs.composite_option_id.hdrd_control


def _get_close_range_filter( test_device, composite=None ):
    """composite: True/False skips unless the FW exposes the hdrd_control / scalar form; None accepts both"""
    dev, _ = test_device
    _skip_if_fw_below_minimum( dev )
    depth_sensor = dev.first_depth_sensor()
    f = depth_sensor.get_embedded_filter( rs.embedded_filter_type.improved_close_range_depth )
    if not f:
        pytest.skip( "Improved Close Range Depth filter not exposed (FW likely below the supported version)" )
    if composite is not None and _is_composite( f ) != composite:
        pytest.skip( f"FW exposes the {'composite' if _is_composite( f ) else 'scalar'} form" )
    return depth_sensor, f


def _is_composite( embedded_filter ):
    return HDRD in embedded_filter.get_supported_composite_options()


def _set_fields( embedded_filter, **fields ):
    cfg = embedded_filter.get_hdrd_control( HDRD )
    for name, value in fields.items():
        setattr( cfg, name, value )
    embedded_filter.set_hdrd_control( HDRD, cfg )


def _set_enable( embedded_filter, value ):
    if _is_composite( embedded_filter ):
        _set_fields( embedded_filter, enable=value )
    else:
        embedded_filter.set_option( rs.option.embedded_filter_enabled, value )


def _set_get_round_trip( embedded_filter, option, value ):
    initial = embedded_filter.get_option( option )
    step = embedded_filter.get_option_range( option ).step
    embedded_filter.set_option( option, value )
    check.almost_equal( embedded_filter.get_option( option ), value, abs=step )
    embedded_filter.set_option( option, initial )
    check.almost_equal( embedded_filter.get_option( option ), initial, abs=step )


def test_close_range_filter_present( test_device ):
    _, embedded_filter = _get_close_range_filter( test_device, composite=False )
    options = embedded_filter.get_supported_options()

    # Enable is required on every transport.
    assert rs.option.embedded_filter_enabled in options

    log.info( f"close-range exposes {len(options)} option(s): {[str(o) for o in options]}" )


def test_close_range_enable_round_trip( test_device ):
    _, embedded_filter = _get_close_range_filter( test_device, composite=False )
    _set_get_round_trip( embedded_filter, rs.option.embedded_filter_enabled, 1.0 )


def test_close_range_enable_invalid_value_rejected( test_device ):
    _, embedded_filter = _get_close_range_filter( test_device, composite=False )
    with pytest.raises( RuntimeError ):
        embedded_filter.set_option( rs.option.embedded_filter_enabled, 2.0 )
    with pytest.raises( RuntimeError ):
        embedded_filter.set_option( rs.option.embedded_filter_enabled, -1.0 )


def test_close_range_downscale_ratio( test_device ):
    """Not every FW exposes the full 4-option surface."""
    _, embedded_filter = _get_close_range_filter( test_device, composite=False )
    options = embedded_filter.get_supported_options()
    if rs.option.downscale_ratio not in options:
        pytest.skip( "downscale_ratio option not exposed by this FW" )

    # Default index = 1 (choice "2")
    check.equal( embedded_filter.get_option( rs.option.downscale_ratio ), RATIO_INDEX_DEFAULT )

    # Disable first - permuting the ratio while enabled may trip the device-side mutex.
    embedded_filter.set_option( rs.option.embedded_filter_enabled, 0.0 )
    _set_get_round_trip( embedded_filter, rs.option.downscale_ratio, 2.0 )

    # Index range is [0..2] for choices "1"/"2"/"4"; 3.0 is out of range.
    with pytest.raises( RuntimeError ):
        embedded_filter.set_option( rs.option.downscale_ratio, 3.0 )


def test_close_range_disparity_shift( test_device ):
    _, embedded_filter = _get_close_range_filter( test_device, composite=False )
    options = embedded_filter.get_supported_options()
    if rs.option.disparity_shift not in options:
        pytest.skip( "disparity_shift option not exposed by this FW" )

    check.equal( embedded_filter.get_option( rs.option.disparity_shift ), SHIFT_DEFAULT )

    embedded_filter.set_option( rs.option.embedded_filter_enabled, 0.0 )
    # Spec: shift > 0 forces ratio = 1; test shift before ratio to avoid the order dependency.
    _set_get_round_trip( embedded_filter, rs.option.disparity_shift, 100.0 )


def test_hdrd_control_present( test_device ):
    _, embedded_filter = _get_close_range_filter( test_device, composite=True )
    cfg = embedded_filter.get_hdrd_control( HDRD )
    assert cfg.header.version == 1
    assert cfg.header.param_count == 7


def test_hdrd_control_enable_round_trip( test_device ):
    _, embedded_filter = _get_close_range_filter( test_device, composite=True )
    original = embedded_filter.get_hdrd_control( HDRD ).enable
    for value in ( 1, 0 ):
        _set_fields( embedded_filter, enable=value )
        check.equal( embedded_filter.get_hdrd_control( HDRD ).enable, value )
    _set_fields( embedded_filter, enable=original )


@pytest.mark.parametrize( "field, value", [( "enable", 2 ), ( "enable", -1 ), ( "downscale_ratio", 3 ), ( "shift_pixels", 257 )] )
def test_hdrd_control_invalid_value_rejected( test_device, field, value ):
    _, embedded_filter = _get_close_range_filter( test_device, composite=True )
    before = embedded_filter.get_hdrd_control( HDRD )
    with pytest.raises( RuntimeError ):
        _set_fields( embedded_filter, **{ field: value } )
    check.equal( getattr( embedded_filter.get_hdrd_control( HDRD ), field ), getattr( before, field ) )


def _find_depth_profile( depth_sensor ):
    return next(
        ( p for p in depth_sensor.profiles
          if p.fps() == DEPTH_FPS
             and p.stream_type() == rs.stream.depth
             and p.format() == rs.format.z16
             and p.as_video_stream_profile().width()  == DEPTH_W
             and p.as_video_stream_profile().height() == DEPTH_H ),
        None )


@pytest.mark.parametrize( "enable_value, expected_bit", [( 0, 0 ), ( 1, CLOSE_RANGE_METADATA_BIT )] )
def test_close_range_metadata_bit( test_device, enable_value, expected_bit ):
    # FW does not yet populate the embedded_filters metadata key for the close-range bit, so
    # frames arrive without it and the test times out. Restore once RSDEV-12008 is resolved.
    log.warning( "Skipping test_close_range_metadata_bit due to FW bug - restore after RSDEV-12008 is solved" )
    pytest.skip( "FW bug tracked by RSDEV-12008" )

    depth_sensor, embedded_filter = _get_close_range_filter( test_device )
    profile = _find_depth_profile( depth_sensor )
    if profile is None:
        pytest.skip( f"depth profile {DEPTH_W}x{DEPTH_H}@{DEPTH_FPS} z16 not advertised; cannot exercise metadata path" )

    _set_enable( embedded_filter, enable_value )

    state = { "checked": False }

    def cb( frame ):
        # Only check frames that carry the metadata key - early frames may not yet.
        if frame.supports_frame_metadata( rs.frame_metadata_value.embedded_filters ):
            md = frame.get_frame_metadata( rs.frame_metadata_value.embedded_filters )
            check.equal( md & CLOSE_RANGE_METADATA_BIT, expected_bit )
            state["checked"] = True

    depth_sensor.open( profile )
    try:
        depth_sensor.start( cb )
        try:
            deadline = time.time() + 10.0
            while not state["checked"] and time.time() < deadline:
                time.sleep( 0.2 )
        finally:
            depth_sensor.stop()
    finally:
        depth_sensor.close()

    assert state["checked"], "no frame carried the embedded_filters metadata key within 10s"
