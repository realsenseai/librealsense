# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs


def test_load_recording_without_device_description(tmp_path):
    # A recording that never got a frame holds only /file_version; loading it must fail, not hang
    filename = str(tmp_path / 'empty.db3')
    sd = rs.software_device()
    sd.add_sensor("Depth")
    rec = rs.recorder(filename, sd)
    del rec

    with pytest.raises(RuntimeError, match="no device description"):
        rs.context().load_device(filename)
