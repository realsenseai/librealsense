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
import logging

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
    start_test_log, stop_test_log, print_terminal_summary,
)
from rspy.pytest.log_live_format import install as install_live_log_format
from rspy.pytest.cli import consume_legacy_flags, apply_pending_flags
from rspy.pytest.device_helpers import find_matching_devices, find_matching_devices_multi, resolve_device_each_serials
from rspy.pytest.collection import filter_and_sort_items
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
        help="Repeat each test N times (alias for pytest-repeat's --count)."
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

    # --repeat N → pytest-repeat's --count N (only if --count wasn't explicitly set)
    repeat_val = config.getoption('repeat_count', default=0)
    if repeat_val and config.getoption('count', default=1) <= 1:
        config.option.count = repeat_val

    # Parse and store context
    context_str = config.getoption("--context", default="")
    if context_str:
        context_list = context_str.split()
        log.info(f"Test context: {context_list}")

    # Set up test log directory
    setup_test_logging(config)

    # Enable LibRS debug logging if --rslog (once, globally)
    if rs and config.getoption("--rslog", default=False):
        rs.log_to_console(rs.log_severity.debug)

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
    # pytest-retry's verbose report is also suppressed (details are in per-test log files).
    if config.getoption("--tb") == "auto":
        config.option.tbstyle = "no"
    config.option.reportchars = "fE"
    try:
        from pytest_retry.retry_plugin import retry_manager
        retry_manager.build_retry_report = lambda *args, **kwargs: None
    except ImportError:
        pass

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

    # Query devices early for test parametrization
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


def pytest_collection_modifyitems(config, items):
    """Auto-skip nightly/dds tests, filter --live, sort by priority."""
    filter_and_sort_items(config, items)


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_protocol(item, nextitem):
    """Wrap each test with log separators and write per-test log file."""
    file_handler = start_test_log(item)
    ensure_newline()
    log.info("-" * 80)
    log.info(f"Test: {item.nodeid}")
    log.info("-" * 80)

    outcome = yield
    stop_test_log(file_handler, nextitem)

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
    if report.failed and call.excinfo:
        ensure_newline()
        log.error(f"{call.when} {report.outcome}: {call.excinfo.typename}: {call.excinfo.value}")
    if call.when == "call":
        ensure_newline()
        log.debug(f"Test execution took {report.duration:.3f}s")


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
def session_setup_teardown():
    """Runs once per session: log startup info, yield, then clean up hub/devices on exit."""
    # Setup — runs once before the first test
    register_signal_handlers(_cleanup_devices)

    yield  # All tests run here

    # Teardown — runs once after the last test
    ensure_newline()
    log.info("")
    log.info("=" * 80)
    log.info("Pytest Session Ending")
    log.info("=" * 80)

    try:
        _cleanup_devices()
    except Exception as e:
        log.warning(f"Error during cleanup: {e}")

    log.info("=" * 80)


# ============================================================================
# Device Fixtures
# ============================================================================

@pytest.fixture
def _test_device_serial(request):
    """Receives the device serial injected by pytest_generate_tests parametrization."""
    return request.param


