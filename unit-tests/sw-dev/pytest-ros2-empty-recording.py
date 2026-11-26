# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import sqlite3
import pytest
import pyrealsense2 as rs


def record_without_frames(filename):
    sd = rs.software_device()
    sd.add_sensor("Depth").add_option(rs.option.gain, rs.option_range(0, 100, 1, 1), True)
    rec = rs.recorder(filename, sd)
    rec.query_sensors()[0].set_option(rs.option.gain, 40)  # set after the recorder is created
    del rec


def test_recording_without_frames_loads(tmp_path):
    filename = str(tmp_path / 'no-frames.db3')
    record_without_frames(filename)

    dev = rs.context().load_device(filename)
    sensors = dev.query_sensors()
    assert len(sensors) == 1
    assert sensors[0].get_option(rs.option.gain) == 40


def test_recording_without_device_description_raises(tmp_path):
    # Keep only /file_version; loading must fail, not hang
    filename = str(tmp_path / 'no-description.db3')
    record_without_frames(filename)
    with sqlite3.connect(filename) as conn:
        conn.execute("DELETE FROM messages WHERE topic_id NOT IN (SELECT id FROM topics WHERE name = '/file_version')")
        conn.execute("DELETE FROM topics WHERE name != '/file_version'")

    with pytest.raises(RuntimeError, match="no device description"):
        rs.context().load_device(filename)
