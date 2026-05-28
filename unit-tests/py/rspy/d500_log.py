# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Background capture of firmware logs from D500 / D585S serial devices.

Two flavours typically used on D585S:
  - CDC ACM channel from the AP CPU (default: /dev/ttyACM1, no baud needed)
  - SMCU UART exposed via the on-board CP2105 USB-UART bridge
    (typically /dev/ttyUSB1 at 460800 baud)

A daemon thread drains the device into a timestamped text file for the
duration of a test. `.stop()` tears it down; an atexit safety net runs
stop() even if the test crashes.
"""

import atexit
import datetime
import errno
import os
import select
import shutil
import subprocess
import termios
import threading
import time

from rspy import log

DEFAULT_DEVICE = "/dev/ttyACM1"

# When the device is freshly enumerated (or re-enumerated after a librealsense
# USB claim), services like ModemManager probe the tty and hold it open with
# TIOCEXCL for a short window. We retry the open for a while to ride that out.
OPEN_RETRY_TIMEOUT_SEC = 30
OPEN_RETRY_DELAY_SEC = 0.5

# Map of supported baud rates to termios constants. Add more as needed.
_BAUD_TABLE = {
    9600:   termios.B9600,
    19200:  termios.B19200,
    38400:  termios.B38400,
    57600:  termios.B57600,
    115200: termios.B115200,
    230400: termios.B230400,
    460800: termios.B460800,
    921600: termios.B921600,
}


def _identify_holder( device_path ):
    """Best-effort: return a short string naming the process(es) holding the device, or None."""
    for cmd in ( ["fuser", "-v", device_path], ["lsof", device_path] ):
        if not shutil.which( cmd[0] ):
            continue
        try:
            result = subprocess.run( cmd, capture_output=True, text=True, timeout=2 )
            out = (result.stdout + result.stderr).strip()
            if out:
                return f"{cmd[0]}: {out}"
        except (subprocess.TimeoutExpired, OSError):
            continue
    return None


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
                    holder = _identify_holder( device_path )
                    holder_msg = f" -- holder: {holder}" if holder else ""
                    log.d( f"D500 log capture: {device_path} busy, retrying up to {timeout}s{holder_msg}" )
                    busy_logged = True
                time.sleep( delay )
                continue
            holder = _identify_holder( device_path ) if e.errno == errno.EBUSY else None
            holder_msg = f" (holder: {holder})" if holder else ""
            log.w( f"D500 log capture skipped: cannot open {device_path}: {e}{holder_msg}" )
            return None


def _configure_tty_raw( fd, baud ):
    """Set the tty to raw mode at the requested baud. Required for CP2105/FTDI;
    CDC ACM nodes ignore baud but the call is harmless there too."""
    speed = _BAUD_TABLE.get( baud )
    if speed is None:
        raise ValueError( f"unsupported baud {baud}; add to _BAUD_TABLE" )
    attrs = termios.tcgetattr( fd )
    iflag, oflag, cflag, lflag, _ispeed, _ospeed, cc = attrs
    # raw input
    iflag &= ~(termios.IGNBRK | termios.BRKINT | termios.PARMRK | termios.ISTRIP
               | termios.INLCR | termios.IGNCR | termios.ICRNL | termios.IXON)
    # raw output
    oflag &= ~termios.OPOST
    # non-canonical, no echo, no signals
    lflag &= ~(termios.ECHO | termios.ECHONL | termios.ICANON | termios.ISIG | termios.IEXTEN)
    # 8N1, enable receiver, ignore modem control lines
    cflag &= ~(termios.CSIZE | termios.PARENB)
    cflag |= termios.CS8 | termios.CREAD | termios.CLOCAL
    termios.tcsetattr( fd, termios.TCSANOW, [iflag, oflag, cflag, lflag, speed, speed, cc] )


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


def start_cdc_log( test_name, device_path=DEFAULT_DEVICE, out_dir=".", baud=None ):
    """
    Open `device_path`, start a daemon thread that writes everything read from
    it into `<out_dir>/<test_name>_<YYYYMMDD_HHMMSS>.txt`, and return a handle
    whose `.stop()` method tears the capture down.

    `baud` is required for true UART devices (e.g. /dev/ttyUSB* on a CP2105
    bridge). Pass None for CDC ACM nodes that don't honour baud.

    Returns None and logs a warning if the device can't be opened.
    An atexit handler also calls stop() so the file flushes on crash.
    """
    timestamp = datetime.datetime.now().strftime( "%Y%m%d_%H%M%S" )
    out_path = os.path.join( out_dir, f"{test_name}_{timestamp}.txt" )

    fd = _open_with_retry( device_path )
    if fd is None:
        return None

    if baud is not None:
        try:
            _configure_tty_raw( fd, baud )
        except (termios.error, ValueError, OSError) as e:
            log.w( f"D500 log capture skipped: cannot configure {device_path} @{baud}: {e}" )
            os.close( fd )
            return None

    out_file = open( out_path, "w", buffering=1 )
    stop_event = threading.Event()
    log.d( f"D500 log capture: {device_path} -> {out_path}" )

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
