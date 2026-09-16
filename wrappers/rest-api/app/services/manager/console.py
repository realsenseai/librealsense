# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Console-facing device operations: firmware logs and the terminal."""

from pathlib import Path
from typing import Any, Dict, List, Optional

from app.core.errors import RealSenseError
from app.services import terminal
from app.services.logs import FwLogCollector


class ConsoleMixin:
    """Mixed into RealSenseManager; see rs_manager.py."""

    def _read_setting_file(self, path: str) -> Optional[str]:
        if not path:
            return None
        try:
            return Path(path).read_text(encoding="utf-8")
        except OSError as exc:
            raise RealSenseError(status_code=400, detail=f"Cannot read {path}: {exc}")

    def fw_logs_status(self, device_id: str) -> Dict[str, Any]:
        collector = self._fw_log_collectors.get(device_id)
        return {"device_id": device_id, "running": bool(collector and collector.running),
                "parsed": bool(collector and collector._parses)}

    def start_fw_logs(self, device_id: str) -> Dict[str, Any]:
        """Start pulling firmware logs into the console; parsed when Settings names an XML."""
        dev = self._require_device(device_id)
        if device_id in self._fw_log_collectors and self._fw_log_collectors[device_id].running:
            return self.fw_logs_status(device_id)
        if not dev.is_firmware_logger():
            raise RealSenseError(status_code=400, detail="Device does not expose firmware logs")
        xml_text = self._read_setting_file(self.settings.get().paths.hwlogger_xml)
        collector = FwLogCollector(dev, device_id, self.console, self.option_lock(device_id), xml_text, on_lost=self.device_lost)
        collector.start()
        self._fw_log_collectors[device_id] = collector
        return self.fw_logs_status(device_id)

    def stop_fw_logs(self, device_id: str) -> Dict[str, Any]:
        collector = self._fw_log_collectors.pop(device_id, None)
        if collector:
            collector.stop()
        return self.fw_logs_status(device_id)

    def recover_flash_logs(self, device_id: str) -> Dict[str, Any]:
        """The device menu's "Recover logs from flash"; the messages land in the console."""
        dev = self._require_device(device_id)
        if not dev.is_firmware_logger():
            raise RealSenseError(status_code=400, detail="Device does not expose firmware logs")
        collector = self._fw_log_collectors.get(device_id)
        if collector is None:
            xml_text = self._read_setting_file(self.settings.get().paths.hwlogger_xml)
            collector = FwLogCollector(dev, device_id, self.console, self.option_lock(device_id), xml_text)
        return {"device_id": device_id, "messages": collector.recover_flash()}

    def run_terminal(self, device_id: str, line: str) -> str:
        dev = self._require_device(device_id)
        xml_text = self._read_setting_file(self.settings.get().paths.commands_xml)
        with self.option_lock(device_id):
            output = terminal.run(dev, line, xml_text)
        self.console.add("info", output, "terminal", device_id=device_id, command=line)
        return output

    def terminal_commands(self) -> List[str]:
        return terminal.command_names(self._read_setting_file(self.settings.get().paths.commands_xml))
