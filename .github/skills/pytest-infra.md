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
| `rspy/pytest/plugins.py` | Required pytest plugin registry + availability check |
| `rspy/devices.py` | Device discovery, port tracking, `enable_only()` |
| `rspy/combined_hub.py` | Virtual port mapping across multiple hubs |
| `rspy/device_hub.py` | Abstract hub interface |
| `rspy/unifi.py` | UniFi PoE switch control via SSH |
| `rspy/acroname.py` | Acroname USB hub control |

### Adding a new pytest plugin requirement

When adding a pytest plugin to `unit-tests/requirements.txt`, you **must** also register it in `rspy/pytest/plugins.py` under `REQUIRED_PYTEST_PLUGINS`. `conftest.py` calls `check_required_plugins()` at `pytest_configure` time and raises `pytest.UsageError` if any listed plugin is not importable.

Why: a missing plugin silently disables its CLI options (e.g. if `pytest-repeat` is absent, `--repeat N` → `--count N` mapping becomes a no-op and tests run once with no error). The registry makes the dependency explicit and fails loudly.

To add one:
1. Pin the plugin in `unit-tests/requirements.txt`.
2. Add a `'<module_name>': '<pip-package-name>'` entry to `REQUIRED_PYTEST_PLUGINS` in `rspy/pytest/plugins.py` (module name is the Python import name, typically with underscores; pip name uses hyphens).

### Hub port management

- CI machines may have multiple hubs (e.g., Acroname + UniFi) wrapped in a `CombinedHub`
- `CombinedHub` assigns **virtual port numbers** sequentially: Acroname ports first (0-7), then UniFi ports (8-11)
- `devices.py` `enable_only()` derives currently enabled ports from `enabled()` + port mapping each call — no global cache

### Log file naming

Python (`logging_setup.py:test_log_name()`):
- `live/frames/pytest-t2ff-pipeline.py::test_x[D455-SN]` → `pytest-live-frames-t2ff-pipeline_D455-SN.log`
- `live/frames/pytest-t2ff-pipeline.py::test_x` → `pytest-live-frames-t2ff-pipeline.log`
- `live/hw-reset/pytest-sanity.py::test_x[D455-SN]` → `pytest-live-hw-reset-sanity_D455-SN.log`

The filename uses **directory path (relative to unit-tests/) + file short name + device param from brackets** — mirroring the legacy `run-unit-tests.py` naming convention. Never includes the test function name.

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
   - `#test:retries N` → `@pytest.mark.flaky(retries=N)` (**not** `pytest.mark.retries(N)` — that marker does not exist in pytest-retry and silently becomes a no-op with a PytestUnknownMarkWarning)
   - `#test:timeout` → check if pytest has a native equivalent
   - `#test:platform` → `@pytest.mark.skipif(platform...)`
   - `#test:flag` → check for `pytest.mark` equivalent
   - If no match exists, ask the user before proceeding

4. **Tests under `unit-tests/dds/` need an explicit `@pytest.mark.dds`**: `run-unit-tests.py` auto-tags every test by its parent directories (`unit-tests/py/rspy/libci.py:derive_tags_from_path`), so anything under `unit-tests/dds/` got the `dds` tag for free. Pytest doesn't auto-derive tags from path, so when migrating a test out of that directory you must add the marker explicitly:
   ```python
   pytestmark = [pytest.mark.dds, ...]   # alongside any device_each markers
   ```
   Also register the marker once in `conftest.py:pytest_configure` (`config.addinivalue_line("markers", "dds: ...")`) to silence `PytestUnknownMarkWarning`. Without this, the GHA Linux DDS jobs and Jenkins runs that pass `--tag dds` will silently skip the migrated test.

   **First-migration cleanup**: `unit-tests/dds/pytest-dds-placeholder.py` exists only so that `--tag dds` collects a non-empty set in the meantime (otherwise pytest exits with rc=5 and the GHA job fails). **Delete that placeholder file in the same PR as the first real dds test migration.**

