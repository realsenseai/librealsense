# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# D401 GMSL ISP color next to the raw dual-RGB feature: a lone Color 0 must stay on the ISP path, and
# the default configuration must not pull in Color 1. Kept apart from the raw-mode tests: switching
# raw <-> ISP needs a hardware reset, which the test infra does only between files. Nightly, like the raw
# tests: the regressions these guard need the raw path, which only the nightly FW/driver has.

import pytest
import pyrealsense2 as rs
import d401_helper as helper
from d401_helper import COLOR0, COLOR1, DEPTH, IR1, IR2

pytestmark = [
    pytest.mark.device_each( "D401" ),
    pytest.mark.device_type( "GMSL" ),
    pytest.mark.context( "nightly" ),
]

W, H = 848, 480


@pytest.fixture
def depth_sensor( test_device ):
    dev, _ = test_device
    yield dev.first_depth_sensor()


def test_default_config_resolves( test_device ):
    """enable_all_streams() (what rs-capture uses) resolves and streams without Color 1. This used to
    fail: both colors were default-tagged, and Color 1 shares its hardware endpoint with IR 2."""
    dev, ctx = test_device
    pipe = rs.pipeline( ctx )
    cfg = rs.config()
    cfg.enable_device( dev.get_info( rs.camera_info.serial_number ) )
    cfg.enable_all_streams()
    streams = { (s.stream_type(), s.stream_index()) for s in cfg.resolve( rs.pipeline_wrapper( pipe ) ).get_streams() }
    assert COLOR1 not in streams, f"default configuration includes Color 1: {sorted( streams )}"
    assert DEPTH in streams, f"default configuration has no depth: {sorted( streams )}"
    pipe.start( cfg )
    try:
        for _ in range( 30 ):
            pipe.wait_for_frames()
    finally:
        pipe.stop()


def test_isp_color_with_depth_and_infrared( depth_sensor ):
    """A lone Color 0 RGB8 stays on the ISP path, so it streams alongside depth and both IRs (raw
    dual-RGB would take the IR imager)."""
    profiles = [helper.need_profile( depth_sensor, DEPTH, rs.format.z16, W, H ),
                helper.need_profile( depth_sensor, IR1, rs.format.y8, W, H ),
                helper.need_profile( depth_sensor, IR2, rs.format.y8, W, H ),
                helper.need_profile( depth_sensor, COLOR0, rs.format.rgb8, W, H )]
    numbers, sizes = helper.stream( depth_sensor, profiles )
    for key in (DEPTH, IR1, IR2, COLOR0):
        helper.check_rate( numbers, key )
    assert sizes[COLOR0] == W * H * 3, f"Color 0 frame is {sizes[COLOR0]} bytes, expected {W * H * 3}"
