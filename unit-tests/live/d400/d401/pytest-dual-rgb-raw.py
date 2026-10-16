# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# D401 GMSL raw dual-RGB: both imagers stream as Color 0 / Color 1 through the raw (CSI pass-through)
# path. Kept apart from the ISP-mode tests: switching raw <-> ISP needs a hardware reset, which the
# test infra does only between files. Nightly: gating runs the golden FW/driver, which predate the
# raw path, so only the nightly with the latest FW/driver can run these.

import pytest
import pyrealsense2 as rs
import d401_helper as helper
from d401_helper import COLOR0, COLOR1, DEPTH, IR2

pytestmark = [
    pytest.mark.device_each( "D401" ),
    pytest.mark.device_type( "GMSL" ),
    pytest.mark.context( "nightly" ),
]

W, H = 848, 480


@pytest.fixture( scope="module", autouse=True )
def require_dual_rgb( test_device ):
    dev, _ = test_device
    helper.skip_without_color1( dev )


@pytest.fixture
def depth_sensor( test_device ):
    dev, _ = test_device
    yield dev.first_depth_sensor()


def color( sensor, key, w=W, h=H ):
    return helper.need_profile( sensor, key, rs.format.rgb8, w, h )


def test_color1_profiles_also_on_color0( depth_sensor ):
    """Every Color 1 RGB8 profile is also published for Color 0 (Color 0 adds the ISP profiles)."""
    def key( p ):
        vp = p.as_video_stream_profile()
        return vp.width(), vp.height(), p.fps()
    colors = { COLOR0: set(), COLOR1: set() }
    for p in depth_sensor.get_stream_profiles():
        k = (p.stream_type(), p.stream_index())
        if k in colors and p.format() == rs.format.rgb8 and p.as_video_stream_profile():
            colors[k].add( key( p ) )
    assert colors[COLOR1], "no Color 1 RGB8 profiles"
    missing = colors[COLOR1] - colors[COLOR0]
    assert not missing, f"Color 1 profiles missing on Color 0: {sorted( missing )}"


def test_both_colors_stream( depth_sensor ):
    """Color 0 + Color 1 stream together at the full rate, as full RGB8 frames, each with its own counter."""
    numbers, sizes = helper.stream( depth_sensor, [color( depth_sensor, COLOR0 ), color( depth_sensor, COLOR1 )] )
    assert set( numbers ) == { COLOR0, COLOR1 }, f"unexpected streams delivered: {sorted( numbers )}"
    n0 = helper.check_rate( numbers, COLOR0 )
    n1 = helper.check_rate( numbers, COLOR1 )
    assert abs( n0 - n1 ) <= helper.FPS * helper.DURATION_S * 0.1, f"Color 0 ({n0}) and Color 1 ({n1}) rates differ"
    for key in (COLOR0, COLOR1):
        assert sizes[key] == W * H * 3, f"Color {key[1]} frame is {sizes[key]} bytes, expected {W * H * 3}"
        got = numbers[key]
        assert all( b > a for a, b in zip( got, got[1:] ) ), f"Color {key[1]} frame numbers not increasing"
        # Both imagers share one HW frame counter; without per-stream counting each stream steps by ~2
        step = (got[-1] - got[0]) / (len( got ) - 1)
        assert step < 1.5, f"Color {key[1]} frame numbers step by {step:.2f} per frame (shared counter?)"


@pytest.mark.parametrize( "w,h", [(848, 480), (1280, 720)] )
def test_depth_with_color1( depth_sensor, w, h ):
    """Depth + Color 1 (one raw color stream) delivers both. This used to crash: the packed RAW10 copy
    was skipped unless two color streams were open, and the copy read past the frame."""
    depth = helper.need_profile( depth_sensor, DEPTH, rs.format.z16, w, h )
    numbers, _ = helper.stream( depth_sensor, [depth, color( depth_sensor, COLOR1, w, h )] )
    helper.check_rate( numbers, DEPTH )
    helper.check_rate( numbers, COLOR1 )


def test_color1_and_ir2_rejected( depth_sensor ):
    """Color 1 and IR 2 come off the same imager pin, so opening them together must be refused."""
    ir2 = helper.need_profile( depth_sensor, IR2, rs.format.y8, W, H )
    try:
        with pytest.raises( RuntimeError, match="same hardware endpoint" ):
            depth_sensor.open( [color( depth_sensor, COLOR1 ), ir2] )
    finally:
        try:
            depth_sensor.close()   # only open if the open() unexpectedly succeeded
        except RuntimeError:
            pass


def test_color1_calibration( depth_sensor ):
    """Color 1 carries real intrinsics, and its extrinsics put it a stereo baseline away from Color 0."""
    c0 = color( depth_sensor, COLOR0 )
    c1 = color( depth_sensor, COLOR1 )
    i = c1.as_video_stream_profile().get_intrinsics()
    assert (i.width, i.height) == (W, H), f"Color 1 intrinsics are for {i.width}x{i.height}"
    assert i.fx > 0 and i.fy > 0, f"Color 1 focal length fx={i.fx} fy={i.fy}"
    assert 0 < i.ppx < W and 0 < i.ppy < H, f"Color 1 principal point ({i.ppx}, {i.ppy}) outside the image"

    e = c0.get_extrinsics_to( c1 )
    r = e.rotation   # column-major 3x3
    det = r[0] * (r[4] * r[8] - r[7] * r[5]) - r[3] * (r[1] * r[8] - r[7] * r[2]) + r[6] * (r[1] * r[5] - r[4] * r[2])
    assert abs( det - 1 ) < 1e-3, f"Color 0 -> Color 1 rotation is not a rotation (det={det:.4f})"
    # Two different imagers: the D401 baseline is ~18 mm, so anything near 0 means a copied calibration
    t = sum( v * v for v in e.translation ) ** 0.5
    assert 0.005 < t < 0.1, f"Color 0 -> Color 1 translation is {t * 1000:.1f} mm"