5. **Replace `rspy.test` and `rspy.log`**: Convert `test.check*` to `assert`/`pytest_check.check.*` (see table below), drop `test.start`/`finish`/`print_results_and_exit`/`unexpected_exception`, and swap `from rspy import log` → `import logging; log = logging.getLogger(__name__)` (`log.d/i/w/e` → `log.debug/info/warning/error`; note stdlib `logging` needs `%s` format strings, not space-joined args). Apply to any helper modules too.

6. **Rename colliding helper modules**: Pytest collects everything in one process, so two directory-local `sw.py` (or similarly named) helpers will clash in `sys.modules`. Rename to unique names (e.g. `sw_device.py`, `sw_syncer.py`) and use `import sw_device as sw` to keep the body diff minimal. Verify by running `pytest` across both sibling dirs together.

7. **Prefer native pytest**: Native pytest features first → pytest plugins → custom implementation (last resort)

8. **No `pytest.mark.live`**: The `--live` CLI flag filters based on `device`/`device_each` markers automatically. `pytest.mark.live` is redundant.

9. **Test locally**: Run the new pytest test locally and verify it passes before considering done. Use `pytest -v unit-tests/live/.../pytest-foo.py` (without `-s` if checking log files — `-s` disables file logging).

10. **Minimal diff**: Keep original function names, variable names, docstrings, and code order. Only change what's required for the migration (imports, assertions, fixtures, markers, globals→params). Don't rename variables for style, reorder functions, or rewrite docstrings. Migration PRs should show minimal diff to reduce review burden and risk.

11. **Flag bugs, don't silently fix them**: If you spot a real bug or latent issue in the legacy test while migrating (e.g. missing teardown, stale references, off-by-one), surface it to the user and let them decide whether to fix it in the migration PR or defer to a follow-up. The default is defer — bundling fixes into a migration PR makes reviews harder and bisects noisier.

12. **Common code snippets**: Common short code snippets can be replaced with convenience helper functions, e.g `rspy.snippets.is_dds_dev`.

13. **FW version gating — use `require_min_fw_version`**: When a test requires a minimum firmware version, do **not** write inline `fw_version = ... / if fw_version < ...: pytest.skip(...)` blocks in each function. Use the shared helper from `rspy.pytest.device_helpers` instead:

    ```python
    from rspy.pytest.device_helpers import require_min_fw_version
    import pyrsutils as rsutils

    def test_something(test_device):
        dev, _ = test_device
        # skip if fw < 5.15.0.0 (require fw >= 5.15.0.0):
        require_min_fw_version(dev, rsutils.version(5, 15, 0, 0), "FEATURE_NAME")

        # skip if fw <= 5.14.0.0 (require fw > 5.14.0.0, i.e. strictly greater):
        require_min_fw_version(dev, rsutils.version(5, 14, 0, 0), "FEATURE_NAME", inclusive=False)
    ```

    The helper caches the result per `(device serial, min_version, inclusive)` — the check runs at most once per device/version combination. On pass the result is cached and subsequent calls are no-ops. On fail `pytest.skip()` is raised (cache never written), so every test that calls it will also skip. It also handles devices that don't expose firmware version info.

14. **Don't wrap test bodies in `try`/`finally` to swallow failures**: Pytest natively reports any unhandled exception as a test failure — there is no need for the legacy `try: ... except: test.unexpected_exception()` pattern. When migrating, just **delete** the `try`/`except` wrapper and let pytest handle it.

    - **Cleanup belongs in a fixture**, not in `try/finally` inside the test body. Use `@pytest.fixture` with `yield`: code before `yield` is setup, code after `yield` runs even when the test fails (see the `_sw_session` autouse fixture in `unit-tests/syncer/pytest-ts-*.py` for the pattern).
    - **If you genuinely must wrap with `try`** (e.g. to attach context to the failure message), **always re-raise or fail explicitly** — never swallow:
      ```python
      try:
          do_something()
      except Exception as e:
          pytest.fail( f"context-specific message: {e}" )  # or: raise
      ```
      A bare `except: pass` or a `finally:` that does cleanup but doesn't re-raise the original exception turns a real failure into a silent pass.
    - **Expected exceptions** (legacy `try/except RuntimeError: test.check_exception(...)`) → migrate to `with pytest.raises(RuntimeError, match="..."):` (see `unit-tests/syncer/pytest-ts-same-fps.py:63` for the pattern).

