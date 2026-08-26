# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import json
from unittest import mock

import pytest

from app.core.errors import RealSenseError
from app.services import firmware
from app.services.firmware import (
    download_firmware,
    is_newer_or_same,
    platform_name,
    _fetch_versions_db,
    _pick_recommended_fw,
)


def test_is_newer_or_same_compares_numerically():
    assert not is_newer_or_same("5.16.0.1", "5.17.0.9")
    assert is_newer_or_same("5.17.0.9", "5.17.0.9")
    assert is_newer_or_same("5.17.0.10", "5.17.0.9")  # 10 > 9, not lexical


def test_is_newer_or_same_lets_unusable_versions_through():
    assert not is_newer_or_same("abc", "5.17.0.9")
    assert not is_newer_or_same(None, "5.17.0.9")
    assert not is_newer_or_same("5.17.0.9", None)


# Exact device names before the wildcards they'd also match, like the real versions DB.
_DB = [
    {"device_name": "Intel RealSense D455", "policy_type": "RECOMMENDED", "component": "FIRMWARE",
     "version": "5.17.3.10", "platform": "*", "link": "d455.bin"},
    {"device_name": "Intel RealSense D4*", "policy_type": "RECOMMENDED", "component": "FIRMWARE",
     "version": "5.17.0.10", "platform": "*", "link": "d4x.bin"},
    {"device_name": "Intel RealSense D4*", "policy_type": "RECOMMENDED", "component": "LIBREALSENSE",
     "version": "2.58.2", "platform": "*", "link": "sw"},
    {"device_name": "Intel RealSense D457", "policy_type": "ESSENTIAL", "component": "FIRMWARE",
     "version": "9.9.9.9", "platform": "*", "link": "x"},
]


def test_pick_prefers_specific_over_wildcard():
    # D455's exact entry (5.17.3.10) precedes the D4* wildcard (5.17.0.10), so it wins
    ver, link = _pick_recommended_fw(_DB, "Intel RealSense D455", "Windows")
    assert (ver, link) == ("5.17.3.10", "d455.bin")


def test_pick_falls_back_to_wildcard():
    ver, link = _pick_recommended_fw(_DB, "Intel RealSense D435", "Windows")
    assert (ver, link) == ("5.17.0.10", "d4x.bin")


def test_pick_ignores_non_firmware_and_non_recommended():
    # LIBREALSENSE component and non-RECOMMENDED policy must never be a FW recommendation
    ver, _ = _pick_recommended_fw(_DB, "Intel RealSense D457", "Windows")
    assert ver == "5.17.0.10"  # matches D4* FIRMWARE/RECOMMENDED, not the ESSENTIAL 9.9.9.9


def test_pick_matches_prefixless_reported_name():
    # SDK reports "RealSense D455" (no "Intel " prefix); DB uses "Intel RealSense D455".
    # The prefix must be stripped on both sides or the proposal never fires.
    ver, link = _pick_recommended_fw(_DB, "RealSense D455", "Windows")
    assert (ver, link) == ("5.17.3.10", "d455.bin")


def test_pick_prefixless_falls_back_to_wildcard():
    ver, _ = _pick_recommended_fw(_DB, "RealSense D435", "Windows")
    assert ver == "5.17.0.10"


def test_pick_no_match_returns_none():
    assert _pick_recommended_fw(_DB, "Some Other Camera", "Windows") == (None, None)
    assert _pick_recommended_fw([], "Intel RealSense D455", "Windows") == (None, None)
    assert _pick_recommended_fw(None, "Intel RealSense D455", "Windows") == (None, None)


_PLATFORM_DB = [
    {"device_name": "Intel RealSense D455", "policy_type": "RECOMMENDED", "component": "FIRMWARE",
     "version": "5.17.3.10", "platform": "Linux arm", "link": "arm.bin"},
]


def test_platform_name_is_versions_db_vocabulary():
    # Whatever the host, it must be a string the DB actually uses (or empty on exotic hosts).
    assert platform_name() in ("Windows amd64", "Windows x86", "Linux amd64", "Linux arm", "Mac OS", "")


def test_pick_requires_exact_platform_match():
    # "Linux" must not match the "Linux arm" entry — the C++ query compares platform strings
    # for equality, and a substring match would pick an image built for another arch.
    assert _pick_recommended_fw(_PLATFORM_DB, "Intel RealSense D455", "Linux amd64") == (None, None)
    assert _pick_recommended_fw(_PLATFORM_DB, "Intel RealSense D455", "Linux arm") == ("5.17.3.10", "arm.bin")


def _fake_urlopen(payload):
    resp = mock.MagicMock()
    resp.read.return_value = json.dumps(payload).encode()
    resp.__enter__.return_value = resp
    return mock.patch("urllib.request.urlopen", return_value=resp)


def test_empty_versions_db_is_not_cached(monkeypatch):
    # An empty list must not be remembered — it would suppress every proposal for the
    # lifetime of the process even after the server recovers.
    monkeypatch.setattr(firmware, "_versions_db_entries", None)
    with _fake_urlopen({"versions": []}):
        assert _fetch_versions_db() == []
    assert firmware._versions_db_entries is None

    with _fake_urlopen({"versions": _DB}):
        assert _fetch_versions_db() == _DB
    assert firmware._versions_db_entries == _DB


def test_download_refuses_to_follow_redirects():
    # The domain allowlist only inspects the URL we are given, so an allowlisted host
    # must not be able to bounce the fetch onward to another address.
    handler = firmware._NoRedirects()
    assert handler.redirect_request(None, None, 302, "Found", {}, "https://10.0.0.1/x.bin") is None


@pytest.mark.parametrize("url", [
    "file:///etc/passwd",
    "/Releases/RS4xx/FW/image.bin",                       # relative — no scheme/host
    "http://librealsense.realsenseai.com/image.bin",      # plaintext
    "https://evil.example/image.bin",
    "https://librealsense.realsenseai.com.evil.example/image.bin",  # suffix look-alike
])
def test_download_rejects_urls_outside_the_versions_db_domain(url):
    with pytest.raises(RealSenseError) as excinfo:
        download_firmware(url)
    assert excinfo.value.status_code == 400
