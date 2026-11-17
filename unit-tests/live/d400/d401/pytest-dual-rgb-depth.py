# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Raw dual-RGB together with depth, over GMSL (D401) and USB (D401/D405). 60 fps over USB only: the GMSL
# link can't yet carry depth plus both raw colors at 60, so the colors drop frames there.

import pytest
import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy.pytest.device_helpers import require_min_fw_version
import d401_helper as helper
from d401_helper import COLOR0, COLOR1, DEPTH

pytestmark = [pytest.mark.device_each( "D401", "D405" )]


@pytest.fixture( scope="module", autouse=True )
def require_dual_rgb( test_device ):
    dev, _ = test_device
    helper.skip_without_color1( dev )


@pytest.fixture
def depth_sensor( test_device ):
    dev, _ = test_device
    yield dev.first_depth_sensor()


def check_depth_with_both_colors( sensor, w, h, fps=helper.FPS ):
    streams = ((DEPTH, rs.format.z16, 2), (COLOR0, rs.format.rgb8, 3), (COLOR1, rs.format.rgb8, 3))
    profiles = [helper.require_profile( sensor, key, fmt, w, h, fps ) for key, fmt, _ in streams]
    for _ in range( 2 ):   # the second start fails if the first stop left the streams wedged
        numbers, sizes = helper.stream( sensor, profiles )
        for key, _, bpp in streams:
            helper.check_rate( numbers, key, fps )
            assert sizes[key] == w * h * bpp, f"{key[0]} {key[1]} frame is {sizes[key]} bytes, expected {w * h * bpp}"


# VGA and below only: from 848x480 up, the CI GMSL rig's capture drops a quarter of the frames
# (RSDEV-16611), while lower resolutions get every frame.
@pytest.mark.context( "nightly" )
def test_depth_with_both_colors( depth_sensor ):
    check_depth_with_both_colors( depth_sensor, 640, 480 )


@pytest.mark.context( "weekly" )
def test_depth_with_both_colors_424x240( depth_sensor ):
    check_depth_with_both_colors( depth_sensor, 424, 240 )


@pytest.mark.context( "nightly" )
@pytest.mark.device_type( "USB" )
def test_depth_with_both_colors_60fps( test_device, depth_sensor ):
    dev, _ = test_device
    require_min_fw_version( dev, rsutils.version( 5, 17, 5, 15 ), "Depth + both colors at 60 fps" )
    check_depth_with_both_colors( depth_sensor, 640, 480, 60 )
