# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Frame-level pytest helpers."""

from pytest_check import check


def check_frame_drops(frame, previous_frame_number, allowed_drops=1, allow_frame_counter_reset=False):
    """Detect frame drops between consecutive frames; fail the test on drop."""
    n = frame.get_frame_number()
    # special case for D400, because the depth sensor may reset itself
    if previous_frame_number > 0 and not (allow_frame_counter_reset and n < 5):
        dropped = n - (previous_frame_number + 1)
        if dropped > allowed_drops:
            check.fail(f"{dropped} frame(s) before {frame} were dropped")
        elif dropped < 0:
            check.fail(f"Frames repeated or out of order. Got {frame} after frame {previous_frame_number}")
