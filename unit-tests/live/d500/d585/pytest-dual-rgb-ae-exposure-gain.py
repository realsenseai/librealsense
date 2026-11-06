# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# A dual-color (2C) device has no color sensor: the RGB auto-exposure, exposure and gain are options of the depth
# sensor under their own DUAL_RGB IDs, independent of the depth ones that keep the standard IDs.
#
# The RGB exposure is in 100 usec units, like the exposure of other color sensors.

import pytest
import pyrealsense2 as rs
import math
import platform
import time
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each( "Dual RGB" ),
]

RGB_AE = rs.option.dual_rgb_enable_auto_exposure
RGB_EXPOSURE = rs.option.dual_rgb_exposure
RGB_GAIN = rs.option.dual_rgb_gain
DEPTH_AE = rs.option.enable_auto_exposure
DEPTH_EXPOSURE = rs.option.exposure
DEPTH_GAIN = rs.option.gain
ALL_OPTIONS = [RGB_AE, RGB_EXPOSURE, RGB_GAIN, DEPTH_AE, DEPTH_EXPOSURE, DEPTH_GAIN]


def accepted_exposure( value ):
    # The Windows camera control takes the exposure on a log2 scale, so only powers of two (in seconds) are accepted
    if platform.system() != "Windows":
        return value
    log2_seconds = math.log2( value * 1e-4 )
    rounded = math.copysign( math.floor( abs( log2_seconds ) + 0.5 ), log2_seconds )
    return int( 2 ** rounded * 1e4 )


@pytest.fixture
def depth_sensor(test_device):
    # Restore every option the test touched, whatever the outcome
    dev, _ = test_device
    sensor = dev.first_depth_sensor()
    original = {o: sensor.get_option( o ) for o in ALL_OPTIONS if sensor.supports( o )}
    yield sensor
    for option in (DEPTH_AE, RGB_AE):
        if option in original:
            sensor.set_option( option, 0 )  # a manual value switches AE off anyway, so restore AE last
    for option, value in original.items():
        if option in (DEPTH_AE, RGB_AE):
            continue
        sensor.set_option( option, value )
    for option in (DEPTH_AE, RGB_AE):
        if option in original:
            sensor.set_option( option, original[option] )


def test_options_are_supported(depth_sensor):
    for option in ALL_OPTIONS:
        assert depth_sensor.supports( option ), f"{option} is not supported"


def test_option_ranges(depth_sensor):
    ae = depth_sensor.get_option_range( RGB_AE )
    assert (ae.min, ae.max, ae.step) == (0, 1, 1)

    exposure = depth_sensor.get_option_range( RGB_EXPOSURE )
    assert exposure.step == 1
    assert exposure.min <= exposure.default <= exposure.max

    # RGB exposure is in 100 usec units, the depth exposure in usec
    assert exposure.max < depth_sensor.get_option_range( DEPTH_EXPOSURE ).max

    gain = depth_sensor.get_option_range( RGB_GAIN )
    assert gain.min <= gain.default <= gain.max


def test_rgb_values_set_get(depth_sensor):
    depth_sensor.set_option( RGB_AE, 0 )
    r = depth_sensor.get_option_range( RGB_EXPOSURE )
    for value in (r.min, 100, r.default):
        depth_sensor.set_option( RGB_EXPOSURE, value )
        assert depth_sensor.get_option( RGB_EXPOSURE ) == accepted_exposure( value )

    g = depth_sensor.get_option_range( RGB_GAIN )
    for value in (g.min, 64, g.max):
        depth_sensor.set_option( RGB_GAIN, value )
        assert depth_sensor.get_option( RGB_GAIN ) == value


def test_rgb_values_out_of_range_rejected(depth_sensor):
    depth_sensor.set_option( RGB_AE, 0 )
    exposure = depth_sensor.get_option_range( RGB_EXPOSURE )
    gain = depth_sensor.get_option_range( RGB_GAIN )
    with pytest.raises( Exception ):
        depth_sensor.set_option( RGB_EXPOSURE, exposure.max + 1 )
    with pytest.raises( Exception ):
        depth_sensor.set_option( RGB_GAIN, gain.max + 1 )


def test_manual_rgb_exposure_disables_rgb_ae_only(depth_sensor):
    depth_sensor.set_option( DEPTH_AE, 1 )
    depth_sensor.set_option( RGB_AE, 1 )

    depth_sensor.set_option( RGB_EXPOSURE, 50 )

    assert depth_sensor.get_option( RGB_AE ) == 0
    assert depth_sensor.get_option( DEPTH_AE ) == 1


def test_manual_rgb_gain_disables_rgb_ae_only(depth_sensor):
    depth_sensor.set_option( DEPTH_AE, 1 )
    depth_sensor.set_option( RGB_AE, 1 )

    depth_sensor.set_option( RGB_GAIN, 64 )

    assert depth_sensor.get_option( RGB_AE ) == 0
    assert depth_sensor.get_option( DEPTH_AE ) == 1


def test_manual_depth_exposure_disables_depth_ae_only(depth_sensor):
    depth_sensor.set_option( DEPTH_AE, 1 )
    depth_sensor.set_option( RGB_AE, 1 )

    depth_sensor.set_option( DEPTH_EXPOSURE, 5000 )

    assert depth_sensor.get_option( DEPTH_AE ) == 0
    assert depth_sensor.get_option( RGB_AE ) == 1


