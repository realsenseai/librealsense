# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from app.services import ply_export
from .conftest import client

URL = "/api/v1/devices/device1/point_cloud/export"


class _FakePly:
    """rs.save_to_ply that records its options and writes a PLY header."""
    option_ply_mesh = "mesh"
    option_ply_normals = "normals"
    option_ply_binary = "binary"
    last = None

    def __init__(self, path):
        self.path = path
        self.options = {}
        _FakePly.last = self

    def set_option(self, option, value):
        self.options[option] = value

    def process(self, frame):
        with open(self.path, "wb") as f:
            f.write(b"ply\nformat " + (b"binary_little_endian" if self.options["binary"] else b"ascii") + b" 1.0\nend_header\n")


def test_export_needs_a_depth_frame(setup_mock_managers):
    response = client.post(URL, json={})
    assert response.status_code == 409


def test_export_downloads_a_ply_with_the_requested_options(setup_mock_managers, monkeypatch):
    rs_manager = setup_mock_managers["rs_manager"]
    monkeypatch.setattr(ply_export.rs, "save_to_ply", _FakePly)
    rs_manager.depth_frames["device1"] = object()

    response = client.post(URL, json={"mesh": False, "normals": True, "binary": False})
    assert response.status_code == 200
    assert response.headers["content-disposition"] == 'attachment; filename="device1.ply"'
    assert response.content.startswith(b"ply\nformat ascii 1.0")
    assert _FakePly.last.options == {"mesh": 0.0, "normals": 1.0, "binary": 0.0}

    assert client.post(URL, json={}).content.startswith(b"ply\nformat binary_little_endian")
