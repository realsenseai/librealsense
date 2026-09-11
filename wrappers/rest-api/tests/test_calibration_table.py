# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import struct
import zlib

import pytest

from app.core.errors import RealSenseError
from app.services import calibration_table as ct


def _table():
    data = bytearray(ct.TABLE_SIZE)
    struct.pack_into("<HHII", data, 0, 0x0102, 25, ct.TABLE_SIZE, 0)  # version 2.1 (big-endian on the wire)
    ct.F3X3.pack_into(data, ct.OFF_INTRINSICS, 0.5, 0, 0.5, 0, 0.9, 0.5, 0, 0, 1)
    struct.pack_into("<f", data, ct.OFF_BASELINE, 95.0)
    struct.pack_into("<I", data, ct.OFF_BROWN, 1)
    ct.F4.pack_into(data, ct.OFF_RECT + 3 * ct.F4.size, 424.0, 424.0, 424.0, 240.0)  # 848x480
    struct.pack_into("<I", data, 12, zlib.crc32(bytes(data[ct.HEADER_SIZE:])) & 0xFFFFFFFF)
    return bytes(data)


def test_parse_reads_the_legacy_editor_fields():
    t = ct.parse(_table())
    assert t["version"] == "2.1" and t["table_type"] == 25 and t["crc_valid"] is True
    assert t["intrinsic_left"] == [[0.5, 0, 0.5], [0, pytest.approx(0.9), 0.5], [0, 0, 1]]
    assert t["baseline"] == 95.0 and t["brown_model"] == 1
    assert t["rect_params"][3] == {"resolution": "848x480", "fx": 424.0, "fy": 424.0, "ppx": 424.0, "ppy": 240.0}
    assert len(t["rect_params"]) == 16


def test_patch_rewrites_fields_and_the_crc_only():
    original = _table()
    edited = ct.apply_patch(original, {"baseline": 96.5, "rect_params": [{"resolution": "848x480", "fx": 430.0}],
                                       "world2left_rot": [[1, 0, 0], [0, 1, 0], [0, 0, 1]]})
    t = ct.parse(edited)
    assert t["baseline"] == 96.5 and t["crc_valid"] is True
    assert t["rect_params"][3]["fx"] == 430.0 and t["rect_params"][3]["ppy"] == 240.0  # untouched fields stay
    assert t["world2left_rot"] == [[1, 0, 0], [0, 1, 0], [0, 0, 1]]
    assert edited[:12] == original[:12]  # header apart from the CRC
    assert edited[ct.OFF_BROWN:ct.OFF_RECT] == original[ct.OFF_BROWN:ct.OFF_RECT]  # reserved bytes preserved


def test_bad_input_is_refused():
    with pytest.raises(RealSenseError):
        ct.parse(b"\x00" * 10)
    with pytest.raises(RealSenseError):
        ct.apply_patch(_table(), {"intrinsic_left": [1, 2, 3]})
    with pytest.raises(RealSenseError):
        ct.apply_patch(_table(), {"rect_params": [{"fx": 1.0}]})
