# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""The console's command line (output-model.cpp run_command): raw hex bytes go straight to the
hardware monitor; named commands come from a Commands.xml definition file."""

import re
import xml.etree.ElementTree as ET
from typing import List, Optional

import pyrealsense2 as rs

from app.core.errors import RealSenseError

HEX_LINE = re.compile(r"^(?:[0-9A-Fa-f]{2}\s*)+$")


def command_names(xml_text: Optional[str]) -> List[str]:
    """Command names a Commands.xml defines, for autocompletion."""
    if not xml_text:
        return []
    try:
        root = ET.fromstring(xml_text)
    except ET.ParseError:
        return []
    return sorted({el.get("Name") for el in root.iter("Command") if el.get("Name")})


def _format_response(data) -> str:
    """Hex bytes, 80 per line, as the legacy console prints a raw response."""
    bytes_ = list(data)
    return "\n".join(" ".join(f"{b:02x}" for b in bytes_[i:i + 80]) for i in range(0, len(bytes_), 80))


def run(dev, line: str, xml_text: Optional[str]) -> str:
    """Execute one console command against a device and return the text to print."""
    line = line.strip()
    if not line:
        raise RealSenseError(status_code=400, detail="Empty command")
    if not dev.is_debug_protocol():
        raise RealSenseError(status_code=400, detail="Device does not support hardware monitor commands")
    debug = dev.as_debug_protocol()
    if HEX_LINE.match(line):
        raw = [int(word, 16) for word in line.split()]
        return _format_response(debug.send_and_receive_raw_data(raw))
    if not xml_text:
        raise RealSenseError(status_code=400, detail="Set a Commands.xml file in Settings to use named commands")
    parser = rs.terminal_parser(xml_text)
    try:
        request = parser.parse_command(line)
    except RuntimeError as exc:
        raise RealSenseError(status_code=400, detail=f"Unknown command: {exc}")
    response = debug.send_and_receive_raw_data(list(request))
    return parser.parse_response(line, list(response))
