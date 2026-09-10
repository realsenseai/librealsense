# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from .conftest import client


def test_jobs_listed_by_device_and_fetched_by_id(patch_dependencies):
    registry = patch_dependencies["rs_manager"].jobs
    job = registry.create("firmware_update", "device1")
    registry.create("export_ply", "device2")

    assert [j["id"] for j in client.get("/api/v1/jobs/?device_id=device1").json()] == [job.id]
    assert len(client.get("/api/v1/jobs/").json()) == 2
    assert client.get(f"/api/v1/jobs/{job.id}").json()["kind"] == "firmware_update"
    assert client.get("/api/v1/jobs/nope").status_code == 404


def test_cancel_marks_the_request_and_refuses_twice_finished(patch_dependencies):
    registry = patch_dependencies["rs_manager"].jobs
    job = registry.create("calibration", "device1")

    assert client.post(f"/api/v1/jobs/{job.id}/cancel").status_code == 200
    assert job.cancel_requested
    job.cancelled()
    assert client.post(f"/api/v1/jobs/{job.id}/cancel").status_code == 409
