# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Unit tests for RealSenseManager._get_frame_metadata.

Uses a stub frame against the real pyrealsense2 enum — no live device needed.
"""

import pyrealsense2 as rs
from unittest.mock import MagicMock

from app.services.rs_manager import RealSenseManager


_ALL_MD = list(rs.frame_metadata_value.__members__.values())


class _StubFrame:
    def __init__(self, supported_set, *, profile_uid=1):
        self.supported_set = set(supported_set)
        self._profile_uid = profile_uid

    def supports_frame_metadata(self, md):
        return md in self.supported_set

    def get_frame_metadata(self, md):
        return int(md)

    def get_profile(self):
        outer = self
        class _P:
            def unique_id(self):
                return outer._profile_uid
        return _P()


def test_returns_only_supported_keys():
    mgr = RealSenseManager(MagicMock())
    supported = _ALL_MD[:3]
    frame = _StubFrame(supported)

    attrs = mgr._get_frame_metadata(frame)

    assert set(attrs.keys()) == {md.name for md in supported}


def test_cache_is_per_profile_uid():
    mgr = RealSenseManager(MagicMock())
    supported_a = _ALL_MD[:2]
    supported_b = _ALL_MD[2:5]
    frame_a = _StubFrame(supported_a, profile_uid=1)
    frame_b = _StubFrame(supported_b, profile_uid=2)

    attrs_a = mgr._get_frame_metadata(frame_a)
    attrs_b = mgr._get_frame_metadata(frame_b)

    assert set(attrs_a.keys()) == {md.name for md in supported_a}
    assert set(attrs_b.keys()) == {md.name for md in supported_b}
