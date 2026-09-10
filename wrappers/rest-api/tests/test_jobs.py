# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest

from app.core.errors import RealSenseError
from app.services import jobs as jobs_module
from app.services.jobs import JobCancelled, JobRegistry


@pytest.fixture
def registry(monkeypatch):
    monkeypatch.setattr(jobs_module, "EMIT_INTERVAL", 0.0)  # every progress call goes out
    emitted = []
    reg = JobRegistry(lambda event, payload: emitted.append((event, payload)))
    reg.emitted = emitted
    return reg


def test_create_emits_a_running_snapshot(registry):
    job = registry.create("firmware_update", "serial-A")
    assert registry.emitted == [("job", job.info.model_dump())]
    assert job.info.state == "running" and job.info.progress == 0.0
    assert registry.get(job.id) is job
    assert [j.id for j in registry.list("serial-A")] == [job.id]
    assert registry.list("serial-B") == []


def test_progress_then_done_carries_message_and_result(registry):
    job = registry.create("export_ply")
    job.progress(0.5, "meshing")
    job.done({"path": "cloud.ply"})

    states = [(p["state"], p["progress"], p["message"]) for _, p in registry.emitted]
    assert states == [("running", 0.0, None), ("running", 0.5, "meshing"), ("done", 1.0, "meshing")]
    assert registry.get(job.id).info.result == {"path": "cloud.ply"}


def test_fail_is_terminal(registry):
    job = registry.create("calibration")
    job.fail("boom")
    job.progress(0.9)  # ignored after the terminal state
    assert job.info.state == "failed" and job.info.error == "boom" and job.info.progress == 0.0


def test_cancel_flags_the_worker_and_refuses_finished_jobs(registry):
    job = registry.create("calibration")
    registry.cancel(job.id)
    assert job.cancel_requested
    with pytest.raises(JobCancelled):
        job.check_cancelled()
    job.cancelled()
    with pytest.raises(RealSenseError) as exc:
        registry.cancel(job.id)
    assert exc.value.status_code == 409


def test_unknown_job_is_404(registry):
    with pytest.raises(RealSenseError) as exc:
        registry.get("nope")
    assert exc.value.status_code == 404


def test_progress_emits_are_throttled_but_terminal_always_goes_out(monkeypatch):
    monkeypatch.setattr(jobs_module, "EMIT_INTERVAL", 3600.0)
    emitted = []
    job = JobRegistry(lambda e, p: emitted.append(p)).create("x")
    job.progress(0.1)
    job.progress(0.2)
    assert len(emitted) == 1  # only the create snapshot made it through
    job.done()
    assert emitted[-1]["state"] == "done"


def test_finished_jobs_are_pruned(registry):
    for i in range(jobs_module.KEEP_FINISHED + 5):
        registry.create("x").done()
    running = registry.create("y")
    assert len(registry.list()) == jobs_module.KEEP_FINISHED + 1
    assert registry.get(running.id) is running
