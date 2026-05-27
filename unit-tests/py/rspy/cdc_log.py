# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Background capture of firmware logs from a CDC ACM serial device.

On Linux the D585S exposes its firmware log over a USB CDC interface,
typically /dev/ttyACM1. This module spawns a daemon thread that drains
that interface into a timestamped text file for the duration of a test.
"""

import atexit
import datetime
import errno
import os
import select
import threading
import time

from rspy import log

DEFAULT_DEVICE = "/dev/ttyACM1"

# When the device is freshly enumerated (or re-enumerated after a librealsense
# USB claim), services like ModemManager probe the tty and hold it open with
# TIOCEXCL for a short window. We retry the open for a few seconds to ride
# that probe out instead of giving up immediately.
OPEN_RETRY_TIMEOUT_SEC = 10
OPEN_RETRY_DELAY_SEC = 0.5


def _open_with_retry( device_path, timeout=OPEN_RETRY_TIMEOUT_SEC, delay=OPEN_RETRY_DELAY_SEC ):
    """Open `device_path` non-blocking, retrying on EBUSY for up to `timeout` seconds."""
    deadline = time.monotonic() + timeout
    busy_logged = False
    while True:
        try:
            return os.open( device_path, os.O_RDONLY | os.O_NONBLOCK )
        except OSError as e:
            if e.errno == errno.EBUSY and time.monotonic() < deadline:
                if not busy_logged:
                    log.d( f"CDC log capture: {device_path} busy (likely ModemManager probe), retrying up to {timeout}s" )
                    busy_logged = True
                time.sleep( delay )
                continue
            log.w( f"CDC log capture skipped: cannot open {device_path}: {e}" )
            return None


class CDCLogCapture:
    def __init__( self, path, fd, out_file, stop_event, thread ):
        self.path = path
        self._fd = fd
        self._file = out_file
        self._stop = stop_event
        self._thread = thread
        self._stopped = False

    def stop( self ):
        if self._stopped:
            return
        self._stopped = True
        self._stop.set()
        self._thread.join( timeout=2 )
        if not self._file.closed:
            self._file.close()


def start_cdc_log( test_name, device_path=DEFAULT_DEVICE, out_dir="." ):
    """
    Open `device_path`, start a daemon thread that writes everything read from
    it into `<out_dir>/<test_name>_<YYYYMMDD_HHMMSS>.txt`, and return a handle
    whose `.stop()` method tears the capture down.

    If the device cannot be opened (e.g. running on a host without the CDC
    interface) a warning is logged and None is returned.

    A safety-net atexit handler also calls stop() so the file is flushed even
    if the test crashes before reaching its cleanup.
    """
    timestamp = datetime.datetime.now().strftime( "%Y%m%d_%H%M%S" )
    out_path = os.path.join( out_dir, f"{test_name}_{timestamp}.txt" )

    fd = _open_with_retry( device_path )
    if fd is None:
        return None

    out_file = open( out_path, "w", buffering=1 )
    stop_event = threading.Event()
    log.d( f"CDC log capture: {device_path} -> {out_path}" )

    def reader():
        try:
            while not stop_event.is_set():
                r, _, _ = select.select( [fd], [], [], 0.5 )
                if fd in r:
                    try:
                        data = os.read( fd, 4096 )
                    except BlockingIOError:
                        continue
                    if data:
                        out_file.write( data.decode( "utf-8", errors="replace" ) )
        finally:
            try:
                os.close( fd )
            except OSError:
                pass

    thread = threading.Thread( target=reader, daemon=True )
    thread.start()

    capture = CDCLogCapture( out_path, fd, out_file, stop_event, thread )
    atexit.register( capture.stop )
    return capture
