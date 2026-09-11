# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import logging
import threading
import time

import pytest

from app.core.errors import RealSenseError
from app.services import terminal
from app.services.logs import FwLogCollector, LogConsole


def _console(max_entries=1000):
    emitted = []
    console = LogConsole(lambda ev, payload: emitted.append((ev, payload)), max_entries)
    return console, emitted


def test_entries_are_numbered_emitted_and_bounded():
    console, emitted = _console(max_entries=3)
    for i in range(5):
        console.add("info", f"m{i}")
    assert [e["message"] for e in console.since()] == ["m2", "m3", "m4"]
    assert [e["id"] for e in console.since(after_id=3)] == [4, 5]
    assert emitted[0] == ("log", console.since()[0]) or emitted[0][0] == "log"
    console.clear()
    assert console.since() == []


def test_python_warnings_land_in_the_console():
    console, _ = _console()
    handler = console.attach_python_logging()
    try:
        logging.getLogger("x").warning("careful %s", 1)
        logging.getLogger("x").info("quiet")
    finally:
        logging.getLogger().removeHandler(handler)
    entries = console.since()
    assert len(entries) == 1 and entries[0]["severity"] == "warn" and entries[0]["message"] == "careful 1" and entries[0]["source"] == "server"


class _Msg:
    def __init__(self, data):
        self.data = data

    def get_data(self):
        return self.data

    def get_severity_str(self):
        return "INFO"

    def get_timestamp(self):
        return 42


class _Parsed:
    def get_message(self): return "temperature ok"
    def get_file_name(self): return "thermal.c"
    def get_thread_name(self): return "main"
    def get_module_name(self): return "Unknown"
    def get_severity(self): return "Info"
    def get_line(self): return 12
    def get_timestamp(self): return 99
    def get_sequence_id(self): return 7


class _FakeFwLogger:
    def __init__(self, dev, live=(), flash=(), parses=False):
        self.live, self.flash, self.parses = list(live), list(flash), parses
        self.collecting = False

    def init_parser(self, _xml): return self.parses
    def start_collecting(self): self.collecting = True
    def stop_collecting(self): self.collecting = False
    def create_message(self): return _Msg(b"")
    def create_parsed_message(self): return _Parsed()

    def get_firmware_log(self, msg):
        if not self.live:
            return False
        msg.data = self.live.pop(0)
        return True

    def get_flash_log(self, msg):
        if not self.flash:
            return False
        msg.data = self.flash.pop(0)
        return True

    def parse_log(self, _msg, _parsed): return True


@pytest.fixture
def fw(monkeypatch):
    from app.services import logs
    made = {}

    def factory(dev):
        made["logger"] = _FakeFwLogger(dev, **made.get("kw", {}))
        return made["logger"]
    monkeypatch.setattr(logs.rs, "firmware_logger", factory)
    return made


def test_raw_fw_logs_are_hex_dumped(fw):
    fw["kw"] = {"live": [b"\x01\x02\xff"]}
    console, _ = _console()
    collector = FwLogCollector(object(), "dev", console, threading.Lock(), None)
    assert collector.pull_once(flash=False) is True
    assert collector.pull_once(flash=False) is False
    entry = console.since()[0]
    assert entry["message"] == "FW_Log_Data: 01 02 ff" and entry["source"] == "fw" and entry["device_id"] == "dev"


def test_parsed_fw_logs_carry_file_line_and_thread(fw):
    fw["kw"] = {"live": [b"\x00"], "parses": True}
    console, _ = _console()
    FwLogCollector(object(), "dev", console, threading.Lock(), "<xml/>").pull_once(flash=False)
    entry = console.since()[0]
    assert (entry["message"], entry["file"], entry["line"], entry["thread"], entry["module"]) == ("temperature ok", "thermal.c", 12, "main", None)


def test_collector_thread_drains_live_logs_and_stops(fw):
    fw["kw"] = {"live": [b"\x01", b"\x02"]}
    console, _ = _console()
    collector = FwLogCollector(object(), "dev", console, threading.Lock(), None)
    collector.start()
    deadline = time.time() + 2
    while len(console.since()) < 2 and time.time() < deadline:
        time.sleep(0.02)
    collector.stop()
    assert len(console.since()) == 2 and fw["logger"].collecting is False and not collector.running


def test_flash_recovery_pulls_everything(fw):
    fw["kw"] = {"flash": [b"\x0a", b"\x0b", b"\x0c"]}
    console, _ = _console()
    assert FwLogCollector(object(), "dev", console, threading.Lock(), None).recover_flash() == 3
    assert all(e["source"] == "fw-flash" for e in console.since())


class _Debug:
    def __init__(self):
        self.sent = []

    def send_and_receive_raw_data(self, data):
        self.sent.append(list(data))
        return [0x10, 0, 0, 0] + list(range(90))


class _Dev:
    def __init__(self, debug=True):
        self.debug, self._dbg = debug, _Debug()

    def is_debug_protocol(self): return self.debug
    def as_debug_protocol(self): return self._dbg


def test_terminal_sends_raw_hex_and_prints_the_response_80_per_line():
    dev = _Dev()
    out = terminal.run(dev, "14 00 ab CD", None)
    assert dev._dbg.sent == [[0x14, 0x00, 0xAB, 0xCD]]
    lines = out.split("\n")
    assert lines[0].startswith("10 00 00 00 00 01") and len(lines) == 2 and len(lines[0].split()) == 80


def test_terminal_named_commands_need_an_xml(monkeypatch):
    with pytest.raises(RealSenseError) as exc:
        terminal.run(_Dev(), "gvd", None)
    assert exc.value.status_code == 400

    class _Parser:
        def __init__(self, xml): self.xml = xml
        def parse_command(self, cmd):
            if cmd != "gvd":
                raise RuntimeError("no such command")
            return [0x10, 0, 0, 0]
        def parse_response(self, cmd, response): return f"parsed {cmd}: {len(response)} bytes"
    monkeypatch.setattr(terminal.rs, "terminal_parser", _Parser)
    assert terminal.run(_Dev(), "gvd", "<Commands/>") == "parsed gvd: 94 bytes"
    with pytest.raises(RealSenseError):
        terminal.run(_Dev(), "nope", "<Commands/>")


def test_terminal_refuses_devices_without_hwm_and_empty_lines():
    with pytest.raises(RealSenseError):
        terminal.run(_Dev(debug=False), "10 00", None)
    with pytest.raises(RealSenseError):
        terminal.run(_Dev(), "   ", None)


def test_command_names_come_from_the_xml():
    xml = '<Commands><Command Name="GVD" Opcode="0x10"/><Command Name="GLD" Opcode="0x0F"/><Other Name="x"/></Commands>'
    assert terminal.command_names(xml) == ["GLD", "GVD"]
    assert terminal.command_names(None) == [] and terminal.command_names("<broken") == []
