# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Logging setup: build dir detection, per-test log dirs, and rspy.log bridging."""

import logging
import os
import re
import sys

from rspy import repo, log as rspy_log

log = logging.getLogger('librealsense')

# Shared format for both the per-test FileHandler and the -s live CLI handler.
# Leading timestamp is local wall-clock HH:MM:SS.mmm — date is implicit (one log
# per test run). %03.0f rounds msecs; %03d would truncate (999.9 → 999, not 1000).
# If cross-timezone correlation is ever needed, set Formatter.converter = time.gmtime.
_LOG_FORMAT = '%(asctime)s.%(msecs)03.0f -%(levelname).1s- %(message)s'
_LOG_DATEFMT = '%H:%M:%S'


class _NestedFormatter( logging.Formatter ):
    """Prepend a logger's `nested` tag (e.g. '[C  ] ') to each line, mirroring the legacy
    rspy.log line prefix (rspy/log.py). A module sets `log.nested = 'C  '` on its own
    logger to tag its (client) lines; read here per-record so it stays per-module."""
    def format( self, record ):
        line = super().format( record )
        tag = getattr( logging.getLogger( record.name ), 'nested', None )
        return f'[{tag}] {line}' if tag else line

# unit-tests/ directory — used as fallback for log output
_unit_tests_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# Walk two levels up from rspy/pytest/ to get unit-tests/py/, then one more for unit-tests/
_unit_tests_dir = os.path.normpath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))

# Live-logging state: set to True when -s is passed (stdout not captured)
live_logging = False

# Per-(module, camera) log paths already opened this session. The first open truncates
# (clears any stale file from a previous run); reopens append, so pytest-retry attempts and
# --repeat/--count passes all accumulate in one file instead of overwriting each other.
_opened_logs = set()

def bridge_rspy_log():
    """Wrap rspy.log.d/i/w/e to also emit via Python logging."""
    def _wrap(original_fn, py_level):
        def wrapper(*args):
            result = original_fn(*args)
            msg = ' '.join(str(a) for a in args)
            log.log(py_level, msg)
            return result
        return wrapper

    # d() is dynamically redefined by debug_on(), so wrap whatever is current
    rspy_log.d = _wrap(rspy_log.d, logging.DEBUG)
    rspy_log.i = _wrap(rspy_log.i, logging.INFO)
    rspy_log.w = _wrap(rspy_log.w, logging.WARNING)
    rspy_log.e = _wrap(rspy_log.e, logging.ERROR)


def _find_build_dir():
    """Walk up from unit-tests/ to find the CMake build dir (contains CMakeCache.txt)."""
    search_dir = _unit_tests_dir
    while True:
        cmake_cache = os.path.join(search_dir, 'CMakeCache.txt')
        if os.path.isfile(cmake_cache):
            log.debug(f'Found build dir: {search_dir}')
            return search_dir
        parent = os.path.dirname(search_dir)
        if parent == search_dir:
            if repo.build:
                log.debug(f'Using repo.build: {repo.build}')
                return repo.build
            break
        search_dir = parent

    log.debug('Could not find build directory, using default')
    return None


def setup_test_logging(config):
    """Set up per-test log directory and JUnit XML output path (<build_dir>/<config>/unit-tests/)."""
    build_dir = _find_build_dir()

    if build_dir:
        cmake_cache_path = os.path.join(build_dir, 'CMakeCache.txt')
        configuration = None

        try:
            with open(cmake_cache_path, 'r') as f:
                for line in f:
                    if line.startswith('CMAKE_BUILD_TYPE:'):
                        parts = line.split('=', 1)
                        if len(parts) == 2:
                            configuration = parts[1].strip()
                            log.debug(f'Found CMAKE_BUILD_TYPE: {configuration}')
                            break
        except Exception as e:
            log.debug(f'Could not read CMAKE_BUILD_TYPE from CMakeCache.txt: {e}')

        if configuration:
            logdir = os.path.join(build_dir, configuration, 'unit-tests')
        else:
            logdir = os.path.join(build_dir, 'unit-tests')
    else:
        logdir = os.path.join(_unit_tests_dir, 'logs')

    os.makedirs(logdir, exist_ok=True)
    log.debug(f'Test logs directory: {logdir}')

    if not config.getoption('--junitxml', default=None):
        junit_xml_path = os.path.join(logdir, 'pytest-results.xml')
        config.option.xmlpath = junit_xml_path
        log.info(f'JUnit XML results: {junit_xml_path}')

    config._test_logdir = logdir


