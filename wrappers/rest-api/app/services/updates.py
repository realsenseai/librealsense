# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Software and firmware update lookup against the versions database, as the legacy updates
window does (common/updates-model.cpp, sw-update/versions-db-manager).

Each DB entry has component (LIBREALSENSE | FIRMWARE), policy_type (ESSENTIAL | RECOMMENDED),
device_name pattern, platform, version, link, release_notes_link and description.
"""

import json
import logging
import urllib.request
from typing import Any, Dict, List, Optional

from app.services import firmware as fw

_cache: Dict[str, List[Dict[str, Any]]] = {}


def fetch_entries(url: str) -> Optional[List[Dict[str, Any]]]:
    """The DB's 'versions' list from ``url`` (https or file://), cached per URL; None on failure."""
    if url in _cache:
        return _cache[url]
    try:
        with urllib.request.urlopen(url, timeout=5) as resp:
            data = json.loads(resp.read().decode("utf-8"))
    except Exception as exc:
        logging.warning("Could not fetch versions DB %s: %s", url, exc)
        return None
    entries = data.get("versions") or []
    if entries:
        _cache[url] = entries
    return entries


def _matches(entry: Dict[str, Any], component: str, device_name: str, host: str) -> bool:
    return (entry.get("component") == component
            and (entry.get("platform") or "*") in ("*", host)
            and fw._device_name_matches(entry.get("device_name", "*"), device_name))


def _candidate(entries, component: str, policy: str, device_name: str, host: str) -> Optional[Dict[str, Any]]:
    for e in entries or []:
        if e.get("policy_type") == policy and _matches(e, component, device_name, host):
            return {"version": e.get("version"), "link": e.get("link"),
                    "release_notes": e.get("release_notes_link"), "description": e.get("description")}
    return None


def _section(entries, component: str, device_name: str, current: Optional[str], host: str) -> Dict[str, Any]:
    """Current vs. essential/recommended for one component, with a verdict like the legacy
    updates window badges: 'essential' outranks 'recommended' outranks 'up_to_date'."""
    essential = _candidate(entries, component, "ESSENTIAL", device_name, host)
    recommended = _candidate(entries, component, "RECOMMENDED", device_name, host)
    verdict = "unknown"
    if current and (essential or recommended):
        verdict = "up_to_date"
        if recommended and not fw.is_newer_or_same(current, recommended["version"]):
            verdict = "recommended"
        if essential and not fw.is_newer_or_same(current, essential["version"]):
            verdict = "essential"
    return {"current": current, "essential": essential, "recommended": recommended, "verdict": verdict}


def check(url: str, device_name: str, firmware_version: Optional[str], sdk_version: Optional[str]) -> Dict[str, Any]:
    """Both sections of the legacy updates window for one device."""
    entries = fetch_entries(url)
    host = fw.platform_name()
    return {
        "source": url,
        "reachable": entries is not None,
        "firmware": _section(entries, "FIRMWARE", device_name, firmware_version, host),
        "software": _section(entries, "LIBREALSENSE", device_name, sdk_version, host),
    }
