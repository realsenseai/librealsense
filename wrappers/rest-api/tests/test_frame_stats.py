# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from app.services.frame_stats import FrameStats, StreamFrameStats


def test_a_gap_longer_than_one_and_a_half_periods_is_a_drop():
    s = StreamFrameStats(30.0)
    t = 0.0
    for _ in range(15):
        s.observe(t)
        t += 33.3
    t += 100  # three frames missing
    for _ in range(15):
        s.observe(t)
        t += 33.3
    # the 30th frame lands past the one-second mark and closes the window
    snap = s.snapshot()
    # 29 frames landed inside the first second (three were missing), one gap counted as a drop
    assert snap["frames_per_second"] == 29 and snap["drops_per_second"] == 1 and snap["expected_fps"] == 30.0


def test_registry_keys_streams_and_forgets_a_device():
    stats = FrameStats()
    for i in range(3):
        snap = stats.observe("d1:depth", i * 33.3, 30.0)
    assert snap["frames_per_second"] == 0  # window not closed yet
    stats.forget("d1:")
    assert stats._streams == {}
