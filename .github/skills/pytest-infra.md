# Pytest Infrastructure & Test Migration

## When to Use This Skill

- Migrating a legacy test (`test-*.py`) to pytest (`pytest-*.py`)
- Modifying pytest infrastructure (`conftest.py`, `rspy/pytest/`, `rspy/devices.py`, `rspy/combined_hub.py`, `rspy/unifi.py`, etc.)
- Verifying Jenkins CI results after any infra or test changes

## Architecture Overview

### Two test frameworks coexist

1. **Legacy**: `run-unit-tests.py` orchestrates `test-*.py` files using `rspy.test` module
2. **Pytest**: `pytest` orchestrates `pytest-*.py` files using `conftest.py` + `rspy/pytest/` helpers

Both frameworks share the same device/hub infrastructure (`rspy/devices.py`, `rspy/device_hub.py`, `rspy/combined_hub.py`, `rspy/unifi.py`, `rspy/acroname.py`).

### Key files

| File | Purpose |
|---|---|
| `conftest.py` | Pytest hooks: device setup, logging, fixtures, CLI flags |
| `rspy/pytest/logging_setup.py` | Per-test log files, terminal summary |
| `rspy/pytest/device_helpers.py` | Device parametrization (`@device_each`) |
| `rspy/pytest/collection.py` | Test filtering (nightly, context gating, `--live`) |
| `rspy/pytest/cli.py` | Legacy CLI flag consumption |
| `rspy/devices.py` | Device discovery, port tracking, `enable_only()` |
| `rspy/combined_hub.py` | Virtual port mapping across multiple hubs |
| `rspy/device_hub.py` | Abstract hub interface |
| `rspy/unifi.py` | UniFi PoE switch control via SSH |
| `rspy/acroname.py` | Acroname USB hub control |

### Hub port management

- CI machines may have multiple hubs (e.g., Acroname + UniFi) wrapped in a `CombinedHub`
- `CombinedHub` assigns **virtual port numbers** sequentially: Acroname ports first (0-7), then UniFi ports (8-11)
- `devices.py` `enable_only()` derives currently enabled ports from `enabled()` + port mapping each call — no global cache

### Log file naming

Python (`logging_setup.py:test_log_name()`):
- `pytest-t2ff-pipeline.py::test_x[D455-SN]` → `pytest-t2ff-pipeline_D455-SN.log`
- `pytest-t2ff-pipeline.py::test_x` → `pytest-t2ff-pipeline.log`

The filename uses **file basename + device param from brackets only** — never the test function name.

### Jenkins report generation

Groovy files (`LRS_linux_compile_lib_ci.groovy`, `LRS_windows_compile_pipeline.groovy`) parse the pytest console log:
- Match `^(FAILED|ERROR) <file>::<test>` lines to generate clickable links
- Link target: `artifact/<outputFolder>/<config>/unit-tests/<logName>`
- Log name is reconstructed from the test name using the same bracket-extraction logic as Python

## Test Migration Checklist

When migrating a legacy `test-*.py` to `pytest-*.py`:

1. **Plan first**: Analyze the test file. Describe what you see, flag any `#test:` keywords or uncertainties, and wait for approval before writing code.

2. **Rename with `git mv`**: `git mv test-foo.py pytest-foo.py` (preserves history)

3. **Handle `#test:` directives**: For each one found in the legacy test:
   - `#test:device` / `#test:device each(...)` → `@pytest.mark.device(...)` / `@pytest.mark.device_each(...)`
   - `#test:donotrun:!nightly` → `@pytest.mark.context("nightly")`
   - `#test:timeout` → check if pytest has a native equivalent
   - `#test:platform` → `@pytest.mark.skipif(platform...)`
   - `#test:flag` → check for `pytest.mark` equivalent
   - If no match exists, ask the user before proceeding

4. **Prefer native pytest**: Native pytest features first → pytest plugins → custom implementation (last resort)

5. **No `pytest.mark.live`**: The `--live` CLI flag filters based on `device`/`device_each` markers automatically. `pytest.mark.live` is redundant.

6. **Test locally**: Run the new pytest test locally and verify it passes before considering done. Use `pytest -v unit-tests/live/.../pytest-foo.py` (without `-s` if checking log files — `-s` disables file logging).

7. **Delete the old file**: After migration is verified, the old `test-*.py` should not remain.

## Writing a New Pytest Test

### Fixture chain

The fixtures form a dependency chain. Use the lowest-level fixture that gives you what you need:

```
module_device_setup          → yields serial_number (or None)
    ↓                           Handles hub port management (enable/disable/recycle)
test_context                 → returns rs.context()
    ↓                           Depends on module_device_setup for hub state
test_device                  → returns (rs.device, rs.context)
                                Grabs the first visible device from context
```

### Which fixture to use

