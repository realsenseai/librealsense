# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Online versions-DB lookup and firmware image download.

No pyrealsense2 dependency, so tests can import it without loading the SDK. Images are
downloaded into memory and flashed from there — no temp files.
"""

import json
import logging
import platform
import re
import sys
import urllib.parse
import urllib.request
from typing import Optional

from app.core.errors import RealSenseError

SERVER_VERSIONS_DB_URL = "https://librealsense.realsenseai.com/Releases/rs_versions_db.json"
_MAX_FW_DOWNLOAD_BYTES = 64 * 1024 * 1024
# Firmware may only be fetched from the domain that serves the versions DB itself, so a
# tampered/mistaken `link` can't turn the backend into a fetcher for arbitrary hosts.
_FW_DOWNLOAD_DOMAIN = ".".join(urllib.parse.urlparse(SERVER_VERSIONS_DB_URL).hostname.split(".")[-2:])

# Fetched once per process (no TTL — re-downloaded on backend restart, like the C++ viewer).
_versions_db_entries = None


class _NoRedirects(urllib.request.HTTPRedirectHandler):
    """Refuse to follow redirects, so the domain check can't be sidestepped by a 3xx.

    Returning None leaves urllib to raise HTTPError for the redirect status.
    """

    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


_fw_download_opener = urllib.request.build_opener(_NoRedirects)


def is_newer_or_same(current: Optional[str], recommended: Optional[str]) -> bool:
    """Numeric version compare. Unparseable versions answer False, so a flash isn't
    refused over a version string we don't understand."""
    def parse(v):
        try:
            return tuple(int(p) for p in (v or "").split("."))
        except ValueError:
            return None

    cur, rec = parse(current), parse(recommended)
    return bool(cur and rec) and cur >= rec


def _device_name_matches(db_name, device_name):
    """True if a DB device_name pattern ('*' = wildcard) matches the reported name.

    The legacy 'Intel ' prefix is stripped from both sides: newer SDKs report names
    without it while the DB still carries it (mirrors is_device_name_equal).
    """
    pattern = re.escape((db_name or "").removeprefix("Intel ")).replace(r"\*", ".*")
    return re.fullmatch(pattern, (device_name or "").removeprefix("Intel ")) is not None


def platform_name():
    """Host name in the versions-DB vocabulary (mirrors rsutils::os::get_platform_name).

    The DB only ever carries "Windows amd64", "Windows x86", "Linux amd64", "Linux arm",
    "Mac OS" or "*", so platform.system() alone ("Darwin", no arch) would never match.
    """
    system = platform.system()
    if system == "Windows":
        return "Windows amd64" if sys.maxsize > 2 ** 32 else "Windows x86"
    if system == "Darwin":
        return "Mac OS"
    if system == "Linux":
        machine = platform.machine().lower()
        return "Linux arm" if machine.startswith(("arm", "aarch64")) else "Linux amd64"
    return ""


def _pick_recommended_fw(entries, device_name, host_platform):
    """First matching RECOMMENDED FIRMWARE (version, link) for a device, or (None, None).

    First match wins, like the C++ query_versions: the DB lists exact device names ahead
    of the wildcard patterns they'd also match.
    """
    for e in entries or []:
        if (e.get("component") == "FIRMWARE" and e.get("policy_type") == "RECOMMENDED"
                and (e.get("platform") or "*") in ("*", host_platform)
                and _device_name_matches(e.get("device_name"), device_name)):
            return e.get("version"), e.get("link")
    return None, None


def _fetch_versions_db():
    """Return the DB 'versions' list (cached for the process lifetime), or None on failure."""
    global _versions_db_entries
    if _versions_db_entries is not None:
        return _versions_db_entries
    try:
        with urllib.request.urlopen(SERVER_VERSIONS_DB_URL, timeout=5) as resp:
            data = json.loads(resp.read().decode("utf-8"))
    except Exception as e:  # network down, timeout, bad JSON — degrade gracefully
        logging.warning("Could not fetch firmware versions DB: %s", e)
        return None
    entries = data.get("versions") or []
    # Only cache a non-empty DB: an empty list (CDN hiccup, wrong endpoint) would
    # otherwise be served for the process lifetime and suppress every proposal.
    if entries:
        _versions_db_entries = entries
    return entries


def recommended_firmware(device_name):
    """Recommended FIRMWARE (version, link) for a device from the online DB, or (None, None)."""
    return _pick_recommended_fw(_fetch_versions_db(), device_name, platform_name())


def download_firmware(url: str, on_progress=None) -> bytes:
    """Download a firmware .bin into memory, reporting 0..1 progress via on_progress().

    Restricted to https on the versions DB's own domain, redirects refused, so a tampered
    link can't point the fetch elsewhere.
    """
    parsed = urllib.parse.urlparse(url)
    host = parsed.hostname or ""
    if parsed.scheme != "https" or not (host == _FW_DOWNLOAD_DOMAIN or host.endswith("." + _FW_DOWNLOAD_DOMAIN)):
        raise RealSenseError(
            status_code=400,
            detail=f"Refusing to download firmware from outside https://*.{_FW_DOWNLOAD_DOMAIN}",
        )
    reported = 0.0
    try:
        buf = bytearray()
        with _fw_download_opener.open(url, timeout=30) as resp:
            total = int(resp.headers.get("Content-Length") or 0)
            while True:
                chunk = resp.read(256 * 1024)
                if not chunk:
                    break
                buf.extend(chunk)
                if len(buf) > _MAX_FW_DOWNLOAD_BYTES:
                    raise RealSenseError(status_code=413, detail="Firmware image exceeds size limit")
                if on_progress and total:
                    reported = min(len(buf) / total, 1.0)
                    on_progress(reported)
        data = bytes(buf)
    except RealSenseError:
        raise
    except Exception as e:
        raise RealSenseError(status_code=502, detail=f"Failed to download firmware: {e}")
    if not data:
        raise RealSenseError(status_code=502, detail="Downloaded firmware image is empty")
    if on_progress and reported < 1.0:
        on_progress(1.0)
    return data
