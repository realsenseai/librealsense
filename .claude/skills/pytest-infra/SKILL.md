---
name: pytest-infra
description: Write a new librealsense pytest test (pytest-*.py) - markers, fixtures, assertions, FW gating, temp files - or change the pytest infrastructure (conftest.py, rspy/pytest). Use before adding a Python test or modifying conftest.py / rspy.
---

# Pytest Tests & Infrastructure

## When to Use This Skill

- Adding a new Python test (`pytest-*.py`) under `unit-tests/`
- Modifying pytest infrastructure (`conftest.py`, `py/rspy/pytest/`, `py/rspy/devices.py`, hub modules)

All Python tests run under pytest. `run-unit-tests.py` is kept only for C++ tests (see the `testing` skill).

## Key Files

| File | Purpose |
|---|---|
| `unit-tests/conftest.py` | Hooks, fixtures, CLI flags, marker registration, default 200s timeout |
| `py/rspy/pytest/device_helpers.py` | Device parametrization (`device_each`), `require_min_fw_version` |
| `py/rspy/pytest/collection.py` | Collection filtering: context gating, `--live`, priority ordering |
| `py/rspy/pytest/logging_setup.py` | Per-test log files, terminal summary |
| `py/rspy/pytest/cli.py` | Legacy CLI flag translation (e.g. `-r` → `-k`) |
| `py/rspy/pytest/plugins.py` | Required pytest plugin registry |
| `py/rspy/devices.py` | Device discovery, hub port control (`enable_only()`) |

## Adding a New Test

1. **Location and name**: put the file in the matching topic directory (e.g. `unit-tests/live/frames/`) named `pytest-<name>.py`. Start with the copyright header (current year).
2. **Pick markers** (module-level `pytestmark = [...]` or per test):

   | Need | Marker |
   |---|---|
   | Any one matching device | `pytest.mark.device("D400*")` |
   | Run once per matching device | `pytest.mark.device_each("D400*")` |
   | Each device matching any of several patterns | `pytest.mark.device_each("D555", "D585")` |
   | Several devices at once (with `test_devices`) | `pytest.mark.device("D400*", "D500*")` |
   | Exclude SKUs | `pytest.mark.device_exclude("D401")` |
   | Only / never on a connection type | `pytest.mark.device_type("GMSL")` / `pytest.mark.device_type_exclude("DDS")` |
   | Nightly / weekly only | `pytest.mark.context("nightly")` |
   | Needs a DDS build | `pytest.mark.dds` |
   | Run earlier / later (default 500) | `pytest.mark.priority(100)` |
   | Longer than the 200s default | `pytest.mark.timeout(600)` |
   | Known flaky | `pytest.mark.flaky(reruns=2)` — **`reruns=`, never `retries=`** (silently ignored, guarded by an infra test) |

   - Exclude unsupported SKUs/transports with markers so they are skipped at collection — don't `pytest.skip()` at runtime for that.
   - Don't add `pytest.mark.live`; `--live` filtering is derived from the `device` markers.
   - A new custom marker must be registered in `conftest.py:pytest_configure`.
3. **Pick the lowest fixture you need**:

   | You need | Fixture |
   |---|---|
   | Device + context | `test_device` → `(dev, ctx)` |
   | D585S in service mode (no-op for others) | `test_device_wrapped` |
   | Several devices | `test_devices` → `([dev, ...], ctx)` |
   | Only a context | `test_context` |
   | Hub setup only, own context (e.g. DDS settings) | `module_device_setup` |
   | Active `--context` tags | `test_context_var` |
   | No device | none |

   The first test in a file recycles the device via the hub; later tests in the file reuse it.
4. **Write the test body**:
   - Logging: `import logging; log = logging.getLogger(__name__)`.
   - Hard stop: `assert`. To keep looping and report every failure: `from pytest_check import check` (`check.equal`, `check.is_true`, `check.almost_equal(a, b, abs=tol)`).
   - Minimum FW: `require_min_fw_version(dev, rsutils.version(5, 15, 0, 0), "FEATURE")` from `rspy.pytest.device_helpers` — no inline version checks.
   - Files written by the test go under the `tmp_path` fixture, never `tempfile.mkdtemp()`.
   - Don't wrap the test in `try/except`; let pytest report the exception. Expected exceptions: `with pytest.raises(...)`. Test-scoped cleanup: a `yield` fixture. If you must catch, keep the original error (`raise` or `pytest.fail(f"...: {e}")`), never `except: pass`.
   - Reuse helpers (`rspy.snippets`, `rspy.pytest.device_helpers`) before writing new ones. Helper module names must be unique across `unit-tests/` (pytest imports all of them in one process).
   - A test that depends on an earlier one: the earlier test sets a flag in a module-level dict, dependents `pytest.skip()` when it's missing.
5. **Run it locally** from `unit-tests/`: `python -m pytest -v live/frames/pytest-<name>.py`. Don't use `-s` when you need the per-test log files.

### Minimal example

```python
# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) <current year> RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [pytest.mark.device_each("D400*")]

def test_depth_streaming(test_device):
    dev, ctx = test_device
    pipe = rs.pipeline(ctx)
    cfg = rs.config()
    cfg.enable_stream(rs.stream.depth, rs.format.z16, 30)
    pipe.start(cfg)
    try:
        frames = pipe.wait_for_frames()
    finally:
        pipe.stop()
    assert frames.size() > 0
```

## Changing the Infrastructure

- **Infra regression tests** live in `unit-tests/infra-tests/`. They mock the hardware (no camera or pyrealsense2 needed) and run on GHA for every PR. Every change to `conftest.py` or `py/rspy/pytest/*.py` must pass `python -m pytest infra-tests/ -v` (from `unit-tests/`), and every new infra feature (marker, CLI flag, skip/fail rule) needs a test there. `test_e2e_*.py` files run real pytest subprocesses against the real `conftest.py` via `infra-tests/helpers.py:run_e2e()`.
- **New pytest plugin**: pin it in `unit-tests/requirements.txt` **and** add it to `REQUIRED_PYTEST_PLUGINS` in `py/rspy/pytest/plugins.py`. A missing plugin otherwise silently disables its CLI options; the registry makes `pytest_configure` fail loudly.
- **Hub ports**: `CombinedHub` numbers ports across hubs (Acroname first, then UniFi). `devices.enable_only()` derives the enabled ports on every call — don't add a cache.
