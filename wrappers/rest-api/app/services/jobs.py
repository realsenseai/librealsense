# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Registry of long-running operations, reported to clients over one Socket.IO event.

A worker thread drives a Job: ``progress()`` while it runs, then ``done()`` or ``fail()``.
Every change is emitted as ``job`` with the job's snapshot, throttled to ~10/s except for
the terminal states, which always go out.
"""

import threading
import time
import uuid
from collections import OrderedDict
from typing import Any, Callable, Dict, List, Optional

from app.core.errors import RealSenseError
from app.models.job import JobInfo

EMIT_INTERVAL = 0.1
KEEP_FINISHED = 50


class JobCancelled(Exception):
    """Raised by ``Job.check_cancelled`` so a worker unwinds when the client cancels."""


class Job:
    def __init__(self, registry: "JobRegistry", kind: str, device_id: Optional[str]):
        now = time.time()
        self.info = JobInfo(id=uuid.uuid4().hex[:12], kind=kind, device_id=device_id, created_at=now, updated_at=now)
        self._registry = registry
        self._cancel_requested = threading.Event()
        self._last_emit = 0.0

    @property
    def id(self) -> str:
        return self.info.id

    @property
    def cancel_requested(self) -> bool:
        return self._cancel_requested.is_set()

    def check_cancelled(self) -> None:
        if self.cancel_requested:
            raise JobCancelled()

    def progress(self, fraction: float, message: Optional[str] = None) -> None:
        if self.info.state != "running":
            return
        self.info.progress = max(0.0, min(1.0, float(fraction)))
        if message is not None:
            self.info.message = message
        self._touch(force=self.info.progress >= 1.0)

    def done(self, result: Any = None, message: Optional[str] = None) -> None:
        self._finish("done", result=result, message=message, progress=1.0)

    def fail(self, error: str) -> None:
        self._finish("failed", error=error)

    def cancelled(self) -> None:
        self._finish("cancelled")

    def _finish(self, state: str, result: Any = None, error: Optional[str] = None,
                message: Optional[str] = None, progress: Optional[float] = None) -> None:
        if self.info.state != "running":
            return
        self.info.state = state  # type: ignore[assignment]
        self.info.result = result
        self.info.error = error
        if message is not None:
            self.info.message = message
        if progress is not None:
            self.info.progress = progress
        self._touch(force=True)

    def _touch(self, force: bool) -> None:
        now = time.time()
        self.info.updated_at = now
        if force or now - self._last_emit >= EMIT_INTERVAL:
            self._last_emit = now
            self._registry._emit("job", self.info.model_dump())


class JobRegistry:
    def __init__(self, emit: Callable[[str, Dict[str, Any]], None]):
        self._emit = emit
        self._jobs: "OrderedDict[str, Job]" = OrderedDict()
        self._lock = threading.Lock()

    def create(self, kind: str, device_id: Optional[str] = None) -> Job:
        job = Job(self, kind, device_id)
        with self._lock:
            self._jobs[job.id] = job
            self._prune()
        job._touch(force=True)
        return job

    def get(self, job_id: str) -> Job:
        with self._lock:
            job = self._jobs.get(job_id)
        if job is None:
            raise RealSenseError(status_code=404, detail=f"Job {job_id} not found")
        return job

    def list(self, device_id: Optional[str] = None) -> List[JobInfo]:
        with self._lock:
            jobs = list(self._jobs.values())
        return [j.info for j in jobs if device_id is None or j.info.device_id == device_id]

    def cancel(self, job_id: str) -> JobInfo:
        """Ask the worker to stop; the job stays 'running' until the worker acknowledges."""
        job = self.get(job_id)
        if job.info.state != "running":
            raise RealSenseError(status_code=409, detail=f"Job {job_id} is already {job.info.state}")
        job._cancel_requested.set()
        return job.info

    def _prune(self) -> None:
        finished = [jid for jid, j in self._jobs.items() if j.info.state != "running"]
        for jid in finished[:-KEEP_FINISHED] if len(finished) > KEEP_FINISHED else []:
            del self._jobs[jid]