@pytest.fixture(scope="function")
def module_device_setup(request):
    """Enable the target device via the hub. Recycles (power-cycles) once per test file, not per test case."""
    serial_number = None

    # Check parametrized serial from device_each
    if hasattr(request.node, 'callspec') and '_test_device_serial' in request.node.callspec.params:
        serial_number = request.node.callspec.params['_test_device_serial']
        log.debug(f"Test using parametrized device: {serial_number}")
    else:
        # Fall back to marker-based detection (for device() marker)
        device_markers = []
        for marker in request.node.iter_markers():
            if marker.name in ['device', 'device_each', 'device_exclude']:
                device_markers.append(marker)

        if not device_markers:
            log.debug(f"Test {request.node.name} has no device requirements")
            yield None
            return

        # Check for multi-device marker: device("D400*", "D400*") has multiple args
        multi_device_marker = next(
            (m for m in device_markers if m.name == 'device' and len(m.args) > 1), None
        )

        if multi_device_marker:
            # Multi-device path: resolve each spec to a unique device
            serial_numbers, had_candidates = find_matching_devices_multi(device_markers,
                                                  cli_includes=request.config.getoption("--device", default=[]),
                                                  cli_excludes=request.config.getoption("--exclude-device", default=[]))
            expected_count = len(multi_device_marker.args)
            if len(serial_numbers) < expected_count:
                pytest.fail(f"Need {expected_count} devices but only {len(serial_numbers)} found")

            # Enable all matched devices, recycle, and yield the list of SNs
            names = [f"{devices.get(sn).name} [{sn}]" for sn in serial_numbers]
            log.info(f"Configuration: {', '.join(names)}")
            try:
                devices.enable_only(serial_numbers, recycle=True)
                log.debug(f"All {len(serial_numbers)} devices enabled and ready")
            except Exception as e:
                pytest.fail(f"Failed to enable devices: {e}")
            yield serial_numbers
            return

        serial_numbers, had_candidates = find_matching_devices(device_markers, each=False,
                                                  cli_includes=request.config.getoption("--device", default=[]),
                                                  cli_excludes=request.config.getoption("--exclude-device", default=[]))

        if not serial_numbers:
            has_required = any(m.name == 'device' for m in device_markers)
            if had_candidates:
                pytest.skip("All matching devices were excluded")
            elif has_required:
                pytest.fail("No devices found matching requirements")
            else:
                pytest.skip("No devices found matching requirements")

        serial_number = serial_numbers[0]
        log.debug(f"Test will use first matching device: {serial_number}")

    # Enable only this device; recycle only once per module (like run-unit-tests.py),
    # but also recycle on retries and repeats (--repeat / --count).
    device = devices.get(serial_number)
    device_name = device.name if device else serial_number
    log.info(f"Configuration: {device_name} [{serial_number}]")

    module = request.node.module
    nodeid = request.node.nodeid
    no_reset = request.config.getoption("--no-reset", default=False)
    last_device = getattr(module, '_last_device_serial', None)
    last_test = getattr(module, '_last_test_nodeid', None)
    is_retry = (last_test == nodeid)
    is_repeat = request.config.getoption("count", default=1) > 1
    device_changed = (last_device is not None and last_device != serial_number)
    first_setup = (last_device is None)
    recycle = not no_reset and (first_setup or device_changed or is_retry or is_repeat)

    if not recycle and not first_setup:
        log.debug(f"Device {serial_number} already enabled, skipping hub setup")
        module._last_test_nodeid = nodeid
        yield serial_number
        return

    try:
        log.debug(f"{'Recycling' if recycle else 'Enabling'} device via hub...")
        devices.enable_only([serial_number], recycle=recycle)
        module._last_device_serial = serial_number
        module._last_test_nodeid = nodeid
        log.debug(f"Device enabled and ready")
    except Exception as e:
        pytest.fail(f"Failed to enable device {serial_number}: {e}")

    yield serial_number


@pytest.fixture
def test_context(request, module_device_setup):
    """Create a fresh rs.context() for the test. Depends on module_device_setup for hub state."""
    if not rs:
        pytest.skip("pyrealsense2 not available")

    ctx = rs.context()

    if module_device_setup and len(list(ctx.devices)) == 0:
        pytest.fail("No devices visible in context after device setup")

    return ctx


@pytest.fixture
def test_device(test_context):
    """Return (device, context) for the first visible device, or fail if none found."""
    devices_list = list(test_context.devices)
    if not devices_list:
        pytest.fail("No device available for test")

    dev = devices_list[0]
    log.debug(f"Test using device: {dev.get_info(rs.camera_info.name) if dev.supports(rs.camera_info.name) else 'Unknown'}")

    return dev, test_context


@pytest.fixture
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
def _safety_mode_state():
    """Module-scoped state holder for D585S service mode, keyed by device serial number.

    Each serial gets its own entry so that multiple D585S cameras in the same module
    (e.g. via device_each) are each entered into service mode independently.
    Teardown runs once at module end, restoring run mode for every camera that was entered.
    """
    state = {}  # serial_number -> {'sensor': ..., 'entered': False}
    yield state
    for sn, entry in state.items():
        if entry['entered'] and entry['sensor'] is not None:
            try:
                entry['sensor'].set_option(rs.option.safety_mode, rs.safety_mode.run)
            except Exception as e:
                # Don't throw on cleanup failure, to not mask test failures and also after test device is usually reset.
                log.error(f"safety_mode restore failed for {sn}: {e}")


@pytest.fixture
def test_device_wrapped(test_device, _safety_mode_state):
    """Like test_device, but puts D585S into service mode once per module per device.

    Many option-setting operations on D585S require service mode. No-op for all other
    device families. Service mode is entered on the first test in the module that uses
    this fixture for a given serial, and restored once at module teardown — not toggled
    per test case. Multiple D585S cameras are each tracked independently by serial.
    """
    dev, ctx = test_device
    is_d585s = dev.supports(rs.camera_info.name) and "D585S" in dev.get_info(rs.camera_info.name)
    if is_d585s:
        sn = dev.get_info(rs.camera_info.serial_number)
        if sn not in _safety_mode_state:
            _safety_mode_state[sn] = {'sensor': None, 'entered': False}
        entry = _safety_mode_state[sn]
        if not entry['entered']:
            safety_sensor = dev.first_safety_sensor()
            if safety_sensor.get_option(rs.option.safety_mode) != rs.safety_mode.service:
                # Will throw on failure — intentional so we fail the test rather than run without service mode.
                safety_sensor.set_option(rs.option.safety_mode, rs.safety_mode.service)
            entry['sensor'] = safety_sensor
            entry['entered'] = True
    yield dev, ctx