## Handling `on_fail=test.ABORT`

The legacy framework supported `with test.closure('Name', on_fail=test.ABORT):` — if that closure failed, all subsequent closures were skipped. In pytest, use the **`pytest-dependency`** plugin (already in `requirements.txt` and `plugins.py`).

**Pattern**: mark the prerequisite test with `@pytest.mark.dependency(scope='module')`, and each dependent test with `@pytest.mark.dependency(scope='module', depends=["prerequisite_name"])`. If the prerequisite fails or is skipped, all dependents are automatically skipped.

```python
# Prerequisite test — asserts (hard fail if condition not met), registers as a dependency
@pytest.mark.dependency(scope='module')
def test_advanced_mode_support(test_device_wrapped):
    """Prerequisite: camera must be in advanced mode."""
    dev, ctx = test_device_wrapped
    assert rs.rs400_advanced_mode(dev).is_enabled()

# Dependent test — skipped automatically if test_advanced_mode_support failed/was skipped
@pytest.mark.dependency(scope='module', depends=["test_advanced_mode_support"])
def test_set_depth_control(test_device_wrapped):
    dev, ctx = test_device_wrapped
    ...
```

**`scope='module'`**: limits dependency resolution to the current test file, so identically-named tests in other files do not interfere.

**Parametrized tests**: when both the prerequisite and dependent tests share the same parametrization (e.g., `device_each`), `pytest-dependency` automatically matches per-parameter — `test_set_depth_control[D455-SN]` is only skipped if `test_advanced_mode_support[D455-SN]` specifically failed, not if a different device's run failed.

**Chain of ABORTs**: if a file has multiple `on_fail=test.ABORT` closures in sequence, list all prerequisite names in `depends=`:

```python
@pytest.mark.dependency(scope='module')
def test_advanced_mode_support(...):   # first ABORT
    assert ...

@pytest.mark.dependency(scope='module', depends=["test_advanced_mode_support"])
def test_visual_preset_support(...):   # second ABORT
    assert ...

# Everything after the second ABORT depends on both
@pytest.mark.dependency(scope='module', depends=["test_advanced_mode_support", "test_visual_preset_support"])
def test_set_depth_control(...):
    ...
```

## Assertions: `assert` vs `pytest-check`

The `pytest-check` plugin is available for soft assertions (non-stopping checks). Use it when the legacy test uses `test.check()` in a loop where execution should continue on failure — this matches the legacy behavior where `test.check()` recorded failures but didn't abort.

| Legacy (`rspy.test`) | Hard assert (stops) | Soft check (continues) |
|---|---|---|
| `test.check(expr)` | `assert expr` | `check.is_true(expr)` |
| `test.check_equal(a, b)` | `assert a == b` | `check.equal(a, b)` |
| `test.check_approx_abs(a, b, tol)` | `assert a == pytest.approx(b, abs=tol)` | `check.almost_equal(a, b, abs=tol)` |

**When to use which:**
- Use `assert` for fatal conditions (hardware failures, setup errors, preconditions)
- Use `check.*` from `pytest-check` when the legacy test uses `test.check()` inside loops or across multiple configurations, so all iterations run and all failures are reported

**Approximate-equality note:** Prefer `check.almost_equal(a, b, abs=tol)` (or `pytest.approx`) over `check.less_equal(abs(a - b), tol)`. Both work, but `almost_equal` is shorter, reads as "approximately equal", and produces a much better failure message (e.g. `0.00099 == 0.001 ± 1e-08`) instead of `0.00001 not <= 1e-08`.

Import: `from pytest_check import check`

## Writing a New Pytest Test

### Fixture chain

The fixtures form a dependency chain. Use the lowest-level fixture that gives you what you need:

