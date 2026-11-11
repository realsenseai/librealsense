# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import numpy as np
import pyrealsense2 as rs

pytestmark = [
    pytest.mark.device("D400*"),
    pytest.mark.device_each("D500*"),
]


@pytest.mark.parametrize("equalize, color_scheme", [(1, 0), (0, 2)])
def test_color_table_matches_colorize(test_device, equalize, color_scheme):
    """Coloring each pixel through get_color_table() must give exactly what colorize() paints"""
    dev, ctx = test_device
    pipeline = rs.pipeline(ctx)
    cfg = rs.config()
    cfg.enable_device(dev.get_info(rs.camera_info.serial_number))
    cfg.enable_stream(rs.stream.depth)
    pipeline.start(cfg)
    try:
        for _ in range(30):  # let auto exposure settle so the frame has a spread of depths
            depth = pipeline.wait_for_frames().get_depth_frame()
        colorizer = rs.colorizer()
        colorizer.set_option(rs.option.histogram_equalization_enabled, equalize)
        colorizer.set_option(rs.option.color_scheme, color_scheme)
        colorizer.set_option(rs.option.min_distance, 0)
        colorizer.set_option(rs.option.max_distance, 4)
        painted = np.asanyarray(colorizer.colorize(depth).get_data())
        table = np.frombuffer(colorizer.get_color_table(depth), dtype=np.uint8).reshape(-1, 3)
        assert table.shape == (0x10000, 3)
        assert (table[0] == 0).all()  # no depth is black
        if color_scheme == 2:  # white to black over 0-4m, independently of how the table is built
            assert (table[1] >= 250).all() and (table[0xFFFF] == 0).all()
        assert np.array_equal(table[np.asanyarray(depth.get_data())], painted)
    finally:
        pipeline.stop()