def configure_junit_logging(config):
    """Embed every test's captured log into its JUnit <system-out> so the Jenkins Test
    Result page shows the log inline (for passing tests too), without the JUnit
    Attachments plugin.

    Must run after the junitxml plugin has created its LogXML (we set xmlpath in
    setup_test_logging, which runs earlier in pytest_configure), so call from
    pytest_sessionstart.
    """
    if not getattr(config.option, 'xmlpath', None):
        return
    try:
        from _pytest.junitxml import LogXML
    except ImportError as e:
        log.debug(f'Could not import LogXML to configure JUnit logging: {e}')
        return
    for plugin in config.pluginmanager.get_plugins():
        if isinstance(plugin, LogXML):
            plugin.logging = 'all'           # capture stdout/stderr + log records into the XML
            plugin.log_passing_tests = True  # ...for every test, including passing ones
            log.debug('JUnit: tests will embed their log in <system-out>')
            return
    log.warning('JUnit: xmlpath is set but no LogXML plugin instance was found -- '
                'test logs will NOT be embedded in the XML.')


def configure_logging(config, debug_requested):
    """Configure root logger level, live logging, and suppress noisy loggers.

    Called from pytest_configure. Sets up log_cli format when -s is passed,
    and suppresses paramiko debug noise when --debug is active.
    """
    global live_logging

    if not debug_requested:
        log_cli_level = config.getoption('--log-cli-level', default=None)
        if log_cli_level and log_cli_level.upper() == 'DEBUG':
            debug_requested = True
    log_level_name = 'DEBUG' if debug_requested else 'INFO'
    logging.getLogger().setLevel(getattr(logging, log_level_name))
    capture = config.getoption('capture', default='fd')
    if capture == 'no':  # -s passed: stream logs to console
        live_logging = True
        config.option.log_cli_level = log_level_name
        config.option.log_cli_format = _LOG_FORMAT
        config.option.log_cli_date_format = _LOG_DATEFMT
    if debug_requested:
        logging.getLogger('paramiko').setLevel(logging.WARNING)


def _log_key(item):
    """Return (fspath, device_id) for grouping tests into one log file per module+device."""
    if item is None:
        return None
    device_id = None
    match = re.search(r'\[(.+)\]', item.name)
    if match:
        device_id = match.group(1)
    return (str(item.fspath), device_id)




def print_terminal_summary(terminalreporter):
    """Print pass/fail/skip summary for Jenkins Groovy parsing.

    Uses both log.info (captured in log files) and print() (always on stdout)
    so Jenkins Groovy can parse the '-I-' prefixed lines from the tee'd console log.
    """
    ensure_newline()

    passed = len(terminalreporter.stats.get('passed', []))
    failed = len(terminalreporter.stats.get('failed', []))
    skipped = len(terminalreporter.stats.get('skipped', []))
    error = len(terminalreporter.stats.get('error', []))
    # Merge setup/teardown errors into failures — they represent tests that did not pass
    # (e.g. device not visible after hub reset).
    failed += error
    total = passed + failed + skipped

    def summary(msg):
        log.info(msg)
        print(f"-I- {msg}")

    summary("")
    summary("=" * 80)
    summary("Test Summary")
    summary("=" * 80)
    summary(f"Total tests run: {total}")
    if passed > 0:
        summary(f"Passed: {passed}")
    if failed > 0:
        summary(f"Failed: {failed}")
    if skipped > 0:
        summary(f"Skipped: {skipped}")
    summary("=" * 80)


def ensure_newline():
    """Pytest's progress dots (F/.) don't end with newline — force one before our log output.
    Only needed when live logging is active (-s), otherwise it breaks -v formatting."""
    if live_logging:
        sys.stdout.write('\n')
        sys.stdout.flush()


