# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import json

import pytest

from app.services import updates
from .conftest import client

DB = {"versions": [
    {"component": "FIRMWARE", "policy_type": "RECOMMENDED", "device_name": "Test Device*", "platform": "*", "version": "5.17.0.10",
     "link": "https://librealsense.realsenseai.com/fw/test.bin", "release_notes_link": "https://x/notes", "description": "recommended fw"},
    {"component": "FIRMWARE", "policy_type": "ESSENTIAL", "device_name": "Test Device*", "platform": "*", "version": "5.16.0.1", "link": "https://x/e.bin"},
    {"component": "LIBREALSENSE", "policy_type": "RECOMMENDED", "device_name": "*", "platform": "*", "version": "2.60.0.0", "link": "https://x/sdk"},
    {"component": "LIBREALSENSE", "policy_type": "ESSENTIAL", "device_name": "*", "platform": "*", "version": "2.50.0.0", "link": "https://x/sdk-old"},
]}


@pytest.fixture
def db_file(tmp_path):
    path = tmp_path / "rs_versions_db.json"
    path.write_text(json.dumps(DB))
    updates._cache.clear()
    return path.as_uri()


def test_verdicts_per_component(db_file):
    # firmware 1.0.0 (the mock's) is below the essential 5.16 -> essential; SDK below recommended only
    result = updates.check(db_file, "Test Device 1", "1.0.0", "2.59.0.0")
    assert result["reachable"] is True
    assert result["firmware"]["verdict"] == "essential" and result["firmware"]["recommended"]["version"] == "5.17.0.10"
    assert result["software"]["verdict"] == "recommended" and result["software"]["recommended"]["link"] == "https://x/sdk"

    assert updates.check(db_file, "Test Device 1", "5.17.0.10", "2.60.0.0")["firmware"]["verdict"] == "up_to_date"
    assert updates.check(db_file, "Test Device 1", "5.16.5.0", "2.60.0.0")["firmware"]["verdict"] == "recommended"
    assert updates.check(db_file, "Other Camera", "1.0.0", "2.60.0.0")["firmware"]["verdict"] == "unknown"


def test_unreachable_db_is_reported_not_raised():
    updates._cache.clear()
    result = updates.check("file:///C:/nowhere/none.json", "Test Device 1", "1.0.0", "2.59.0.0")
    assert result["reachable"] is False and result["firmware"]["verdict"] == "unknown"


def test_endpoint_uses_the_custom_url_from_settings(setup_mock_managers, db_file):
    rs_manager = setup_mock_managers["rs_manager"]
    rs_manager.settings.update({"update": {"sw_update_official_server": False, "sw_update_url": db_file}})
    body = client.get("/api/v1/updates/device1").json()
    assert body["source"] == db_file and body["firmware"]["current"] == "1.0.0" and body["firmware"]["verdict"] == "essential"
    assert body["software"]["current"] not in (None, "")
