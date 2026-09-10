# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import io
import zipfile

import numpy as np

from .conftest import client

URL = "/api/v1/devices/device1/stream/snapshot"


class _Profile:
    def fps(self):
        return 30

    def format(self):
        class _F:
            name = "z16"
        return _F()

    def unique_id(self):
        return 1

    def as_video_stream_profile(self):
        return self

    def width(self):
        return 3

    def height(self):
        return 2


class _Frame:
    def __init__(self):
        self.data = np.arange(6, dtype=np.uint16).reshape(2, 3)

    def get_profile(self):
        return _Profile()

    def supports_frame_metadata(self, _md):
        return False

    def get_timestamp(self):
        return 12.5

    def get_frame_number(self):
        return 99

    def get_frame_timestamp_domain(self):
        class _D:
            name = "global_time"
        return _D()

    def get_width(self):
        return 3

    def get_height(self):
        return 2

    def get_data(self):
        return self.data


def test_snapshot_downloads_a_zip_of_the_newest_frame(setup_mock_managers):
    rs_manager = setup_mock_managers["rs_manager"]
    rs_manager.last_frames["device1"] = {"depth": {"frame": _Frame(), "shown": np.zeros((2, 3, 3), np.uint8), "motion": None}}

    response = client.get(URL, params={"stream": "Depth"})

    assert response.status_code == 200
    assert response.headers["content-type"] == "application/zip"
    assert response.headers["content-disposition"] == 'attachment; filename="device1_depth_99.zip"'
    with zipfile.ZipFile(io.BytesIO(response.content)) as z:
        assert set(z.namelist()) == {"depth_99.png", "depth_99.raw", "depth_99_metadata.csv"}
        assert b"frame_number,99" in z.read("depth_99_metadata.csv")


def test_snapshot_without_a_frame_is_404(setup_mock_managers):
    assert client.get(URL, params={"stream": "color"}).status_code == 404
