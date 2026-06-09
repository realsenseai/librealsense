# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import importlib.util
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

# import-module-name → pip package name. Used purely to render a readable
# error if a dep is missing on the agent. Jenkins is expected to install
# wrappers/rest-api/requirements.txt + unit-tests/wrappers/rest-api/requirements.txt
# during its Install Requirements stage.
_REQUIRED_MODULES = {
    "fastapi": "fastapi",
    "uvicorn": "uvicorn",
    "pydantic": "pydantic",
    "multipart": "python-multipart",  # FastAPI File/UploadFile route registration
    "aiortc": "aiortc",
    "socketio": "python-socketio",
    "cv2": "opencv-python",
    "numpy": "numpy",
    "httpx": "httpx",  # FastAPI TestClient
}


def _fail_if_missing_packages():
    """Pre-flight: surface a readable error if any required dep is missing.

    Without this, ``test_api_service.py`` collection blows up with a cryptic
    FastAPI route-registration error (e.g. ``Form data requires "python-multipart"``)
    that points into framework internals instead of telling you which install
    step is missing.
    """
    missing = [
        pip_name for mod, pip_name in _REQUIRED_MODULES.items()
        if importlib.util.find_spec(mod) is None
    ]
    if missing:
        pytest.fail(
            "rest-api wrapper: missing required Python package(s): "
            + ", ".join(missing)
            + ".\nInstall via:\n"
            "  pip install -r wrappers/rest-api/requirements.txt\n"
            "  pip install -r unit-tests/wrappers/rest-api/requirements.txt"
        )


def test_rest_api_wrapper(module_device_setup):
    _fail_if_missing_packages()
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
        [sys.executable, "-m", "pytest", "-v", "--log-cli-level=DEBUG", rest_api_test],
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
        log.info(p.stdout)
    assert p.returncode == 0
