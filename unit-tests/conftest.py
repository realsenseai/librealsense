# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Pytest configuration and fixtures for RealSense unit tests.

This module provides the pytest infrastructure to replace the proprietary LibCI system.
It manages:
- Device hub control for power cycling
- Device selection based on markers
- Context filtering to ensure tests only see intended devices
- Session-scoped device management

Implementation is split across rspy.pytest sub-modules; this file keeps only the hooks
and fixtures that pytest requires in conftest.py for auto-discovery.
"""

import pytest
import sys
import os
import re
import logging

# Defense against ROS 2 launch.logging: when ROS is sourced, launch_testing's
# pytest entry-point transitively imports launch.logging, which installs a
# logger class whose __init__ forces propagate=False on every new logger.
# That stops pytest's live log handler (set up below) from ever seeing test
# logs. Reset the class and re-enable propagate on already-poisoned loggers.
# No-op on clean machines — only fires when a non-stdlib Logger class is in use.
if logging.getLoggerClass() is not logging.Logger:
    print("-W- non-default Logger class detected (likely ROS launch.logging): "
          "resetting class and restoring propagate")
    logging.setLoggerClass(logging.Logger)
    for _name, _lgr in list(logging.Logger.manager.loggerDict.items()):
        if isinstance(_lgr, logging.Logger) and type(_lgr) is not logging.Logger and not _lgr.propagate:
            _lgr.propagate = True

# unit-tests/py/ contains rspy — the shared helper library used by all RealSense tests
current_dir = os.path.dirname(os.path.abspath(__file__))
# pytest built-in: exclude infra-tests/e2e/ from collection (those are static test cases
# run in isolated subprocesses by the infra regression tests, not by the parent pytest)
collect_ignore = [os.path.join(current_dir, 'infra-tests', 'e2e')]
py_dir = os.path.join(current_dir, 'py')
if py_dir not in sys.path:
    sys.path.insert(0, py_dir)

# Consume --debug before any rspy imports (rspy.log also consumes it from sys.argv)
_debug_requested = '--debug' in sys.argv

# Make sure the freshly-built pyrealsense2/pyrealdds/pyrsutils win over any copy
# pip may have left in the user site (~/.local/...). Must run before any rspy import
# that may pull pyrealsense2 transitively.
from rspy import python_path
python_path.block_user_site_for({'pyrealsense2', 'pyrealdds', 'pyrsutils'})

from rspy import devices, repo
from rspy.signals import register_signal_handlers
from rspy.pytest.logging_setup import (
    setup_test_logging, bridge_rspy_log, ensure_newline, configure_logging,
    open_log, close_log, _compose_log_name, print_terminal_summary,
    configure_junit_logging,
)
from rspy.pytest.log_live_format import install as install_live_log_format
from rspy.pytest.cli import consume_legacy_flags, apply_pending_flags
from rspy.pytest.device_helpers import (
    resolve_device_each_serials,
    select_target_device,
    _MISSING_SENTINEL_PREFIX,
    _SKIP_SENTINEL_PREFIX,
)
from rspy.pytest.collection import filter_and_sort_items, assert_module_fixtures_are_per_camera
from rspy.pytest.plugins import check_required_plugins

log = logging.getLogger('librealsense')

# Bridge rspy.log → Python logging early, before any test output
bridge_rspy_log()

# Translate legacy CLI flags before pytest parses sys.argv
consume_legacy_flags()


# ============================================================================
# pyrealsense2 Import
# ============================================================================
# pyrealsense2 is built as part of the CMake build — repo.find_pyrs_dir() locates the .pyd/.so
pyrs_dir = repo.find_pyrs_dir()
if pyrs_dir and pyrs_dir not in sys.path:
    sys.path.insert(1, pyrs_dir)

# Forked children (rspy.test.remote) are a fresh interpreter that won't see our sys.path
# edits; export PYTHONPATH so they can import rspy / pyrealsense2 (cf. run-unit-tests.py).
existing = os.environ.get( "PYTHONPATH", "" )
os.environ["PYTHONPATH"] = py_dir + ( os.pathsep + existing if existing else "" )
if pyrs_dir:
    os.environ["PYTHONPATH"] += os.pathsep + pyrs_dir

try:
    import pyrealsense2 as rs
except ImportError:
    log.warning('No pyrealsense2 library available!')
    rs = None

try:
    import pyrsutils
except ImportError:
    log.warning('No pyrsutils library available!')
    pyrsutils = None

try:
    import pyrealdds  # noqa: F401 — caches the module so unit-tests/dds/pytest-*.py find it after pytest reshuffles sys.path
except ImportError:
    pass


# ============================================================================
# Pytest Hooks
# ============================================================================

def pytest_addoption(parser):
    """Register RealSense-specific CLI options (device filters, hub control, etc.)."""
    group = parser.getgroup('librealsense', 'RealSense unit test options')
    group.addoption(
        "--device",
        action="append",
        default=[],
        help="Include only devices matching pattern (e.g., --device D455). "
             "Can be used multiple times or with a space-separated value (--device 'D455 D435')."
    )
    group.addoption(
        "--exclude-device",
        action="append",
        default=[],
        help="Exclude devices matching pattern (e.g., --exclude-device D455). "
             "Can be used multiple times or with a space-separated value (--exclude-device 'D555 D585S')."
    )
    group.addoption(
        "--context",
        action="store",
        default="",
        help="Context for test configuration (e.g., --context \"nightly weekly\"). Space-separated list."
    )
    group.addoption(
        "--rslog",
        action="store_true",
        default=False,
        help="Enable LibRS debug logging (rs.log_to_console)."
    )
    group.addoption(
        "--no-reset",
        action="store_true",
        default=False,
        help="Don't recycle (power-cycle) devices between tests."
    )
    group.addoption(
        "--hub-reset",
        action="store_true",
        default=False,
        help="Reset the hub itself during initialization."
    )
    group.addoption(
        "--live",
        action="store_true",
        default=False,
        help="Only run tests that require a live device (have at least one device/device_each marker)."
    )
    group.addoption(
        "--not-live",
        action="store_true",
        default=False,
        help="Only run tests that don't require a live device (skip tests with device/device_each markers). "
             "Mutually exclusive with --live."
    )
    group.addoption(
        "--tag",
        action="store",
        default="",
        help="Run only tests with the given marker (alias for pytest's -m). "
             "Legacy run-unit-tests.py compatibility."
    )
    group.addoption(
        "--repeat",
        action="store",
        default=0,
        type=int,
        dest="repeat_count",
        help="Run all tests in each file N times (module-scoped alias for pytest-repeat's --count). Use --count for per-test repetition."
    )
    # --debug and -r/--regex conflict with pytest built-ins and are consumed before
    # pytest parses args. Document them here so they show up in --help:
    group.addoption(
        "--rs-help",
        action="store_true",
        default=False,
        help="Pre-parsed flags (no need for --rs-help): "
             "--debug (enable -D- debug logs), "
             "-r/--regex <pattern> (filter tests by name, maps to -k), "
             "--tag <name> (run only tests with marker, maps to -m), "
             "--retries N (retry failed tests N times)."
    )
    group.addoption(
        "--test-dir",
        action="append",
        default=[],
        help="Restrict pytest discovery to tests under this directory or file. "
             "May be repeated (e.g. `--test-dir live/image-quality --test-dir test-fw-update.py`). "
             "Matches run-unit-tests.py --test-dir for shared UNIT_TESTS_ARGS."
    )


# Shared context tags (e.g. "nightly", "weekly") — tests check this to adjust behavior
context_list = []


def pytest_configure(config):
    """Early setup: register markers, configure defaults, and query connected devices."""
    global context_list

    check_required_plugins()
    apply_pending_flags(config)

    if config.getoption("--live", default=False) and config.getoption("--not-live", default=False):
        raise pytest.UsageError("--live and --not-live are mutually exclusive")

    tag_value = config.getoption("--tag", default="")
    if tag_value and not config.option.markexpr:
        config.option.markexpr = tag_value

    # --repeat N → pytest-repeat's --count N + module scope (only if --count wasn't explicitly set).
    # Using --repeat (our alias) always runs the full file N times; use --count for per-test repetition.
    repeat_val = config.getoption('repeat_count', default=0)
    if repeat_val and config.getoption('count', default=1) <= 1:
        config.option.count = repeat_val
        config.option.repeat_scope = 'module'

    # --retries N is handled natively by the pytest-retry plugin: failed tests rerun
    # up to N times, and the plugin tears down + re-creates module/class-scoped
    # fixtures between attempts (pytest-retry's "preliminary teardown trick" -
    # see pytest_retry.retry_plugin in the version pinned by requirements.txt).
    # This gives us free device recycling and precondition re-apply.
    #
    # By default pytest-retry's `should_handle_retry` skips setup/teardown phase
    # failures.  We relax that to also retry setup-phase failures (call.when ==
    # "setup"), since those are the common case for transient hub/USB glitches
    # at fixture time — the retry loop already does the right thing (tears down
    # then re-runs setup + call), it just refuses to enter for setup failures.
    # Teardown still excluded — re-running teardown after a teardown failure
    # is brittle and matches pytest-retry's upstream stance.
    # Regression for Jenkins win #113344 (fixture-time ERRORs must trigger retry).
    # Version pinned by requirements.txt so upstream renames give a deterministic
    # ImportError rather than silent behaviour drift.
    try:
        from pytest_retry import retry_plugin
        def _retry_setup_too(call):
            if call.excinfo is None or call.excinfo.typename == "Skipped":
                return False
            return call.when in ("setup", "call")
        retry_plugin.should_handle_retry = _retry_setup_too
    except ImportError:
        pass

    # We override pytest-repeat's `__pytest_repeat_step_number` to module scope so
    # module-scoped fixtures (e.g. module_device_setup) can depend on it and re-instantiate
    # per repeat pass.  That override only makes sense in module-scope repetition.  If a
    # user runs `--count N` *directly* with the default function-scope, force module scope
    # to stay consistent with the override.  --repeat already sets this above.
    if config.getoption('count', default=1) > 1 and config.getoption('repeat_scope', default='function') == 'function':
        log.warning("--count > 1 with default function-scope conflicts with our module-scoped "
                    "__pytest_repeat_step_number override; forcing --repeat-scope=module. "
                    "Use --repeat N for the module-scoped alias.")
        config.option.repeat_scope = 'module'

    # Parse and store context
    context_str = config.getoption("--context", default="")
    if context_str:
        context_list = context_str.split()
        log.info(f"Test context: {context_list}")

    # Set up test log directory
    setup_test_logging(config)

    # Enable LibRS debug logging if --rslog (once, globally)
    # log_to_console writes directly to stderr from C++. Pytest's default fd-level
    # capture swallows it, so we downgrade to sys-level capture (Python only) which
    # lets C++ stderr through while still capturing Python stdout/stderr.
    if rs and config.getoption("--rslog", default=False):
        rs.log_to_console(rs.log_severity.debug)
        if config.option.capture == 'fd':
            config.option.capture = 'sys'

    # Test discovery defaults (replaces pytest.ini which is .gitignored)
    config.addinivalue_line("python_files", "pytest-*.py")
    config.addinivalue_line("python_classes", "Test*")
    config.addinivalue_line("python_functions", "test_*")

    # Default timeout: 200s, thread-based (Windows-compatible)
    if not config.getoption("--timeout", default=None):
        config.option.timeout = 200
        config.option.timeout_method = "thread"

    # Suppress verbose failure tracebacks — per-test log files have full details.
    # Keep short one-liners (-rfE) so Jenkins Groovy can parse them for log file links.
    if config.getoption("--tb") == "auto":
        config.option.tbstyle = "no"
    config.option.reportchars = "fE"

    # Suppress paramiko and cryptography deprecation warnings
    config.addinivalue_line("filterwarnings", "ignore::DeprecationWarning:cryptography")
    config.addinivalue_line("filterwarnings", "ignore::DeprecationWarning:paramiko")
    config.addinivalue_line("filterwarnings", "ignore:TripleDES has been moved")
    config.addinivalue_line("filterwarnings", "ignore:Blowfish has been moved")

    # Register custom markers
    config.addinivalue_line(
        "markers", "device(pattern): mark test to run on devices matching pattern (e.g., D400*, D455)"
    )
    config.addinivalue_line(
        "markers", "device_each(pattern): mark test to run on each device matching pattern separately"
    )
    config.addinivalue_line(
        "markers", "device_exclude(pattern): exclude devices matching pattern from test execution"
    )
    config.addinivalue_line(
        "markers", "live: tests requiring live devices"
    )
    config.addinivalue_line(
        "markers", "context(name): test only runs when name is in --context (e.g., nightly, weekly, dds)"
    )
    config.addinivalue_line(
        "markers", "priority(value): test execution priority (lower runs first, default 500)"
    )
    config.addinivalue_line(
        "markers", "device_type(type): run test only on devices with a matching connection type (e.g., GMSL, USB, DDS)"
    )
    config.addinivalue_line(
        "markers", "device_type_exclude(type): skip test if device connection type matches (e.g., GMSL, USB, DDS)"
    )
    config.addinivalue_line(
        "markers", "dds: test requires a DDS-enabled build (selected by --tag dds / -m dds)"
    )

    # Configure standard logging with format matching legacy rspy.log output
    configure_logging(config, _debug_requested)

    # Live-format LogRecord args so pytest's LogCaptureHandler (which retains
    # records for the test's captured-logs report) doesn't pin arg objects.
    # Critical for rs.frame args: the syncer's publish pool defaults to 16
    # slots, and retained rs.frame refs block pool reclamation -- see PR
    # #14962 investigation.
    install_live_log_format()

    # Log build environment info (printed directly — pytest log handlers aren't active yet)
    print(f"-I- {'=' * 80}")
    if rs:
        print(f"-I- Using pyrealsense2 from: {rs.__file__}")
    if repo.build:
        print(f"-I- Build directory: {repo.build}")
    print(f"-I- {'=' * 80}")

    # Create hub after logging is configured so discovery prints are visible
    devices.init_hub()

    # Echo CLI device filters once (' '.join handles both repeated-flag and space-separated forms)
    exclude_list = config.getoption("--exclude-device", default=[])
    if exclude_list:
        print(f"-D- excluding devices: {' '.join(exclude_list)}")
    include_list = config.getoption("--device", default=[])
    if include_list:
        print(f"-D- including only devices: {' '.join(include_list)}")

    # Skip under --not-live: nothing reads harness devices then, and the DDS context it
    # creates would otherwise pollute discovery for the forked DDS servers.
    if not config.getoption("--not-live", default=False):
        try:
            hub_reset = config.getoption("--hub-reset", default=False)
            enable_dds = 'dds' in context_list
            devices.query(hub_reset=hub_reset, disable_dds=not enable_dds)
            devices.map_unknown_ports()
        except Exception as e:
            log.warning(f"Failed to query devices during configuration: {e}")


def pytest_generate_tests(metafunc):
    """Expand @device_each into one test instance per matching device."""
    resolve_device_each_serials(metafunc)


def pytest_collection_modifyitems(session, config, items):
    """Auto-skip nightly/dds tests, filter --live, sort by priority."""
    assert_module_fixtures_are_per_camera(session, items)
    test_dirs = config.getoption("--test-dir", default=[])
    if test_dirs:
        abs_dirs = [os.path.abspath(p) for p in test_dirs]
        included = [item for item in items
                    if any(str(item.path).startswith(p) for p in abs_dirs)]
        config.hook.pytest_deselected(items=[item for item in items if item not in included])
        items[:] = included
    filter_and_sort_items(config, items)


def _emit_test_header(nodeid):
    """Write the ``Test: <nodeid>`` banner into the current module+camera log."""
    ensure_newline()
    log.info("-" * 80)
    log.info(f"Test: {nodeid}")
    log.info("-" * 80)


@pytest.fixture(autouse=True)
def _test_log_banner(request):
    """Emit the per-test header into the log. Runs during test setup -- i.e. AFTER the
    module-scoped log handler (module_log) is open and the device is enabled -- so the header
    lands in the correct module+camera file. (Logging it from a per-test protocol hook instead
    would race a parametrized module fixture's deferred teardown and land in the wrong file.)

    Setup-phase failures in module_device_setup happen BEFORE this fixture runs, so those paths
    emit the header themselves (via _emit_test_header) to keep the error anchored to its item."""
    _emit_test_header(request.node.nodeid)
    _record_log_alias(request)
    yield
    ensure_newline()


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item, call):
    """Log test duration and any failures/errors."""
    outcome = yield
    report = outcome.get_result()

    if report.skipped:
        ensure_newline()
        reason = report.longrepr[-1]
        log.info(reason)
    # Call-phase failures are logged from pytest_runtest_call so they also appear on
    # pytest-retry attempts (which bypass this hook); here we cover setup/teardown.
    if report.failed and call.excinfo and call.when != "call":
        ensure_newline()
        log.error(f"{call.when} {report.outcome}: {call.excinfo.typename}: {call.excinfo.value}")
    if call.when == "call":
        ensure_newline()
        log.debug(f"Test execution took {report.duration:.3f}s")


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_call(item):
    """Surface pytest-check soft-check failures in the call phase.

    pytest-check defers its failures to pytest_runtest_makereport. But pytest-retry
    reruns a test by invoking pytest_runtest_call directly and building the report
    with TestReport.from_item_and_call, which never fires makereport. So on a retry
    attempt the soft-check failures are invisible to the retry decision (the test
    looks passed) and instead surface later against the teardown phase, which
    pytest-retry refuses to retry. Net effect: a retried test is reported "passed"
    yet still fails the run with a teardown error.

    Flushing the failures here, in the call phase, makes them visible to pytest-retry
    on every attempt (a genuinely flaky soft-check test passes on retry; a persistent
    one stays failed) and keeps them off the teardown report. Scoped to fire only for
    the buggy case; every other path is left to pytest-check unchanged.
    """
    outcome = yield

    # Record a call-phase failure into the per-test log. pytest-retry reruns a test by
    # invoking pytest_runtest_call directly -- bypassing pytest_runtest_makereport -- and
    # reopens the module log in 'w' mode, which truncates the original attempt's logged
    # failure. Logging here means every attempt (including the last retry) records its
    # failure, so a failing test's .log never ends on just the "Test:" header.
    if outcome.excinfo is not None and outcome.excinfo[0].__name__ != "Skipped":
        ensure_newline()
        log.error(f"call failed: {outcome.excinfo[0].__name__}: {outcome.excinfo[1]}")

    try:
        from pytest_check import check_log
    except ImportError:
        return
    failures = check_log.get_failures()
    if not failures:
        return
    # Don't mask a real exception, and leave pytest-check's xfail handling to it.
    if outcome.excinfo is not None or item.get_closest_marker("xfail"):
        return
    num_failures = check_log._num_failures
    check_log.clear_failures()
    message = "\n".join(failures + ["-" * 60, f"Failed Checks: {num_failures}"])
    ensure_newline()
    log.error(f"call failed: {num_failures} soft-check failure(s):\n{message}")
    raise AssertionError(message)


def pytest_sessionstart(session):
    """Configure the junitxml plugin once it exists (after pytest_configure)."""
    configure_junit_logging(session.config)


def pytest_terminal_summary(terminalreporter, exitstatus, config):
    """Print pass/fail/skip summary for Jenkins Groovy parsing."""
    print_terminal_summary(terminalreporter)


# ============================================================================
# Session-Scoped Fixtures
# ============================================================================

def _cleanup_devices():
    """Release hub and rs.context — required so BrainStem threads don't prevent exit."""
    if devices.hub:
        try:
            if devices.hub.is_connected():
                log.debug("Cleanup: disconnecting from hub(s)")
                devices.hub.disable_ports()
                devices.wait_until_all_ports_disabled()
            devices.hub.disconnect()
        except Exception:
            pass
        devices.hub = None
    devices._context = None
    import gc
    gc.collect()  # Force release so BrainStem USB hub threads shut down


@pytest.fixture(scope="session", autouse=True)
def session_setup_teardown(request):
    """Runs once per session: register cleanup, yield, then clean up hub/devices on exit."""
    # Setup — runs once before the first test
    register_signal_handlers(_cleanup_devices)

    yield  # All tests run here

    # Teardown — runs once after the last test. The module-scoped log handler is already closed
    # (at the last module's teardown), so this output never tails a test's .log. Emit it with
    # pytest's output capture suspended and via print() so it reaches the console -- otherwise
    # fixture-teardown stdout is captured and discarded, and a logging.info would have no handler.
    def _emit_session_end():
        print(f"\n-I- {'=' * 80}")  # leading newline: pytest's last progress line has no EOL yet
        print("-I- Pytest Session Ending")
        print(f"-I- {'=' * 80}")
        try:
            _cleanup_devices()
        except Exception as e:
            print(f"-W- Error during cleanup: {e}")

    # getplugin returns None if capture is disabled (e.g. -p no:capture); guard so teardown
    # (and _cleanup_devices) still runs instead of raising AttributeError.
    capmanager = request.config.pluginmanager.getplugin("capturemanager")
    if capmanager is not None:
        with capmanager.global_and_fixture_disabled():
            _emit_session_end()
    else:
        _emit_session_end()

    log.info("=" * 80)


# ============================================================================
# Device Fixtures
# ============================================================================

@pytest.fixture(scope="module")
def _test_device_serial(request):
    """Receives the device-selection value injected by ``pytest_generate_tests``.

    Possible values (set in ``resolve_device_each_serials``):

    - ``None``                    — no parametrization (test has no device markers).
    - ``str`` (plain serial)      — single-spec ``device(...)`` or ``device_each(...)``
                                    resolved to one device.
    - ``str`` (sentinel)          — ``__SKIP__:<pattern>`` or ``__MISSING__:<pattern>``
                                    when the lab had no usable match.
    - ``list[str]``               — multi-spec ``device("A", "B")`` resolved to a list
                                    of unique serials.
    """
    return getattr(request, 'param', None)


@pytest.fixture(scope="module")
def __pytest_repeat_step_number(request):
    """Override pytest-repeat's function-scoped fixture so module-scoped
    consumers (e.g. module_device_setup) can depend on it and pytest re-instantiates
    them on each repeat pass — driving the device recycle between passes.

    Assumes ``repeat_scope='module'`` (forced in ``pytest_configure`` whenever
    ``--count`` / ``--repeat`` ask for >1 pass).  Without parametrize
    (--count==1) ``request.param`` is unset and we just return 0.

    Note: with native pytest-retry handling --retries, retries don't go through
    pytest-repeat — pytest-retry tears down module fixtures directly between
    attempts. This fixture is only relevant for --repeat / --count.
    """
    return getattr(request, 'param', 0)


def _device_log_id(serial):
    """Device-portion id used for the per-(module, camera) log filename.

    Mirrors the device parametrize ids built in ``resolve_device_each_serials`` (``<name>-<sn>``,
    ``+``-joined for multi-device, ``MISSING-``/``SKIP-`` for sentinels) -- and deliberately omits
    any extra ``@pytest.mark.parametrize`` dimensions (config/resolution). So every parametrize
    case of one camera shares ONE file (``<module>_<name>-<sn>.log``), with the device enable at
    the top and disable at the bottom, while each camera still gets its own file. ``None`` for a
    test with no device markers.
    """
    if serial is None:
        return None
    if isinstance(serial, list):
        return '+'.join(f"{devices.get(sn).name}-{sn}" if devices.get(sn) else sn for sn in serial)
    if serial.startswith(_MISSING_SENTINEL_PREFIX):
        return f"MISSING-{serial[len(_MISSING_SENTINEL_PREFIX):]}"
    if serial.startswith(_SKIP_SENTINEL_PREFIX):
        return f"SKIP-{serial[len(_SKIP_SENTINEL_PREFIX):]}"
    dev = devices.get(serial)
    return f"{dev.name}-{serial}" if dev else serial


# Per-item log filenames to hardlink onto a camera's collapsed log, keyed by the camera log's
# absolute path. Lets Jenkins' per-case report links (which reconstruct <module>_<full-bracket>.log
# per item) resolve to the collapsed file WITHOUT any Jenkins/deploy-repo change.
_log_alias_registry = {}


def _per_item_log_name(fspath, item_name):
    """The legacy per-item log filename (full bracket id) Jenkins reconstructs for an item."""
    m = re.search(r'\[(.+)\]', item_name)
    return _compose_log_name(fspath, m.group(1) if m else None)


def _record_log_alias(request):
    """Record this item's per-item log filename so module_log teardown can link it to the camera's
    collapsed log. No-op for non-device tests or when the per-item name already equals the camera
    name (single-param modules -- the common case -- need no alias)."""
    logdir = getattr(request.config, '_test_logdir', None)
    cs = getattr(request.node, 'callspec', None)
    device_id = _device_log_id(cs.params.get('_test_device_serial') if cs else None)
    if not logdir or device_id is None:
        return
    fspath = str(request.node.fspath)
    camera_name = _compose_log_name(fspath, device_id)
    item_name = _per_item_log_name(fspath, request.node.name)
    if item_name != camera_name:
        _log_alias_registry.setdefault(os.path.join(logdir, camera_name), set()).add(item_name)


def _create_log_aliases(config, fspath, device_id):
    """Hardlink (copy fallback) each recorded per-item name to the camera's collapsed log, so a
    multi-param module's per-case Jenkins links all resolve to the one camera file."""
    logdir = getattr(config, '_test_logdir', None)
    if not logdir or device_id is None:
        return
    camera_path = os.path.join(logdir, _compose_log_name(fspath, device_id))
    names = _log_alias_registry.pop(camera_path, ())
    if not names or not os.path.exists(camera_path):
        return
    for item_name in names:
        alias = os.path.join(logdir, item_name)
        if alias == camera_path:
            continue
        try:
            if os.path.lexists(alias):
                os.remove(alias)          # retries re-create the alias; replace any stale one
            os.link(camera_path, alias)   # hardlink: no content copy, archived as a real file
        except OSError:
            try:
                import shutil
                shutil.copyfile(camera_path, alias)
            except OSError as e:
                log.warning(f"Could not create per-case log alias {alias}: {e}")


@pytest.fixture(scope="module", autouse=True)
def module_log(request, _test_device_serial):
    """Own the per-(module, camera) log file for the whole module lifecycle.

    Module-scoped so one file spans setup (device enable) -> every test -> teardown (device
    disable). Depends on ``_test_device_serial`` so pytest re-instantiates it per camera (one file
    per module+camera) and so it satisfies the cross-camera module-fixture guard. The filename uses
    the device-portion id only (``_device_log_id``), so all of a camera's ``@parametrize`` cases
    collapse into that camera's single file.

    ``module_device_setup`` depends on this fixture, so the handler opens before the device is
    enabled and closes after it is disabled -- keeping a parametrized module fixture's deferred
    teardown (pytest runs the previous camera's teardown during the next camera's protocol) from
    leaking the disable into the next camera's file.
    """
    device_id = _device_log_id(_test_device_serial)
    handler = open_log(str(request.node.fspath), device_id, request.config)
    try:
        yield
    finally:
        close_log(handler)
        # link any extra-param cases' per-item names to this camera's collapsed log (Jenkins links)
        _create_log_aliases(request.config, str(request.node.fspath), device_id)


@pytest.fixture(scope="module", autouse=True)
def module_device_setup(request, _test_device_serial, __pytest_repeat_step_number, module_log):
    """Power the target device(s) on for the module and off again at teardown — once per
    (module, parametrized value).

    Resolution (markers, CLI filters, missing/skip sentinels) happens in
    ``resolve_device_each_serials`` at collection time; this fixture just consumes
    ``_test_device_serial`` and owns the hub-port lifecycle:

    - ``None``            → test has no device markers; yield None.
    - ``list[str]``       → multi-device marker; enable all serials, yield the list, disable on teardown.
    - sentinel strings    → ``pytest.skip`` / ``pytest.fail``.
    - plain serial string → enable that device, yield it, disable on teardown.

    Port state is owned by this fixture's lifecycle — enable on setup, disable on teardown — so
    isolation and recycle fall out of pytest re-instantiating the fixture per
    (module, device, repeat-step); there is no global port tracking. Isolation in the default
    path comes from the *previous* module's teardown having powered its device off, so setup only
    needs to power on its own. With ``--no-reset`` the device is isolated without a power-cycle
    (``disable_other_ports=True``) and left on at teardown — matching the legacy fast path.

    ``autouse=True`` is REQUIRED, not just an optimization: some tests build their own
    ``rs.context()`` and never request ``test_device``/``test_context`` (e.g.
    ``live/streaming/pytest-jpeg-compressed-format.py``, ``live/d500/pytest-detect-D555.py``);
    they get their port powered only because this fixture runs automatically for every
    device-marked module.
    """
    serial_number = _test_device_serial

    if serial_number is None:
        log.debug(f"Module {request.node.name} has no device requirements")
        yield None
        return

    # nodeid of the item that triggered this module fixture -- used to anchor setup-phase failures
    # (which happen before _test_log_banner runs) to the failing test in the log.
    item_id = getattr(getattr(request, '_pyfuncitem', None), 'nodeid', None) or request.node.nodeid

    no_reset = request.config.getoption("--no-reset", default=False)
    # Decide whether setup power-cycles the device (vs just turning it on):
    #  --no-reset            -> never recycle; isolate statelessly and leave the device on.
    #  no hub (Jetson/MIPI)  -> recycle; teardown-disable is a no-op there, so enable_only(recycle=
    #                           True) falls back to hardware_reset() to clear prior state.
    #  hub, port already ON  -> recycle. The device should be OFF here (prev module's teardown
    #                           disabled it, or query()'s initial disable-all did). A powered port
    #                           means a teardown was skipped (crash/kill) and the device was left
    #                           in an unknown state -> power-cycle it clean. Self-heals the
    #                           within-session leak that the old recycle=True sweep used to catch.
    #  hub, port OFF         -> don't recycle; enabling it here IS the power-on (teardown-off +
    #                           setup-on = the cycle). Avoids re-disabling an already-off port.
    serials = serial_number if isinstance(serial_number, list) else [serial_number]
    if no_reset:
        recycle = False
    elif devices.hub is None:
        recycle = True
    else:
        recycle = devices.any_port_powered(serials)
    disable_other_ports = no_reset
    teardown_disable = not no_reset

    def _teardown(serials):
        if not teardown_disable:
            return
        try:
            devices.disable(serials)
        except Exception as e:
            log.warning(f"Failed to disable {serials} on teardown: {e}")

    if isinstance(serial_number, list):
        # Multi-device path: parametrized list of serials. Sentinels are always strings,
        # so the list form never carries skip/missing semantics.
        names = [
            f"{(devices.get(sn).name if devices.get(sn) else sn)} [{sn}]"
            for sn in serial_number
        ]
        log.info(f"Configuration: {', '.join(names)}")
        try:
            devices.enable_only(serial_number, recycle=recycle, disable_other_ports=disable_other_ports)
            log.debug(f"All {len(serial_number)} devices enabled and ready")
        except Exception as e:
            # Setup failed after possibly powering the port(s): teardown won't run (no yield),
            # so power off what we tried to enable here, lest it linger into the next module.
            _emit_test_header(item_id)
            try:
                devices.disable(serial_number)
            except Exception as cleanup_err:
                log.warning(f"Cleanup after failed enable raised: {cleanup_err}")
            pytest.fail(f"Failed to enable devices: {e}")
        yield serial_number
        _teardown(serial_number)
        return

    # Single-device path (parametrized string value, including sentinels).
    if serial_number.startswith(_SKIP_SENTINEL_PREFIX):
        pattern = serial_number[len(_SKIP_SENTINEL_PREFIX):]
        _emit_test_header(item_id)
        pytest.skip(f"No suitable devices for requirements: {pattern}")
    if serial_number.startswith(_MISSING_SENTINEL_PREFIX):
        pattern = serial_number[len(_MISSING_SENTINEL_PREFIX):]
        _emit_test_header(item_id)
        pytest.fail(f"No devices found matching requirements: {pattern}")
    log.debug(f"Test using parametrized device: {serial_number}")

    device = devices.get(serial_number)
    device_name = device.name if device else serial_number
    log.info(f"Configuration: {device_name} [{serial_number}]")

    try:
        log.debug(f"{'Recycling' if recycle else 'Enabling'} device...")
        devices.enable_only([serial_number], recycle=recycle, disable_other_ports=disable_other_ports)
        log.debug(f"Device enabled and ready")
    except Exception as e:
        # Setup failed after possibly powering the port: teardown won't run (no yield), so power
        # off what we tried to enable here, lest it linger into the next module.
        _emit_test_header(item_id)
        try:
            devices.disable([serial_number])
        except Exception as cleanup_err:
            log.warning(f"Cleanup after failed enable raised: {cleanup_err}")
        pytest.fail(f"Failed to enable device {serial_number}: {e}")

    yield serial_number
    _teardown([serial_number])


@pytest.fixture(scope="module")
def test_context(module_device_setup):
    """Create an rs.context() once per module/device. Depends on module_device_setup for hub state."""
    if not rs:
        pytest.skip("pyrealsense2 not available")

    ctx = rs.context({"device-mask":0xfe}) # Intel only (no platform camera when testing locally)

    if module_device_setup and len(list(ctx.devices)) == 0:
        pytest.fail("No devices visible in context after device setup")

    return ctx


@pytest.fixture(scope="module")
def test_device(test_context, module_device_setup):
    """Return (device, context) for the test's target device, or fail if none found."""
    devices_list = list(test_context.devices)
    if not devices_list:
        pytest.fail("No device available for test")

    dev = select_target_device(devices_list, module_device_setup)
    log.debug(f"Test using device: {dev.get_info(rs.camera_info.name) if dev.supports(rs.camera_info.name) else 'Unknown'}")

    return dev, test_context


@pytest.fixture
def function_scoped_device(test_context, module_device_setup):
    """Function-scoped: re-query the module-scoped ``test_context`` and return a
    *fresh* device wrapper for the test's target device.  Use this in tests that
    mutate persistent device state (e.g. HDR sequencer overrides in
    ``pytest-hdr-long.py``) and need each test to start from a new device object,
    even though the underlying ``rs.context()`` is shared across the module.

    Reuses the module-scoped context — no extra context construction cost.  Pair
    with ``test_context`` if the test also needs the context (e.g. to build an
    ``rs.pipeline(ctx)``).
    """
    devices_list = list(test_context.devices)
    if not devices_list:
        pytest.fail("No device available for test")

    dev = select_target_device(devices_list, module_device_setup)
    log.debug(f"Test using fresh device handle: {dev.get_info(rs.camera_info.name) if dev.supports(rs.camera_info.name) else 'Unknown'}")

    return dev


@pytest.fixture(scope="module")
def test_devices(test_context, module_device_setup):
    """Return (device_list, context) for multi-device tests.

    Used with device("D400*", "D400*") markers. module_device_setup enables the
    required hub ports; this fixture grabs the matching devices from the context.
    """
    if not isinstance(module_device_setup, list):
        pytest.fail("test_devices fixture requires a multi-device marker, e.g. @pytest.mark.device('D400*', 'D400*')")

    serial_numbers = module_device_setup
    device_list = []
    for sn in serial_numbers:
        for dev in test_context.devices:
            if dev.supports(rs.camera_info.serial_number) and dev.get_info(rs.camera_info.serial_number) == sn:
                device_list.append(dev)
                break

    if len(device_list) < len(serial_numbers):
        pytest.fail(f"Expected {len(serial_numbers)} devices in context but found {len(device_list)}")

    return device_list, test_context


@pytest.fixture
def test_context_var():
    """Expose the --context tags (e.g. ['nightly', 'weekly']) so tests can branch on them."""
    return context_list


@pytest.fixture(scope="module")
def test_device_wrapped(test_device):
    """Like test_device, but puts a D585S into service mode for this device's tests and restores
    run mode at teardown. No-op for other device families.

    Parametrized per device (via test_device), so enter and restore both run while this camera is
    the enabled one: service mode is restored at the device's own teardown, before the hub
    switches to the next camera (not deferred to module end via shared state).
    """
    dev, ctx = test_device
    # Read serial up front, while the device is still healthy: the restore path below runs in an
    # except block where the device may be disconnected, so get_info() there could throw too.
    sn = dev.get_info(rs.camera_info.serial_number) if dev.supports(rs.camera_info.serial_number) else "?"
    is_d585s = dev.supports(rs.camera_info.name) and "D585S" in dev.get_info(rs.camera_info.name)
    safety_sensor = None
    if is_d585s:
        from rspy import tests_wrapper  # local import: pulls in pyrealsense2, unavailable in infra-tests
        safety_sensor = dev.first_safety_sensor()
        if safety_sensor.get_option(rs.option.safety_mode) != rs.safety_mode.service:
            # Will throw on failure — intentional so we fail the test rather than run without service mode.
            # Retries internally: the FW needs a few seconds after enumeration before it accepts the switch.
            tests_wrapper.set_safety_mode(safety_sensor, rs.safety_mode.service)
    yield dev, ctx
    if safety_sensor is not None:
        try:
            # tests_wrapper already imported in the setup block above (still bound across the yield)
            tests_wrapper.set_safety_mode(safety_sensor, rs.safety_mode.run)
        except Exception as e:
            # Best-effort: don't mask test failures, and the device may already be reset by teardown time.
            log.warning(f"safety_mode restore skipped for {sn}: {e}")
