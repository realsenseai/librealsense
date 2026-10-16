# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# D401 GMSL raw dual-RGB across every output resolution. Each one is a different demosaic crop/scale of
# the native 1288x808 image, so each gets its own Color 0 + Color 1 run. Nightly: too slow to gate PRs.

import pytest
import pyrealsense2 as rs
import d401_helper as helper
from d401_helper import COLOR0, COLOR1

pytestmark = [
    pytest.mark.device_each( "D401" ),
    pytest.mark.device_type( "GMSL" ),
    pytest.mark.context( "nightly" ),
]

RESOLUTIONS = [(1280, 720), (848, 480), (640, 480), (640, 360), (480, 270), (424, 240)]


@pytest.fixture( scope="module", autouse=True )
def require_dual_rgb( test_device ):
    dev, _ = test_device
    helper.skip_without_color1( dev )


@pytest.fixture
def depth_sensor( test_device ):
    dev, _ = test_device
    yield dev.first_depth_sensor()


@pytest.mark.parametrize( "w,h", RESOLUTIONS )
def test_both_colors_stream_at( depth_sensor, w, h ):
    profiles = [helper.need_profile( depth_sensor, COLOR0, rs.format.rgb8, w, h ),
                helper.need_profile( depth_sensor, COLOR1, rs.format.rgb8, w, h )]
    numbers, sizes = helper.stream( depth_sensor, profiles )
    for key in (COLOR0, COLOR1):
        helper.check_rate( numbers, key )
        assert sizes[key] == w * h * 3, f"Color {key[1]} frame is {sizes[key]} bytes, expected {w * h * 3}"