def test_rgb_and_depth_values_are_independent(depth_sensor):
    depth_sensor.set_option( DEPTH_AE, 0 )
    depth_sensor.set_option( RGB_AE, 0 )

    depth_sensor.set_option( DEPTH_EXPOSURE, 4000 )
    depth_sensor.set_option( DEPTH_GAIN, 32 )
    depth_sensor.set_option( RGB_EXPOSURE, 30 )
    depth_sensor.set_option( RGB_GAIN, 80 )

    assert depth_sensor.get_option( DEPTH_EXPOSURE ) == 4000
    assert depth_sensor.get_option( DEPTH_GAIN ) == 32
    assert depth_sensor.get_option( RGB_EXPOSURE ) == accepted_exposure( 30 )
    assert depth_sensor.get_option( RGB_GAIN ) == 80


def test_rgb_options_work_while_streaming(test_device, depth_sensor):
    # Hybrid streaming: depth and both color streams at once
    _, ctx = test_device
    cfg = rs.config()
    cfg.enable_stream( rs.stream.depth, 640, 480, rs.format.z16, 30 )
    cfg.enable_stream( rs.stream.color, 1, 640, 480, rs.format.rgb8, 30 )
    cfg.enable_stream( rs.stream.color, 2, 640, 480, rs.format.rgb8, 30 )
    pipe = rs.pipeline( ctx )
    pipe.start( cfg )
    try:
        for _ in range( 10 ):
            pipe.wait_for_frames()

        depth_sensor.set_option( DEPTH_AE, 1 )
        depth_sensor.set_option( RGB_AE, 1 )
        depth_sensor.set_option( RGB_EXPOSURE, 30 )
        depth_sensor.set_option( RGB_GAIN, 48 )

        assert depth_sensor.get_option( RGB_AE ) == 0
        assert depth_sensor.get_option( RGB_EXPOSURE ) == accepted_exposure( 30 )
        assert depth_sensor.get_option( RGB_GAIN ) == 48
        assert depth_sensor.get_option( DEPTH_AE ) == 1
        pipe.wait_for_frames()
    finally:
        pipe.stop()


def color_frames_with_metadata( ctx, apply_options, is_settled ):
    """
    Stream depth and both color streams, apply the options, and return the latest frame of each color stream once
    is_settled( frame ) holds for both: the metadata follows the option by a few frames.
    """
    cfg = rs.config()
    cfg.enable_stream( rs.stream.depth, 640, 480, rs.format.z16, 30 )
    cfg.enable_stream( rs.stream.color, 1, 640, 480, rs.format.rgb8, 30 )
    cfg.enable_stream( rs.stream.color, 2, 640, 480, rs.format.rgb8, 30 )
    pipe = rs.pipeline( ctx )
    pipe.start( cfg )
    try:
        apply_options()
        colors = {}
        deadline = time.time() + 5
        while time.time() < deadline:
            for f in pipe.wait_for_frames():
                if f.get_profile().stream_type() == rs.stream.color:
                    colors[f.get_profile().stream_index()] = f
            if len( colors ) == 2 and all( is_settled( f ) for f in colors.values() ):
                break
        return colors
    finally:
        pipe.stop()


def metadata( frame, key ):
    if not frame.supports_frame_metadata( key ):
        pytest.skip( "per-frame metadata is not enabled at the OS level" )
    return frame.get_frame_metadata( key )


# Skipped until the firmware fix: the actual exposure and gain it reports do not follow the options
@pytest.mark.skip( reason="firmware actual exposure/gain metadata differs from the setpoint" )
def test_color_frame_metadata_reports_rgb_exposure_and_gain(test_device, depth_sensor):
    _, ctx = test_device

    def apply_options():
        depth_sensor.set_option( RGB_AE, 0 )
        depth_sensor.set_option( RGB_EXPOSURE, 30 )
        depth_sensor.set_option( RGB_GAIN, 48 )

    exposure = accepted_exposure( 30 )
    # Actual exposure is in 100 usec units, like the option; the firmware may report one unit below the setpoint
    def is_settled( f ):
        return f.supports_frame_metadata( rs.frame_metadata_value.actual_exposure )                and abs( f.get_frame_metadata( rs.frame_metadata_value.actual_exposure ) - exposure ) <= 1                and f.get_frame_metadata( rs.frame_metadata_value.gain_level ) == 48

    for index, f in color_frames_with_metadata( ctx, apply_options, is_settled ).items():
        assert abs( metadata( f, rs.frame_metadata_value.actual_exposure ) - exposure ) <= 1, f"color {index}"
        assert metadata( f, rs.frame_metadata_value.gain_level ) == 48, f"color {index}"


@pytest.mark.xfail( reason="the firmware reports a constant auto-exposure state in the color metadata", strict=False )
def test_color_frame_metadata_reports_rgb_ae_state(test_device, depth_sensor):
    _, ctx = test_device
    for ae in (1, 0):
        def is_settled( f ):
            return f.supports_frame_metadata( rs.frame_metadata_value.auto_exposure )                    and f.get_frame_metadata( rs.frame_metadata_value.auto_exposure ) == ae

        colors = color_frames_with_metadata( ctx, lambda: depth_sensor.set_option( RGB_AE, ae ), is_settled )
        for index, f in colors.items():
            assert metadata( f, rs.frame_metadata_value.auto_exposure ) == ae, f"color {index}, AE {ae}"
