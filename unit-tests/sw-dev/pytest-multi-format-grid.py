# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
import numpy as np
import cv2
import logging

import sw_device as sw

log = logging.getLogger(__name__)
pytestmark = [pytest.mark.timeout(60)]

W, H = 640, 480
# average per-channel pixel diff between SDK and cv2 outputs on the same input.
# observed ~0.58-0.77; tight tolerance to flag any change.
TOL_MEAN = 1

_FOUR_TWO_TWO = {rs.format.yuyv, rs.format.uyvy}
_WITH_ALPHA  = {rs.format.rgba8, rs.format.bgra8}

_CONVERTERS = {
    rs.format.yuyv: rs.yuy2_converter,
    rs.format.uyvy: rs.uyvy_converter,
    rs.format.nv12: rs.nv12_converter,
    rs.format.m420: rs.m420_converter,
}


def _decode_to_rgb(raw, in_fmt):
    if in_fmt == rs.format.yuyv:
        return cv2.cvtColor(raw.reshape(H, W, 2), cv2.COLOR_YUV2RGB_YUYV)
    if in_fmt == rs.format.uyvy:
        return cv2.cvtColor(raw.reshape(H, W, 2), cv2.COLOR_YUV2RGB_UYVY)
    if in_fmt == rs.format.nv12:
        return cv2.cvtColor(raw.reshape(H * 3 // 2, W), cv2.COLOR_YUV2RGB_NV12)
    # M420: rearrange (2 Y rows + 1 UV row) -> NV12 layout, then cv2 NV12. SDK's
    # m420_parse_one_line interleaves UV as `u0 v0 u2 v2 ...` — same as NV12.
    blocks = raw.reshape(H // 2, 3, W)
    nv12 = np.vstack([blocks[:, :2, :].reshape(H, W), blocks[:, 2, :]])
    return cv2.cvtColor(nv12, cv2.COLOR_YUV2RGB_NV12)


def cv2_convert(raw, in_fmt, out_fmt):
    rgb = _decode_to_rgb(raw, in_fmt)
    if out_fmt == rs.format.rgb8:
        return rgb
    if out_fmt == rs.format.bgr8:
        return rgb[..., ::-1]
    alpha = np.full(rgb.shape[:2] + (1,), 255, dtype=np.uint8)
    if out_fmt == rs.format.rgba8:
        return np.concatenate([rgb, alpha], axis=-1)
    if out_fmt == rs.format.bgra8:
        return np.concatenate([rgb[..., ::-1], alpha], axis=-1)
    raise ValueError(f"unsupported output format {out_fmt}")


def sdk_convert(raw, in_fmt, out_fmt):
    """Inject `raw` (in_fmt wire bytes) through sw_device, run the SDK's converter
    for in_fmt -> out_fmt, return decoded RGB/BGR(A) array."""
    bpp = 2 if in_fmt in _FOUR_TWO_TWO else 1
    channels = 4 if out_fmt in _WITH_ALPHA else 3
    with sw.sensor("test") as s:
        stream = s.video_stream("Color", rs.stream.color, in_fmt, bpp)
        s.start(stream)
        f = stream.frame()
        f.pixels = raw
        received = s.publish(f)
        out_frame = _CONVERTERS[in_fmt](out_fmt).process(received)
        return np.asanyarray(out_frame.get_data()).reshape(H, W, channels).copy()


INPUTS  = [rs.format.yuyv, rs.format.uyvy, rs.format.nv12, rs.format.m420]
# Y8/Y16 can be added — SDK supports them from YUYV/NV12/M420 but not from UYVY.
OUTPUTS = [rs.format.rgb8, rs.format.rgba8, rs.format.bgr8, rs.format.bgra8]

@pytest.mark.parametrize("in_fmt",  INPUTS,  ids=[f.name for f in INPUTS])
@pytest.mark.parametrize("out_fmt", OUTPUTS, ids=[f.name for f in OUTPUTS])
def test_sdk_vs_cv2(in_fmt, out_fmt):
    """SDK converter (in_fmt -> out_fmt) vs cv2 reference on the same raw bytes."""
    size = W * H * 2 if in_fmt in _FOUR_TWO_TWO else W * H * 3 // 2
    raw = np.random.default_rng(0).integers(0, 256, size=size, dtype=np.uint8)
    sdk_out = sdk_convert(raw, in_fmt, out_fmt)
    ref = cv2_convert(raw, in_fmt, out_fmt)
    diff = np.subtract(sdk_out, ref, dtype=np.int16)
    mean = np.abs(diff).mean()
    log.info(f"{in_fmt} -> {out_fmt}  mean |d|={mean:.2f}")
    assert mean <= TOL_MEAN, f"{in_fmt} -> {out_fmt} mean |d|={mean:.2f} > {TOL_MEAN}"
