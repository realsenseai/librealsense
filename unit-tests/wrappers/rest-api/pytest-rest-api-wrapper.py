# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import os
import platform
import subprocess
import sys
import logging
import pytest
from rspy import repo

log = logging.getLogger(__name__)

# rest-api only supports Linux x86_64 — its requirements.txt excludes aarch64,
# and there is no Windows port today.
pytestmark = [
    pytest.mark.device("D455"),
    pytest.mark.skipif(
        sys.platform != "linux" or platform.machine() == "aarch64",
        reason="rest-api wrapper supports x86_64 Linux only",
    ),
]


def test_rest_api_wrapper(module_device_setup):
    rest_api_test = os.path.join(repo.root, "wrappers", "rest-api", "tests", "test_api_service.py")
    # The subprocess starts a fresh interpreter, so the parent's sys.path
    # injection of the locally-built pyrealsense2 (unit-tests/conftest.py)
    # is not inherited. Forward it via PYTHONPATH.
    env = os.environ.copy()
    pyrs_dir = repo.find_pyrs_dir()
    if pyrs_dir:
        existing = env.get("PYTHONPATH", "")
        env["PYTHONPATH"] = pyrs_dir + os.pathsep + existing if existing else pyrs_dir
    p = subprocess.run(
        [sys.executable, "-m", "pytest", rest_api_test],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
        timeout=30,
        check=False,
        env=env,
    )
    if p.returncode != 0:
        log.error("Subprocess failed (rc=%s):\n%s", p.returncode, p.stdout)
    else:
        log.debug(p.stdout)
    assert p.returncode == 0
