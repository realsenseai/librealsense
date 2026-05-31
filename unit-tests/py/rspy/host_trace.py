# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Host-side diagnostics capture for librealsense tests.

Provides start_dmesg_log() which spawns a background subprocess that streams
the Linux kernel ring buffer into a timestamped file alongside the test's
CDC/SMCU captures. Useful for catching v4l2/uvcvideo/usbfs warnings that
show up when a stream fails to start or restart.
"""

import atexit
import datetime
import os
import shutil
import subprocess
import time

from rspy import log


class _ProcessCapture:
    def __init__( self, path, proc, file_handle, method ):
        self.path = path
        self.method = method
        self._proc = proc
        self._file = file_handle
        self._stopped = False

    def stop( self ):
        if self._stopped:
            return
        self._stopped = True
        try:
            self._proc.terminate()
            self._proc.wait( timeout=2 )
        except subprocess.TimeoutExpired:
            try:
                self._proc.kill()
                self._proc.wait( timeout=1 )
            except (subprocess.TimeoutExpired, OSError):
                pass
        except OSError:
            pass
        if self._file is not None and not self._file.closed:
            self._file.close()


def _try_spawn( out_path, cmd, settle_sec=0.3 ):
    """Open out_path, start cmd writing into it. Return (proc, file) or (None, None)."""
    f = open( out_path, "w", buffering=1 )
    try:
        proc = subprocess.Popen( cmd, stdout=f, stderr=subprocess.STDOUT )
    except (OSError, FileNotFoundError):
        f.close()
        return None, None
    time.sleep( settle_sec )
    if proc.poll() is not None:
        f.close()
        return None, None
    return proc, f


def start_dmesg_log( test_name, out_dir="." ):
    """
    Start a background capture of the kernel ring buffer into
    <out_dir>/<test_name>_dmesg_<YYYYMMDD_HHMMSS>.txt.

    Tries `journalctl -kf` first (works on systemd hosts without root), then
    falls back to `sudo -n dmesg -wT` (only if passwordless sudo is set up).
    Returns a handle whose .stop() terminates the subprocess and closes the
    file, or None if no capture method is available.

    An atexit handler also calls stop(), so the file flushes on crash.
    """
    timestamp = datetime.datetime.now().strftime( "%Y%m%d_%H%M%S" )
    out_path = os.path.join( out_dir, f"{test_name}_dmesg_{timestamp}.txt" )

    proc = None
    file_handle = None
    method = None

    if shutil.which( "journalctl" ):
        proc, file_handle = _try_spawn(
            out_path,
            ["journalctl", "-kf", "-o", "short-precise", "--since", "now"],
        )
        if proc is not None:
            method = "journalctl"

    if proc is None and shutil.which( "dmesg" ) and shutil.which( "sudo" ):
        try:
            ok = subprocess.run(
                ["sudo", "-n", "true"],
                capture_output=True,
                timeout=1,
            ).returncode == 0
        except (subprocess.TimeoutExpired, OSError):
            ok = False
        if ok:
            proc, file_handle = _try_spawn( out_path, ["sudo", "-n", "dmesg", "-wT"] )
            if proc is not None:
                method = "sudo dmesg"

    if proc is None:
        log.w( "dmesg log capture skipped: neither journalctl nor passwordless 'sudo dmesg' available" )
        return None

    log.d( f"dmesg log capture ({method}): -> {out_path}" )
    capture = _ProcessCapture( out_path, proc, file_handle, method )
    atexit.register( capture.stop )
    return capture