| You need | Use this fixture | Example |
|---|---|---|
| A device + context | `test_device` | Most hardware tests: streaming, options, metadata |
| Just a context (multiple devices or custom queries) | `test_context` | Multi-device tests, device enumeration tests |
| Only hub setup, you create your own context | `module_device_setup` | Tests that need custom context settings (e.g., DDS config) |
| No device at all | None (don't use any) | Algorithm tests, software-device tests |

### Device markers

```python
# Test runs once with any matching D400-series device
pytestmark = [pytest.mark.device("D400*")]

# Test runs once PER matching device (parametrized)
pytestmark = [pytest.mark.device_each("D400*")]

# Multiple markers = test runs for each marker's matches
pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
]

# Exclude specific devices
pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_exclude("D401"),  # D401 has no color sensor
]
```

### Example: simple test using `test_device`

```python
import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [pytest.mark.device_each("D400*")]

def test_depth_streaming(test_device):
    dev, ctx = test_device
    name = dev.get_info(rs.camera_info.name)
    log.info(f"Testing depth streaming on {name}")

    pipe = rs.pipeline(ctx)
    cfg = rs.config()
    cfg.enable_stream(rs.stream.depth, rs.format.z16, 30)
    pipe.start(cfg)
    frames = pipe.wait_for_frames()
    pipe.stop()

    assert frames.size() > 0
```

### Example: test using `test_context` for custom queries

```python
def test_device_count(test_context):
    devices = list(test_context.devices)
    log.info(f"Found {len(devices)} device(s)")
    assert len(devices) >= 1
```

### Example: test using `module_device_setup` with custom context

```python
def test_custom_context(module_device_setup):
    ctx = rs.context({"dds": {"enabled": True}})
    devices = list(ctx.devices)
    assert len(devices) >= 1
```

### Fixture scope and recycling behavior

- `module_device_setup` is **function-scoped** but tracks state per-module: the first test in a file triggers a hub recycle, subsequent tests in the same file reuse the device without recycling (unless the device changes or the test is a retry).
- `test_context` creates a **fresh `rs.context()`** per test.
- `test_device` grabs the **first visible device** from the context.

### Context-gated tests (nightly, weekly)

```python
@pytest.mark.context("nightly")  # only runs with --context nightly
def test_long_running(test_device):
    ...
```

## Verifying Jenkins CI Results

**CRITICAL: Never assume failures are unrelated to your changes.** Always investigate every failure.

### What to check in a Jenkins build

1. **Sub-build results**: Check all 4 platforms (Linux, Windows, Jetson JP5, Jetson JP6)
   ```
   curl -s -k -u "..." "https://rsjenkins.realsenseai.com/job/LRS_libci_pipeline/<N>/consoleText" | grep "completed:"
   ```

2. **Reports**: Get each sub-build's report artifact (`job.<N>.report`)

3. **For each failure**, verify:
   - Is the failing test one that passed in the previous baseline build?
   - Does the failure message make sense for the device it claims to test? (e.g., "D585S" test getting a "D435" error means wrong device was enabled)
   - Does the test log show the correct device setup sequence?

### Expected log patterns (legacy tests via `run-unit-tests.py`)

```
-D- configuration: [D400* -> D455_<SN>]
-D-     enabling ports [4] disabling currently enabled ports [8]
-D-     Disabling ports [1] on Unifi Switch        ← only hubs with ports to change
-D-     Enabling ports [4] on Acroname              ← only the needed hub
-D-     device removed: <old_SN>
-D-     device added: <new_SN> <pyrealsense2.device: D455 ...>
-D-     running: ['/usr/bin/python3', '-u', '.../test-foo.py', ...]
-D-     test took X.XX seconds
```

### Expected log patterns (pytest tests)

```
-I- --------------------------------------------------------------------------------
-I- Test: unit-tests/live/frames/pytest-foo.py::test_bar[D455-<SN>]
-I- --------------------------------------------------------------------------------
-D- Test using parametrized device: <SN>
-I- Configuration: D455 [<SN>]
-D- Recycling device via hub...
-D- enabling ports [4] disabling currently enabled ports [8]
-D- Device enabled and ready
-D- Test using device: Intel RealSense D455
-I- <test output>
-D- Test execution took X.XXXs
```

### Red flags to investigate

- **Wrong device in test**: Log says "D585S" test but error mentions D435 features → port management bug, wrong device was enabled
- **Missing device setup logs**: No "Enabling ports" / "Disabling ports" lines → port management may be wrong
- **"no device matches configuration"**: Device wasn't detected at all — could be port not enabled, or device genuinely missing
- **Test log cuts off without result**: Exception killed the test before it could log the outcome — check for `-E-` error lines
- **`ERROR` vs `FAILED` in pytest**: `ERROR` = fixture/setup failure, `FAILED` = test assertion failure. Both should appear in the report with clickable links.

### Comparing with baseline

Always compare failing tests against a known-good build before dismissing failures:
```bash
# Get the pytest log from a baseline build
curl -s -k -u "..." "https://rsjenkins.realsenseai.com/job/<job>/<baseline>/artifact/pytest-unit-tests.<N>.log" | grep "ERROR\|FAILED"
```

If a test passed in the baseline but fails in your build, your changes caused it — investigate.