```
module_device_setup          → yields serial_number (or list of SNs for multi-device)
    ↓                           Handles hub port management (enable/disable/recycle)
test_context                 → returns rs.context()
    ↓                           Depends on module_device_setup for hub state
test_device                  → returns (rs.device, rs.context)
                                Grabs the first visible device from context
test_devices                 → returns ([rs.device, ...], rs.context)
                                For multi-device tests (requires multi-arg device marker)
```

### Which fixture to use

| You need | Use this fixture | Example |
|---|---|---|
| A device + context | `test_device` | Most hardware tests: streaming, options, metadata |
| A device + context, D585S in service mode | `test_device_wrapped` | D585S option/preset tests — service mode entered once per module, restored at module teardown |
| Multiple devices + context | `test_devices` | Multi-device tests (use with `device("D400*", "D400*")`) |
| Just a context (custom queries) | `test_context` | Device enumeration, custom context settings |
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

# Multi-device: need 2 unique D400 devices (use with test_devices fixture)
pytestmark = [pytest.mark.device("D400*", "D400*")]

# Multi-device: need one D400 and one D500
pytestmark = [pytest.mark.device("D400*", "D500*")]

# Multi-device: need specific devices
pytestmark = [pytest.mark.device("D455", "D435")]
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

## Infra Regression Tests

The `unit-tests/infra-tests/` directory contains regression tests for the pytest infrastructure itself. **No cameras or pyrealsense2 required** — only the hardware layer is mocked. These tests run on GHA for every PR.

### Rules

- **Every change to `conftest.py` or `rspy/pytest/*.py` must pass these tests.** Run `cd unit-tests && python -m pytest infra-tests/ -v` before pushing.
- **Every new infra feature needs a corresponding test.** New marker? Add to `test_e2e_markers.py`. New CLI flag? Add to `test_e2e_cli_options.py`. Changed skip/fail logic? Add to `test_e2e_skip_fail.py`.

### File layout

| File | What it tests |
|---|---|
| `helpers.py` | Shared: fake device inventory, mock builders, `run_e2e()` subprocess runner |
| `e2e_conftest.py` | Mock conftest copied into subprocess temp dirs (mocks hardware, exec()s real conftest) |
| `test_collection.py` | `collection.py`: context gating, `--live` filtering, priority sorting, device grouping |
| `test_device_helpers.py` | `device_helpers.py`: pattern matching, wildcards, excludes, CLI filters |
| `test_cli.py` | `cli.py`: `-r`/`--regex` → `-k` translation |
| `test_log_naming.py` | `logging_setup.py`: per-test log file naming |
| `test_e2e_markers.py` | All custom markers registered without warnings |
| `test_e2e_collection.py` | Context gating, `--live`, priority ordering in a real subprocess |
| `test_e2e_device_each.py` | `@device_each` parametrization, excludes, CLI filters, test IDs |
| `test_e2e_skip_fail.py` | `@device` fails vs `@device_each` skips when no match |
| `test_e2e_cli_options.py` | All CLI flags accepted (`--device`, `--context`, `--live`, `--debug`, etc.) |
| `test_e2e_port_management.py` | `enable_only()` called with correct serials and recycle flag |

### How E2E tests work

The E2E tests use `subprocess.run([sys.executable, "-m", "pytest", ...])` — same Python, same packages, works on CI. Each test:

1. Copies `e2e_conftest.py` to a temp dir (mocks hardware, then `exec()`s the real `conftest.py`)
2. Writes an inline test file with specific markers/assertions
3. Runs pytest in the temp dir as a subprocess
4. Parses the output for pass/fail/skip counts and `enable_only()` call logs

This means the E2E tests exercise the **real** production code. If someone changes `module_device_setup`, `filter_and_sort_items`, `resolve_device_each_serials`, or any hook in `conftest.py` — these tests break.

### Comparing with baseline

Always compare failing tests against a known-good build before dismissing failures:
```bash
# Get the pytest log from a baseline build
curl -s -k -u "..." "https://rsjenkins.realsenseai.com/job/<job>/<baseline>/artifact/pytest-unit-tests.<N>.log" | grep "ERROR\|FAILED"
```

If a test passed in the baseline but fails in your build, your changes caused it — investigate.
