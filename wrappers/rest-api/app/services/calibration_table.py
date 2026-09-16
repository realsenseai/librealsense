# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""The D400 coefficients (calibration) table as the legacy table editor shows it
(common/calibration-model.cpp): left/right intrinsics, world-to-camera rotations, baseline and
the rectified focal length / principal point per resolution. Layout: src/ds/d400/d400-private.h
`d400_coefficients_table`; the CRC over everything after the header is the standard CRC-32."""

import struct
import zlib
from typing import Any, Dict, List

from app.core.errors import RealSenseError

HEADER = struct.Struct("<HHIII")  # version (big-endian on the wire, read raw), table type, size, param, crc32
HEADER_SIZE = HEADER.size  # 16
F3X3 = struct.Struct("<9f")
F4 = struct.Struct("<4f")
RESOLUTIONS = ["1920x1080", "1280x720", "640x480", "848x480", "640x360", "424x240", "320x240", "480x270",
               "1280x800", "960x540", "reserved", "reserved", "640x400", "576x576", "720x720", "1152x1152"]
OFF_INTRINSICS = HEADER_SIZE
OFF_BASELINE = OFF_INTRINSICS + 4 * F3X3.size  # 160
OFF_BROWN = OFF_BASELINE + 4
OFF_RECT = OFF_BROWN + 4 + 88  # 256
TABLE_SIZE = OFF_RECT + len(RESOLUTIONS) * F4.size + 64  # 576 with the trailing reserved block
MIN_TABLE_SIZE = OFF_RECT + len(RESOLUTIONS) * F4.size  # 512: what a D455 actually returns


def _matrix(data: bytes, offset: int) -> List[List[float]]:
    v = F3X3.unpack_from(data, offset)
    return [list(v[0:3]), list(v[3:6]), list(v[6:9])]


def parse(data: bytes) -> Dict[str, Any]:
    if len(data) < MIN_TABLE_SIZE:
        raise RealSenseError(status_code=422, detail=f"Calibration table too short ({len(data)} bytes); expected a D400 coefficients table")
    version_raw, table_type, table_size, param, crc = HEADER.unpack_from(data, 0)
    version = ((version_raw & 0xFF) << 8) | (version_raw >> 8)  # big-endian major.minor
    rect = []
    for i, name in enumerate(RESOLUTIONS):
        fx, fy, ppx, ppy = F4.unpack_from(data, OFF_RECT + i * F4.size)
        rect.append({"resolution": name, "fx": fx, "fy": fy, "ppx": ppx, "ppy": ppy})
    return {
        "version": f"{version >> 8}.{version & 0xFF}",
        "table_type": table_type,
        "table_size": table_size,
        "crc_valid": crc == zlib.crc32(data[HEADER_SIZE:len(data)]) & 0xFFFFFFFF,
        "intrinsic_left": _matrix(data, OFF_INTRINSICS),
        "intrinsic_right": _matrix(data, OFF_INTRINSICS + F3X3.size),
        "world2left_rot": _matrix(data, OFF_INTRINSICS + 2 * F3X3.size),
        "world2right_rot": _matrix(data, OFF_INTRINSICS + 3 * F3X3.size),
        "baseline": struct.unpack_from("<f", data, OFF_BASELINE)[0],
        "brown_model": struct.unpack_from("<I", data, OFF_BROWN)[0],
        "rect_params": rect,
    }


def _flat(m: Any, name: str) -> List[float]:
    try:
        flat = [float(x) for row in m for x in row] if m and isinstance(m[0], (list, tuple)) else [float(x) for x in m]
    except (TypeError, ValueError):
        raise RealSenseError(status_code=422, detail=f"{name} must be a 3x3 matrix")
    if len(flat) != 9:
        raise RealSenseError(status_code=422, detail=f"{name} must be a 3x3 matrix")
    return flat


def apply_patch(data: bytes, patch: Dict[str, Any]) -> bytes:
    """A copy of `data` with the given fields replaced and the CRC recomputed (the legacy
    editor's write path). Fields not mentioned keep their bytes, reserved areas included."""
    if len(data) < MIN_TABLE_SIZE:
        raise RealSenseError(status_code=422, detail="Calibration table too short")
    out = bytearray(data)
    for i, key in enumerate(("intrinsic_left", "intrinsic_right", "world2left_rot", "world2right_rot")):
        if key in patch and patch[key] is not None:
            F3X3.pack_into(out, OFF_INTRINSICS + i * F3X3.size, *_flat(patch[key], key))
    if patch.get("baseline") is not None:
        struct.pack_into("<f", out, OFF_BASELINE, float(patch["baseline"]))
    for entry in patch.get("rect_params") or []:
        index = entry.get("index")
        if index is None and "resolution" in entry:
            names = [r for r in RESOLUTIONS]
            index = names.index(entry["resolution"]) if entry["resolution"] in names else None
        if index is None or not 0 <= int(index) < len(RESOLUTIONS):
            raise RealSenseError(status_code=422, detail="rect_params entries need an index (0-15) or a known resolution")
        offset = OFF_RECT + int(index) * F4.size
        fx, fy, ppx, ppy = F4.unpack_from(out, offset)
        F4.pack_into(out, offset, float(entry.get("fx", fx)), float(entry.get("fy", fy)), float(entry.get("ppx", ppx)), float(entry.get("ppy", ppy)))
    crc = zlib.crc32(bytes(out[HEADER_SIZE:])) & 0xFFFFFFFF
    struct.pack_into("<I", out, 12, crc)
    return bytes(out)
