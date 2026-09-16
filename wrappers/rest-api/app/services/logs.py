# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""The output console (common/output-model.cpp): SDK log lines, the server's own warnings,
and firmware logs pulled from a device, kept in one bounded buffer and streamed as ``log``."""

import logging
import os
import re
import tempfile
import threading
import time
from collections import deque
from typing import Any, Callable, Deque, Dict, List, Optional

import pyrealsense2 as rs

SEVERITIES = {"debug": rs.log_severity.debug, "info": rs.log_severity.info,
              "warn": rs.log_severity.warn, "error": rs.log_severity.error, "fatal": rs.log_severity.fatal}
_SEVERITY_NAMES = {v: k for k, v in SEVERITIES.items()}
SEVERITY_RANK = {"debug": 0, "info": 1, "warn": 2, "error": 3, "fatal": 4}


FLUSH_INTERVAL = 0.1  # firmware logs arrive by the hundreds per second; clients get batches


# The SDK's file sink writes " dd/MM HH:mm:ss,ms LEVEL [thread] (file.cpp:123) message" (src/log.h)
_SDK_LINE = re.compile(r"^\s*(\d\d/\d\d \d\d:\d\d:\d\d,\d+)\s+(\w+)\s+\[(\d+)\]\s+\(([^:()]+):(\d+)\)\s?(.*)$")
_SDK_LEVELS = {"INFO": "info", "WARNING": "warn", "WARN": "warn", "ERROR": "error", "FATAL": "fatal",
               "DEBUG": "debug", "VERBOSE": "debug", "TRACE": "debug"}
TAIL_INTERVAL = 0.2


def parse_sdk_line(line: str) -> Optional[Dict[str, Any]]:
    """One line of the SDK log file as (severity, message, file, line), or None for a continuation."""
    m = _SDK_LINE.match(line.rstrip("\r\n"))
    if not m:
        return None
    return {"severity": _SDK_LEVELS.get(m.group(2).upper(), "info"), "message": m.group(6),
            "file": m.group(4), "line": int(m.group(5))}


class SdkLogTail:
    """Reads what the SDK appended to its log file since the last poll.

    The SDK logs from its own threads while holding its own locks; a Python callback there
    (rs.log_to_callback) needs the GIL, which a Python thread blocked inside an SDK call may
    hold - a deadlock seen in the wild. Tailing the file keeps the SDK's threads out of Python.
    """

    def __init__(self, path: str):
        self.path = path
        self._pos = 0

    def poll(self) -> List[str]:
        try:
            size = os.path.getsize(self.path)
        except OSError:
            return []
        if size < self._pos:  # rolled over
            self._pos = 0
        if size == self._pos:
            return []
        with open(self.path, "r", encoding="utf-8", errors="replace") as f:
            f.seek(self._pos)
            chunk = f.read()
            self._pos = f.tell()
        lines = chunk.split("\n")
        if not chunk.endswith("\n"):  # a partial last line: read it next time
            self._pos -= len(lines[-1].encode("utf-8", errors="replace"))
            lines = lines[:-1]
        return [l for l in lines if l.strip()]


class LogConsole:
    def __init__(self, emit: Callable[[str, Any], None], max_entries: int = 1000):
        self._emit = emit
        self._entries: Deque[Dict[str, Any]] = deque(maxlen=max_entries)
        self._pending: List[Dict[str, Any]] = []
        self._lock = threading.Lock()
        self._next_id = 1
        self._sdk_installed = False
        # One flusher thread for the life of the console: `add` only sets an event, so callers
        # never block on thread creation while holding the console lock.
        self._dirty = threading.Event()
        self._flusher = threading.Thread(target=self._flush_loop, name="console-flusher", daemon=True)
        self._flusher.start()
        self._tail_thread: Optional[threading.Thread] = None

    def add(self, severity: str, message: str, source: str = "sdk", file: Optional[str] = None,
            line: Optional[int] = None, **extra: Any) -> Dict[str, Any]:
        with self._lock:
            entry = {"id": self._next_id, "ts": time.time(), "severity": severity, "message": message,
                     "source": source, "file": file, "line": line, **extra}
            self._next_id += 1
            self._entries.append(entry)
            self._pending.append(entry)
        self._dirty.set()
        return entry

    def _flush_loop(self) -> None:
        while True:
            self._dirty.wait()
            time.sleep(FLUSH_INTERVAL)  # let a burst accumulate
            self._dirty.clear()
            self.flush()

    def flush(self) -> None:
        """Send what accumulated since the last flush as one ``log_batch`` event."""
        with self._lock:
            batch, self._pending = self._pending, []
        if batch:
            self._emit("log_batch", batch)

    def since(self, after_id: int = 0, limit: int = 500) -> List[Dict[str, Any]]:
        with self._lock:
            return [e for e in self._entries if e["id"] > after_id][-limit:]

    def clear(self) -> None:
        with self._lock:
            self._entries.clear()

    def set_max_entries(self, max_entries: int) -> None:
        with self._lock:
            self._entries = deque(self._entries, maxlen=max_entries)

    def install_sdk_logging(self, min_severity: str = "info", log_file: Optional[str] = None) -> None:
        """Route librealsense log lines here, via the SDK's file sink and a tailing thread."""
        if self._sdk_installed:
            return
        severity = SEVERITIES.get(min_severity, rs.log_severity.info)
        path = log_file or os.path.join(tempfile.gettempdir(), f"realsense-rest-api-{os.getpid()}.log")
        # The SDK's file sink flushes every 10 lines (src/log.h LogFlushThreshold). The scratch
        # file therefore takes everything at debug so lines reach the console promptly, and the
        # console keeps only what the configured severity asks for. A user-chosen file gets the
        # configured severity as is.
        min_rank = SEVERITY_RANK.get(min_severity, 1)
        try:
            rs.log_to_file(rs.log_severity.debug if not log_file else severity, path)
            if not log_file:
                rs.enable_rolling_log_file(20)  # MB; the scratch file must not grow without bound
        except RuntimeError as exc:
            logging.warning("SDK log capture unavailable: %s", exc)
            return
        self._sdk_installed = True
        tail = SdkLogTail(path)

        def run() -> None:
            last: Optional[Dict[str, Any]] = None
            while True:
                for line in tail.poll():
                    parsed = parse_sdk_line(line)
                    if parsed is None:
                        if last is not None:  # continuation of a multi-line message
                            with self._lock:
                                last["message"] += "\n" + line.strip()
                        continue
                    if SEVERITY_RANK.get(parsed["severity"], 1) < min_rank:
                        last = None
                        continue
                    last = self.add(parsed["severity"], parsed["message"], "sdk", parsed["file"], parsed["line"])
                time.sleep(TAIL_INTERVAL)

        self._tail_thread = threading.Thread(target=run, name="sdk-log-tail", daemon=True)
        self._tail_thread.start()

    def attach_python_logging(self, level: int = logging.WARNING) -> logging.Handler:
        """The server's own warnings and errors belong in the console too."""
        console = self

        class _Handler(logging.Handler):
            def emit(self, record: logging.LogRecord) -> None:
                severity = "error" if record.levelno >= logging.ERROR else "warn" if record.levelno >= logging.WARNING else "info"
                console.add(severity, record.getMessage(), "server", record.filename, record.lineno)

        handler = _Handler(level)
        logging.getLogger().addHandler(handler)
        return handler


