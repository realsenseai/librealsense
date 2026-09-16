# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest

import app.api.dependencies as dependencies
from app.services.settings import SettingsStore
from .conftest import client


@pytest.fixture(autouse=True)
def settings_in_tmp(tmp_path):
    store = SettingsStore(tmp_path / "settings.json")
    client.app.dependency_overrides[dependencies.get_settings_store] = lambda: store
    yield store
    client.app.dependency_overrides.pop(dependencies.get_settings_store, None)


def test_get_settings_returns_every_group():
    body = client.get("/api/v1/settings/").json()
    assert set(body) == {"record", "update", "console", "paths", "context", "calibration", "post_processing", "viewer"}


def test_put_merges_and_returns_the_whole_settings(settings_in_tmp):
    response = client.put("/api/v1/settings/", json={"record": {"file_save_mode": "ask"}})
    assert response.status_code == 200
    assert response.json()["record"]["file_save_mode"] == "ask"
    assert response.json()["viewer"]["metric_system"] is True
    assert settings_in_tmp.get().record.file_save_mode == "ask"


def test_put_rejects_a_bad_value():
    response = client.put("/api/v1/settings/", json={"context": {"dds_domain": 999}})
    assert response.status_code == 422