def _compose_log_name(file_path, device_id):
    """Build the log filename from a module file path + optional device param id.

    Mirrors the legacy run-unit-tests.py naming: directory components (relative to
    unit-tests/) are joined with '-' and prepended to the file's short name.

    Examples:
      ('live/frames/pytest-t2ff-pipeline.py', 'D455-104623060005') -> 'pytest-live-frames-t2ff-pipeline_D455-104623060005.log'
      ('live/frames/pytest-t2ff-pipeline.py', None)                -> 'pytest-live-frames-t2ff-pipeline.log'
      ('pytest-standalone.py', None)                                -> 'pytest-standalone.log'
    """
    # Resolve relative path within the unit-tests tree
    normalized = file_path.replace(os.sep, '/')
    marker = 'unit-tests/'
    idx = normalized.rfind(marker)
    if idx >= 0:
        rel_path = normalized[idx + len(marker):]
    elif os.path.isabs(file_path) or normalized.startswith('/'):
        # Absolute path outside the unit-tests tree — use basename only to avoid
        # embedding host filesystem paths in the log filename.
        rel_path = os.path.basename(normalized)
    else:
        # Relative path with no unit-tests/ marker — only hit by infra test
        # mocks that pass paths like "live/frames/pytest-depth.py" directly.
        rel_path = normalized

    dirname = os.path.dirname(rel_path)
    basename = os.path.splitext(os.path.basename(rel_path))[0]

    if dirname:
        # conftest sets python_files=pytest-*.py, so basename always starts with 'pytest-'.
        # Strip the prefix, then rebuild as 'pytest-{dirs}-{short_name}'.
        dir_parts = dirname.replace('/', '-')
        basename = f"pytest-{dir_parts}-{basename[len('pytest-'):]}"

    log_name = f"{basename}_{device_id}" if device_id else basename
    log_name = re.sub(r'[<>:"/\\|?*]', '_', log_name)
    return log_name + ".log"


def test_log_name(item):
    """Derive log filename from a test item (directory path + file basename + device param)."""
    match = re.search(r'\[(.+)\]', item.name)
    device_id = match.group(1) if match else None
    return _compose_log_name(str(item.fspath), device_id)


def open_log(file_path, device_id, config):
    """Open a per-(module, device) FileHandler on the root logger and return it (or None when
    logging is off -- ``-s`` capture mode or no log dir configured).

    The caller is the module-scoped log fixture, which closes this via ``close_log`` at module
    teardown. Because the handler is owned by the module+device fixture lifecycle (not the per-test
    protocol hook), the *whole* module+camera run -- device enable at setup, every test, device
    disable at teardown -- lands in one file. This is what keeps a parametrized module fixture's
    deferred teardown (pytest runs the previous param's teardown during the next param's protocol)
    from leaking the disable into the next camera's log file.
    """
    logdir = getattr(config, '_test_logdir', None)
    capture = config.getoption('capture', default='fd')
    if not logdir or capture == 'no':
        return None
    log_path = os.path.join(logdir, _compose_log_name(file_path, device_id))
    try:
        # First open this session truncates (clears a stale file from a previous run); reopens
        # append, so pytest-retry attempts and --repeat/--count passes of the same (module, device)
        # all accumulate in ONE file -- each attempt's setup, test output, failure, and teardown --
        # instead of the latest attempt overwriting the previous ones.
        mode = 'a' if log_path in _opened_logs else 'w'
        handler = logging.FileHandler(log_path, mode=mode)
        handler.setFormatter(_NestedFormatter(_LOG_FORMAT, datefmt=_LOG_DATEFMT))
        handler.setLevel(logging.DEBUG)
        logging.getLogger().addHandler(handler)
        _opened_logs.add(log_path)
        return handler
    except Exception as e:
        log.warning(f"Could not create test log file {log_path}: {e}")
        return None


def close_log(handler):
    """Detach and close a handler returned by open_log (no-op if None)."""
    if handler is None:
        return
    logging.getLogger().removeHandler(handler)
    handler.close()
