# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# MJPEG and H.264 color pass through undecoded on D585 USB, in both sensor configurations: the dedicated color
# sensor (3C) and the first color stream of the depth-interface pins (2C). Frames carry the compressed payload
# at its received size, not padded to width*height.

import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D585"),
    pytest.mark.device_exclude("D585S"),
    pytest.mark.device_type("USB"),
    pytest.mark.requires(fw="7.62.47425.18412"),   # MS7 FW, where compressed color is published on 2C and 3C
]

W, H, FPS = 1280, 720, 30
FRAMES = 30


@pytest.mark.parametrize("fmt", [rs.format.mjpeg, rs.format.h264])
def test_compressed_color_passthrough(test_device, fmt):
    dev, ctx = test_device
    index = 1 if "Dual RGB" in dev.get_info(rs.camera_info.name) else 0   # 2C names its colors Color 1 / Color 2
    assert any(p.stream_type() == rs.stream.color and p.stream_index() == index and p.format() == fmt and p.fps() == FPS
               and p.as_video_stream_profile().width() == W and p.as_video_stream_profile().height() == H
               for s in dev.query_sensors() for p in s.get_stream_profiles()), f"FW publishes no {fmt} color {W}x{H}@{FPS}"

    pipe = rs.pipeline(ctx)
    cfg = rs.config()
    cfg.enable_device(dev.get_info(rs.camera_info.serial_number))
    cfg.enable_stream(rs.stream.color, index, W, H, fmt, FPS)
    pipe.start(cfg)
    sizes = []
    try:
        for i in range(FRAMES):
            color = pipe.wait_for_frames().get_color_frame(index)
            assert color.get_profile().format() == fmt
            head = bytes(color.get_data())[:8]
            if fmt == rs.format.mjpeg:
                assert head[:2] == b'\xff\xd8', "JPEG frame does not start with SOI"
            else:
                zeros = len(head) - len(head.lstrip(b'\0'))   # Annex B start code; leading zero bytes may precede it
                assert zeros >= 2 and head[zeros:zeros + 1] == b'\1', f"H.264 frame has no start code: {head.hex()}"
            sizes.append(color.get_data_size())
    finally:
        pipe.stop()

    log.debug(f"{fmt} frame sizes: {min(sizes)}..{max(sizes)}")
    assert 0 < max(sizes) < W * H, f"{fmt} frames are not compressed: {max(sizes)} bytes"
    if fmt == rs.format.h264:   # IDR and P frames differ in size; a JPEG of a static scene can repeat its size
        assert len(set(sizes)) > 1, f"{fmt} frame sizes should vary, all were {sizes[0]}"
