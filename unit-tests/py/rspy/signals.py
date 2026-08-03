# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

from rspy import log
import os, sys, signal, subprocess

signal_handler = lambda: log.d("Signal handler not set")
_cleanup_in_progress = False
_watchdog = None

# Grace the watchdog gives this process to clean up after SIGTERM/SIGINT before
# force-killing it (must exceed a normal hub/device cleanup)
WATCHDOG_GRACE_S = 60

# Separate watchdog process: the handlers below never run if the main thread is stuck
# in a native call (e.g. mutex/GIL deadlock), so a Jenkins abort would leave an orphan
# holding the device hub. The watchdog gets the same signal, waits out the grace, then
# SIGKILLs us; it exits on its own when our end of its stdin pipe closes.
_WATCHDOG_CODE = '''
import os, signal, sys, time
ppid, grace = int(sys.argv[1]), float(sys.argv[2])
def _abort(signum, frame):
    deadline = time.monotonic() + grace
    while time.monotonic() < deadline:
        try:
            os.kill(ppid, 0)
        except OSError:
            os._exit(0)  # parent exited on its own during the grace period
        time.sleep(0.5)
    try:
        os.kill(ppid, signal.SIGKILL)
    except OSError:
        pass
    os._exit(0)
signal.signal(signal.SIGTERM, _abort)
signal.signal(signal.SIGINT, _abort)
os.write(1, b'R')  # handshake: handlers armed, parent may proceed
while True:
    try:
        if not os.read(0, 1):  # EOF: parent exited (or crashed) without a signal
            os._exit(0)
    except OSError:
        os._exit(0)
'''


def start_abort_watchdog():
    """
    Start the GIL-independent abort watchdog (POSIX only; no-op on Windows or if one is
    still running). See _WATCHDOG_CODE above for why it must be a separate process.
    """
    global _watchdog
    if os.name != 'posix' or (_watchdog is not None and _watchdog.poll() is None):
        return
    try:
        if _watchdog is not None:  # the previous one died: release its pipe before replacing it
            _watchdog.stdin.close()
            _watchdog = None
        proc = subprocess.Popen([sys.executable, '-c', _WATCHDOG_CODE, str(os.getpid()), str(WATCHDOG_GRACE_S)],
                                stdin=subprocess.PIPE,
                                stdout=subprocess.PIPE,
                                stderr=subprocess.DEVNULL)
        ack = proc.stdout.read(1)  # wait until its signal handlers are armed
        proc.stdout.close()
        if ack != b'R':
            log.w('abort watchdog died during startup')
            return
        _watchdog = proc
        log.d('abort watchdog started, pid', _watchdog.pid)
    except Exception as e:
        log.w('failed to start abort watchdog:', e)


def register_signal_handlers(on_signal=None):
    def handle_abort(signum, _):
        global signal_handler, _cleanup_in_progress
        if _cleanup_in_progress:
            # Second signal during cleanup — force-exit immediately so we don't hang
            log.w("got signal", signum, "during cleanup — force-exiting")
            os._exit(1)
        _cleanup_in_progress = True
        log.w("got signal", signum, "aborting... ")
        signal_handler()
        os._exit(1)

    global signal_handler
    signal_handler = on_signal or signal_handler

    signal.signal(signal.SIGTERM, handle_abort)  # for when aborting via Jenkins
    signal.signal(signal.SIGINT, handle_abort)  # for Ctrl+C
    start_abort_watchdog()
