# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Tests for the rspy/signals.py abort watchdog: a separate process that force-kills a run
whose signal handler can never execute (main thread stuck in a native call).
"""

import os
import signal
import subprocess
import sys
import time

import pytest

POSIX = sys.platform != 'win32'

pytestmark = [pytest.mark.skipif(not POSIX, reason='watchdog is POSIX-only')]
if POSIX:
    # bounds the blocking reads below; 'signal' (not the conftest-wide 'thread' default) so
    # a timeout interrupts this test only instead of ending the whole session. Applied only
    # on POSIX: pytest-timeout resolves the method even for skipped tests, and 'signal'
    # needs SIGALRM, which Windows doesn't have.
    pytestmark.append(pytest.mark.timeout(60, method='signal'))

UNIT_TESTS_DIR = os.path.normpath(os.path.join(os.path.dirname(__file__), '..'))


@pytest.fixture
def spawn():
    """Starts child runs, and at teardown kills any survivor plus its watchdog."""
    children = []

    def _spawn(child_code):
        # start_new_session: own process group, so signal_group() below can emulate a
        # Jenkins abort (SIGTERM to the whole tree) without hitting this pytest itself
        child = subprocess.Popen([sys.executable, '-u', '-c', child_code],
                                 cwd=UNIT_TESTS_DIR,
                                 env={**os.environ, 'PYTHONPATH': os.path.join(UNIT_TESTS_DIR, 'py')},
                                 stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT,
                                 text=True,
                                 start_new_session=True)
        children.append(child)
        return child

    yield _spawn

    for child in children:
        try:
            os.killpg(os.getpgid(child.pid), signal.SIGKILL)
        except OSError:
            pass  # already gone
        child.wait()
        child.stdout.close()


def signal_group(proc, sig):
    """Jenkins aborts SIGTERM every process in the build's tree — watchdog included."""
    os.killpg(os.getpgid(proc.pid), sig)


def read_line_starting_with(proc, prefix):
    """Blocking; the file-level timeout bounds the wait."""
    while True:
        line = proc.stdout.readline()
        if line.startswith(prefix):
            return line
        if line == '':
            pytest.fail(f'child never printed {prefix}')


def wait_for_exit(proc, timeout):
    try:
        proc.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        pytest.fail(f'child still alive after {timeout}s')


def test_watchdog_kills_stuck_cleanup(spawn):
    """SIGTERM handler that never finishes cleanup -> watchdog SIGKILLs after grace."""
    child = spawn('''
import time
from rspy import signals

def stuck_cleanup():
    while True:  # simulates cleanup that can never complete
        time.sleep(1)

signals.WATCHDOG_GRACE_S = 2
signals.register_signal_handlers(stuck_cleanup)
print('READY', flush=True)
time.sleep(120)
''')
    read_line_starting_with(child, 'READY')
    signal_group(child, signal.SIGTERM)
    # grace is 2s; well before the 120s sleep ends the watchdog must have killed it
    wait_for_exit(child, timeout=15)
    assert child.returncode == -signal.SIGKILL


def test_watchdog_exits_when_parent_ends_normally(spawn):
    """No signal: parent exits normally, watchdog must follow (no leaked process)."""
    child = spawn('''
from rspy import signals

signals.register_signal_handlers()
print('READY', flush=True)
print('WATCHDOG_PID', signals._watchdog.pid, flush=True)
''')
    read_line_starting_with(child, 'READY')
    # a child that failed to start a watchdog dies on the AttributeError below, and the
    # read then hits EOF instead of a pid
    watchdog_pid = int(read_line_starting_with(child, 'WATCHDOG_PID').split()[1])
    wait_for_exit(child, timeout=15)
    # give the watchdog a moment to notice the pipe EOF
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        try:
            os.kill(watchdog_pid, 0)
        except OSError:
            return  # watchdog gone
        time.sleep(0.2)
    pytest.fail('watchdog process leaked after parent exited')


def test_clean_abort_not_killed_by_watchdog(spawn):
    """Cleanup that finishes within grace: process exits via os._exit(1), not SIGKILL."""
    child = spawn('''
import time
from rspy import signals

signals.WATCHDOG_GRACE_S = 30
signals.register_signal_handlers(lambda: time.sleep(0.5))
print('READY', flush=True)
time.sleep(120)
''')
    read_line_starting_with(child, 'READY')
    signal_group(child, signal.SIGTERM)
    wait_for_exit(child, timeout=15)
    assert child.returncode == 1  # normal abort path, watchdog never fired
