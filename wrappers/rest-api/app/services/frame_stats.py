# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Per-stream frame-drop statistics, as the legacy output console's "Frame Drops per Second"
dashboard computes them (common/output-model.cpp frame_drops_dashboard): a gap between two
consecutive frames longer than 1.5 frame periods counts as a drop; frames and drops are
totalled per one-second window."""

import threading
from typing import Dict, Optional


class StreamFrameStats:
    def __init__(self, fps: float):
        self.fps = fps
        self.last_ts: Optional[float] = None
        self.window_start: Optional[float] = None
        self.frames = 0
        self.drops = 0
        self.frames_per_second = 0
        self.drops_per_second = 0

    def observe(self, ts_ms: float, fps: Optional[float] = None) -> None:
        if fps:
            self.fps = fps
        if self.last_ts is not None and self.fps > 0 and (ts_ms - self.last_ts) > 1.5 * (1000.0 / self.fps):
            self.drops += 1
        self.last_ts = ts_ms
        self.frames += 1
        if self.window_start is None:
            self.window_start = ts_ms
        elif ts_ms - self.window_start >= 1000.0:
            self.frames_per_second = self.frames
            self.drops_per_second = self.drops
            self.frames = 0
            self.drops = 0
            self.window_start = ts_ms

    def snapshot(self) -> Dict[str, float]:
        return {"frames_per_second": self.frames_per_second, "drops_per_second": self.drops_per_second, "expected_fps": self.fps}


class FrameStats:
    """Thread-safe registry of StreamFrameStats keyed by "device:stream"."""

    def __init__(self):
        self._streams: Dict[str, StreamFrameStats] = {}
        self._lock = threading.Lock()

    def observe(self, key: str, ts_ms: float, fps: float) -> Dict[str, float]:
        with self._lock:
            stats = self._streams.get(key)
            if stats is None:
                stats = self._streams[key] = StreamFrameStats(fps)
            stats.observe(ts_ms, fps)
            return stats.snapshot()

    def forget(self, prefix: str) -> None:
        with self._lock:
            for key in [k for k in self._streams if k.startswith(prefix)]:
                del self._streams[key]
