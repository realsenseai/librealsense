# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Frame snapshots, the legacy viewer's "save" button (stream-model.cpp snapshot_frame):
PNG of what is shown, the raw pixels, and the frame attributes as CSV, zipped together."""

import csv
import io
import zipfile
from typing import Any, Dict, Optional, Tuple

import cv2
import numpy as np


def _png(image: np.ndarray, pixel_format: str) -> Optional[bytes]:
    """Encode a frame's pixels as PNG; None for layouts OpenCV cannot write."""
    fmt = pixel_format.lower()
    if image.ndim == 3 and image.shape[2] == 3 and fmt != "bgr8":
        image = cv2.cvtColor(image, cv2.COLOR_RGB2BGR)  # PNG encoder wants BGR order
    if image.ndim == 2 and image.dtype not in (np.uint8, np.uint16):
        return None
    ok, buf = cv2.imencode(".png", np.ascontiguousarray(image))
    return buf.tobytes() if ok else None


def _csv(rows) -> bytes:
    out = io.StringIO()
    writer = csv.writer(out, lineterminator="\n")
    for row in rows:
        writer.writerow(row)
    return out.getvalue().encode("utf-8")


def build_snapshot(
    stream: str,
    frame_number: int,
    raw: Optional[np.ndarray],
    pixel_format: str,
    shown: Optional[np.ndarray],
    metadata: Dict[str, Any],
    motion: Optional[Dict[str, float]] = None,
) -> Tuple[str, bytes]:
    """Return (base file name, zip bytes).

    ``raw`` is the frame as the camera delivered it (after post-processing for depth),
    ``shown`` the image the viewer displays (colorized depth) when that differs. Motion
    frames carry no image: they get a CSV with the sample instead.
    """
    base = f"{stream}_{frame_number}"
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w", zipfile.ZIP_DEFLATED) as z:
        if motion is not None:
            z.writestr(f"{base}.csv", _csv([["x", "y", "z"], [motion["x"], motion["y"], motion["z"]]] +
                                           [[k, v] for k, v in metadata.items()]))
        else:
            image = shown if shown is not None else raw
            png = _png(image, "rgb8" if shown is not None else pixel_format) if image is not None else None
            if png:
                z.writestr(f"{base}.png", png)
            if raw is not None:
                z.writestr(f"{base}.raw", np.ascontiguousarray(raw).tobytes())
            z.writestr(f"{base}_metadata.csv", _csv([["Stream", stream]] + [[k, v] for k, v in metadata.items()]))
    return base, buffer.getvalue()
