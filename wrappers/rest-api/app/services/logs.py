# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""The output console (common/output-model.cpp): SDK log lines, the server's own warnings,
and firmware logs pulled from a device, kept in one bounded buffer and streamed as ``log``."""

import logging
import threading
import time
from collections import deque
from typing import Any, Callable, Deque, Dict, List, Optional

import pyrealsense2 as rs

SEVERITIES = {"debug": rs.log_severity.debug, "info": rs.log_severity.info,
              "warn": rs.log_severity.warn, "error": rs.log_severity.error, "fatal": rs.log_severity.fatal}
_SEVERITY_NAMES = {v: k for k, v in SEVERITIES.items()}


FLUSH_INTERVAL = 0.1  # firmware logs arrive by the hundreds per second; clients get batches


class LogConsole:
    def __init__(self, emit: Callable[[str, Any], None], max_entries: int = 1000):
        self._emit = emit
        self._entries: Deque[Dict[str, Any]] = deque(maxlen=max_entries)
        self._pending: List[Dict[str, Any]] = []
        self._lock = threading.Lock()
        self._next_id = 1
        self._sdk_installed = False
        self._flusher: Optional[threading.Timer] = None

    def add(self, severity: str, message: str, source: str = "sdk", file: Optional[str] = None,
            line: Optional[int] = None, **extra: Any) -> Dict[str, Any]:
        with self._lock:
            entry = {"id": self._next_id, "ts": time.time(), "severity": severity, "message": message,
                     "source": source, "file": file, "line": line, **extra}
            self._next_id += 1
            self._entries.append(entry)
            self._pending.append(entry)
            if self._flusher is None:
                self._flusher = threading.Timer(FLUSH_INTERVAL, self.flush)
                self._flusher.daemon = True
                self._flusher.start()
        return entry

    def flush(self) -> None:
        """Send what accumulated since the last flush as one ``log_batch`` event."""
        with self._lock:
            batch, self._pending = self._pending, []
            self._flusher = None
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
        """Route librealsense log lines here (once per process; the SDK keeps every callback)."""
        if self._sdk_installed:
            return
        severity = SEVERITIES.get(min_severity, rs.log_severity.info)

        def on_log(sev, msg):
            self.add(_SEVERITY_NAMES.get(sev, "info"), msg.raw(), "sdk", msg.filename(), msg.line_number())

        try:
            rs.log_to_callback(severity, on_log)
            if log_file:
                rs.log_to_file(severity, log_file)
            self._sdk_installed = True
        except RuntimeError as exc:
            logging.warning("SDK log capture unavailable: %s", exc)

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

    def __init__(self, dev, device_id: str, console: LogConsole, lock: threading.Lock, xml_text: Optional[str]):
        self.device_id = device_id
        self._console = console
        self._lock = lock
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
