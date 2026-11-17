# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# A dual-color (2C) device reports the AE policy its FW actually applied in the per-frame metadata, on USB and GMSL.
# Depth/IR alone run depth priority, color alone runs color priority, and together they follow the
# RS2_OPTION_DEPTH_AUTO_EXPOSURE_MODE policy (Auto resolves to hybrid).

import pytest
import pyrealsense2 as rs
import pyrsutils as rsutils
import time
import logging
from rspy.pytest.device_helpers import require_min_fw_version
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each( "Dual RGB" ),
]

AE_POLICY = rs.frame_metadata_value.ae_policy
AE_MODE = rs.option.depth_auto_exposure_mode
MIN_FW = rsutils.version( "7.58.45946.14332" )  # TODO: FW release that adds the AE policy metadata
FPS = 30
FRAMES_TO_CHECK = 15
SETTLE_FRAMES = 5
TIMEOUT_S = 10

DEPTH_PRIORITY = int( rs.colored_ir_auto_exposure_mode.depth_priority )
COLOR_PRIORITY = int( rs.colored_ir_auto_exposure_mode.color_priority )
HYBRID = int( rs.colored_ir_auto_exposure_mode.hybrid )
AUTO = int( rs.colored_ir_auto_exposure_mode.auto )


@pytest.fixture
def depth_sensor(test_device):
    # Depth, IR and both color streams all live on the depth sensor of a dual-color device
    dev, _ = test_device
    require_min_fw_version( dev, MIN_FW, "AE policy frame metadata" )
    sensor = dev.first_depth_sensor()
    original = sensor.get_option( AE_MODE )
    yield sensor
    sensor.set_option( AE_MODE, original )
    try:
        sensor.close()   # the sensor outlives the test - never hand it to the next one still open
    except RuntimeError:
        pass             # not open, which is the normal path


def profile(sensor, stream_type, index=0):
    # Color streams of a dual-color device must share a resolution, here the first Color 1 profile at FPS is enough
    for p in sensor.get_stream_profiles():
        if p.stream_type() == stream_type and p.stream_index() == index and p.fps() == FPS \
                and p.format() not in (rs.format.raw16, rs.format.yuyv):
            return p
    pytest.skip( f"Device publishes no {stream_type} {index} profile at {FPS} FPS" )


def collect_policies(sensor, profiles):
    """Stream the profiles; return {stream name: set of AE policy values}, None standing for a frame without the metadata."""
    policies = {}
    counts = {}

    def on_frame(f):
        # The first frames may still carry the previous policy
        name = f.get_profile().stream_name()
        counts[name] = counts.get( name, 0 ) + 1
        if counts[name] > SETTLE_FRAMES:
            value = f.get_frame_metadata( AE_POLICY ) if f.supports_frame_metadata( AE_POLICY ) else None
            policies.setdefault( name, set() ).add( value )

    sensor.open( profiles )
    sensor.start( on_frame )
    try:
        deadline = time.time() + TIMEOUT_S
        while time.time() < deadline and any( counts.get( p.stream_name(), 0 ) < SETTLE_FRAMES + FRAMES_TO_CHECK for p in profiles ):
            time.sleep( 0.1 )
    finally:
        sensor.stop()
        sensor.close()
    # Asserting here, not in the callback, where an exception is only logged
    assert set( policies ) == {p.stream_name() for p in profiles}, f"streams with frames: {sorted( policies )}"
    return policies


def test_depth_and_ir_only_report_depth_priority(depth_sensor):
    profiles = [profile( depth_sensor, rs.stream.depth ), profile( depth_sensor, rs.stream.infrared, 1 )]
    policies = collect_policies( depth_sensor, profiles )
    assert all( v == {DEPTH_PRIORITY} for v in policies.values() ), policies


def test_color_only_reports_color_priority(depth_sensor):
    policies = collect_policies( depth_sensor, [profile( depth_sensor, rs.stream.color, 1 )] )
    assert all( v == {COLOR_PRIORITY} for v in policies.values() ), policies


@pytest.mark.parametrize( "requested, expected", [
    pytest.param( AUTO, HYBRID, id="auto" ),
    pytest.param( DEPTH_PRIORITY, DEPTH_PRIORITY, id="depth-priority" ),
    pytest.param( COLOR_PRIORITY, COLOR_PRIORITY, id="color-priority" ),
    pytest.param( HYBRID, HYBRID, id="hybrid" ),
] )
def test_depth_and_color_report_the_selected_policy(depth_sensor, requested, expected):
    depth_sensor.set_option( AE_MODE, requested )
    profiles = [profile( depth_sensor, rs.stream.depth ), profile( depth_sensor, rs.stream.color, 1 )]
    policies = collect_policies( depth_sensor, profiles )
    assert all( v == {expected} for v in policies.values() ), policies
