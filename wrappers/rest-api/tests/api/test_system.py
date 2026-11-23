# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import subprocess

import pytest
from unittest.mock import MagicMock, patch

from .conftest import client


def test_enable_metadata_noop_on_non_windows():
    with patch("app.api.endpoints.system.platform.system", return_value="Linux"):
        response = client.post("/api/v1/system/enable_metadata")
    assert (response.status_code, response.json()) == (200, None)


@pytest.mark.parametrize(
    "returncode,status,detail",
    [
        (0, 200, None),
        (1223, 400, "declined"),
        (99, 500, "exit 99"),
        (subprocess.TimeoutExpired(cmd="powershell.exe", timeout=120), 504, None),
    ],
    ids=["ok", "declined", "failure", "timeout"],
)
def test_enable_metadata_windows(returncode, status, detail):
    mock_run = MagicMock()
    if isinstance(returncode, Exception):
        mock_run.side_effect = returncode
    else:
        mock_run.return_value.returncode = returncode

    with patch("app.api.endpoints.system.platform.system", return_value="Windows"), \
         patch("app.api.endpoints.system.subprocess.run", mock_run):
        response = client.post("/api/v1/system/enable_metadata")
    assert response.status_code == status
    if detail:
        assert detail in response.json()["detail"]
