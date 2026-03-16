# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Logging setup: build dir detection, per-test log dirs, and rspy.log bridging."""

import logging
import os
import re
import sys

from rspy import repo, log as rspy_log

log = logging.getLogger('librealsense')

# unit-tests/ directory — used as fallback for log output
_unit_tests_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# Walk two levels up from rspy/pytest/ to get unit-tests/py/, then one more for unit-tests/
_unit_tests_dir = os.path.normpath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))

# Live-logging state: set to True when -s is passed (stdout not captured)
live_logging = False

# Per-module+device log handler tracking
_current_log_key = None       # (fspath, device_id) tuple
_current_file_handler = None


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
        config.option.log_cli_format = '-%(levelname).1s- %(message)s'
        config.option.log_cli_date_format = ''
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


def start_test_log(item):
    """Open a per-module+device FileHandler. Reuses the existing handler when the key
    (file + device param) hasn't changed, so all tests for the same module+device
    share a single .log file.

    Returns the FileHandler (to pass to stop_test_log), or None if logging is
    not applicable (e.g. -s mode or no log directory configured).
    """
    global _current_log_key, _current_file_handler

    logdir = getattr(item.config, '_test_logdir', None)
    capture = item.config.getoption('capture', default='fd')

    if not logdir or capture == 'no':
        return None

    key = _log_key(item)
    if key == _current_log_key and _current_file_handler is not None:
        return None  # reuse existing handler

    # Key changed — close previous handler if any
    if _current_file_handler is not None:
        logging.getLogger().removeHandler(_current_file_handler)
        _current_file_handler.close()
        _current_file_handler = None
        _current_log_key = None

    log_name = test_log_name(item)
    log_path = os.path.join(logdir, log_name)
    try:
        file_handler = logging.FileHandler(log_path, mode='w')
        file_handler.setFormatter(logging.Formatter('-%(levelname).1s- %(message)s'))
        file_handler.setLevel(logging.DEBUG)
        logging.getLogger().addHandler(file_handler)
        _current_log_key = key
        _current_file_handler = file_handler
        return file_handler
    except Exception as e:
        log.warning(f"Could not create test log file {log_path}: {e}")
        return None


def stop_test_log(handler, nextitem):
    """Close the per-module+device FileHandler only when the next test has a different
    key (different file or device) or when the session is ending (nextitem is None)."""
    global _current_log_key, _current_file_handler

    if _current_file_handler is None:
        return

    next_key = _log_key(nextitem)
    if next_key == _current_log_key:
        return  # next test shares the same log file

    logging.getLogger().removeHandler(_current_file_handler)
    _current_file_handler.close()
    _current_file_handler = None
    _current_log_key = None


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
    total = passed + failed + skipped + error

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
    if error > 0:
        summary(f"Errors: {error}")
    summary("=" * 80)


def ensure_newline():
    """Pytest's progress dots (F/.) don't end with newline — force one before our log output.
    Only needed when live logging is active (-s), otherwise it breaks -v formatting."""
    if live_logging:
        sys.stdout.write('\n')
        sys.stdout.flush()


def test_log_name(item):
    """Derive log filename from file basename + device param (from brackets in item.name).

    Examples:
      'live/frames/pytest-t2ff-pipeline.py::test_x[D455-104623060005]' -> 'pytest-t2ff-pipeline_D455-104623060005.log'
      'live/frames/pytest-t2ff-pipeline.py::test_x'                   -> 'pytest-t2ff-pipeline.log'
    """
    file_path = item.fspath
    basename = os.path.splitext(os.path.basename(str(file_path)))[0]

    match = re.search(r'\[(.+)\]', item.name)
    if match:
        device_id = match.group(1)
        log_name = f"{basename}_{device_id}"
    else:
        log_name = basename

    log_name = re.sub(r'[<>:"/\\|?*]', '_', log_name)
    return log_name + ".log"
