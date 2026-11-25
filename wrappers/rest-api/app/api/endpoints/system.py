# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import platform
import subprocess
import sys

from fastapi import APIRouter, HTTPException

router = APIRouter()

_UAC_DECLINED = 1223  # ERROR_CANCELLED


def _enable_metadata_elevated(timeout: float = 120.0) -> int:
    """Spawn an elevated python that calls rs.enable_metadata(). Returns its exit code (1223 = UAC declined).

    Uses .NET ProcessStartInfo directly (via PowerShell) so the `-c "..."` arg
    survives the elevation hop intact.
    """
    script = "import pyrealsense2 as rs; rs.enable_metadata()"
    exe = sys.executable.replace("'", "''")  # PS single-quote escape
    ps = (f"$psi = [Diagnostics.ProcessStartInfo]@{{FileName='{exe}'; "
          f"Arguments='-c \"{script}\"'; "
          f"Verb='runas'; UseShellExecute=$true; WindowStyle='Hidden'}}; "
          f"try {{ $p = [Diagnostics.Process]::Start($psi); $p.WaitForExit(); exit $p.ExitCode }} "
          f"catch {{ $c = $_.Exception.NativeErrorCode; if (-not $c) {{ $c = 1 }}; exit $c }}")
    return subprocess.run(["powershell.exe", "-NoProfile", "-Command", ps], timeout=timeout).returncode


@router.post("/enable_metadata")
def enable_metadata():
    """Enable Windows UVC frame metadata for the connected cameras, behind a UAC prompt: as the SDK's
    enable_metadata, which does nothing elsewhere"""
    if platform.system() != "Windows":
        return
    try:
        rc = _enable_metadata_elevated()
    except subprocess.TimeoutExpired:
        raise HTTPException(504, "Metadata enable timed out (user did not approve UAC?).")
    if rc == _UAC_DECLINED:
        raise HTTPException(400, "UAC elevation was declined.")
    if rc:
        raise HTTPException(500, f"Metadata registry write failed (exit {rc}).")