def _hex(data) -> str:
    return " ".join(f"{b:02x}" for b in data)


class FwLogCollector:
    """Pulls firmware logs from one device on a thread, parsed when an XML definition is given.

    Every pull is an HWM command: it takes the device's option lock like any other control
    traffic, so it never races the option poller or a REST write.
    """

    def __init__(self, dev, device_id: str, console: LogConsole, lock: threading.Lock, xml_text: Optional[str],
                 on_lost: Optional[Callable[[str, str], None]] = None):
        self.device_id = device_id
        self._console = console
        self._lock = lock
        self._on_lost = on_lost
        self._logger = rs.firmware_logger(dev)
        self._parses = bool(xml_text) and self._logger.init_parser(xml_text)
        self._stop = threading.Event()
        self._thread: Optional[threading.Thread] = None

    @property
    def running(self) -> bool:
        return self._thread is not None and self._thread.is_alive()

    def start(self) -> None:
        with self._lock:
            self._logger.start_collecting()
        self._thread = threading.Thread(target=self._run, name=f"fw-logs-{self.device_id}", daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._stop.set()
        if self._thread:
            self._thread.join(timeout=3)
        try:
            with self._lock:
                self._logger.stop_collecting()
        except RuntimeError:
            pass

    def _run(self) -> None:
        while not self._stop.is_set():
            try:
                got = self.pull_once(flash=False)
            except RuntimeError as exc:
                if "no longer present" in str(exc).lower() or "0xc00d3ea2" in str(exc).lower():
                    logging.warning("fw logs on %s: device gone (%s); stopping", self.device_id, exc)
                    self._stop.set()
                    if self._on_lost:
                        self._on_lost(self.device_id, str(exc)[:120])
                    break
                logging.debug("fw log pull failed on %s: %s", self.device_id, exc)
                got = False
            if not got:
                self._stop.wait(0.05)

    def pull_once(self, flash: bool) -> bool:
        """Read one message (live or from flash) into the console; False when there is none."""
        message = self._logger.create_message()
        with self._lock:
            got = self._logger.get_flash_log(message) if flash else self._logger.get_firmware_log(message)
        if not got:
            return False
        self._console.add(**self._describe(message), source="fw-flash" if flash else "fw", device_id=self.device_id)
        return True

    def _describe(self, message) -> Dict[str, Any]:
        if self._parses:
            parsed = self._logger.create_parsed_message()
            if self._logger.parse_log(message, parsed):
                module = parsed.get_module_name()
                return {"severity": str(parsed.get_severity()).lower(),
                        "message": parsed.get_message(), "file": parsed.get_file_name(), "line": parsed.get_line(),
                        "thread": parsed.get_thread_name(), "module": None if module == "Unknown" else module,
                        "fw_timestamp": parsed.get_timestamp(), "sequence_id": parsed.get_sequence_id()}
        return {"severity": str(message.get_severity_str()).lower() or "info",
                "message": f"FW_Log_Data: {_hex(message.get_data())}", "file": None, "line": None,
                "fw_timestamp": message.get_timestamp()}

    def recover_flash(self) -> int:
        """Pull everything the firmware kept in flash; returns how many messages arrived."""
        count = 0
        while self.pull_once(flash=True):
            count += 1
        return count
