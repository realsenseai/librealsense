# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import os
import platform
import shutil
import socket
import subprocess
import sys
import logging
import pytest
from rspy import repo

log = logging.getLogger(__name__)

# Browser + server + real camera. This wrapper owns the camera; Playwright owns the server
# and the browser (see the viewer's playwright.config.ts).
pytestmark = [
    pytest.mark.device("D455"),
    pytest.mark.skipif(
        sys.platform != "linux" or platform.machine() == "aarch64",
        reason="rest-api wrapper supports x86_64 Linux only",
    ),
]

_SERVER_DIR = os.path.join(repo.root, "wrappers", "rest-api")
_VIEWER_DIR = os.path.join(_SERVER_DIR, "tools", "react-viewer")

# Playwright gives up first so a slow run still writes its summary; the marker is the
# backstop and must never fire, as conftest's thread method kills the whole run.
_PW_GLOBAL_MS = 600_000
_SUBPROCESS_TIMEOUT = 700
_MARKER_TIMEOUT = 1200


def _free_port():
    """The viewer builds its URLs from window.location, so any port works -- taking a free
    one means a leftover server or a colleague on :8000 cannot collide with the run."""
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def _fail_if_not_ready_to_run():
    # jammy's apt node is 12; @playwright/test does not check at runtime
    node = shutil.which("node") and subprocess.run(
        ["node", "-p", "process.versions.node.split('.')[0]"],
        stdout=subprocess.PIPE, universal_newlines=True).stdout.strip()
    if not node or int(node) < 22:
        pytest.fail(f"react-viewer e2e: need Node 22+, found {node or 'none'}. LibCI installs it "
                    "in 'Install Requirements'; locally use deb.nodesource.com/setup_22.x")

    # The server only mounts the viewer when a build is present, so without this the
    # suite would run against bare API 404s instead of the app.
    if not os.path.isfile(os.path.join(_SERVER_DIR, "static", "index.html")):
        pytest.fail("react-viewer e2e: the viewer is not built.\n"
                    "  cd wrappers/rest-api/tools/react-viewer && npm run build && npm run bundle")


# Playwright marks results with ✓/✘. The log holds them as valid UTF-8, but nothing in the
# file says so, and Jenkins' artifact viewer guesses a single-byte charset and shows mojibake.
_GLYPHS = str.maketrans({"✓": "ok", "✔": "ok", "✘": "FAIL", "✗": "FAIL",
                         "×": "x", "›": ">", "─": "-", "│": "|",
                         "…": "..."})


@pytest.mark.timeout(_MARKER_TIMEOUT)
def test_react_viewer_e2e(module_device_setup, request):
    _fail_if_not_ready_to_run()

    env = os.environ.copy()
    env.update(API_URL=f"http://127.0.0.1:{_free_port()}",
               PYTHON_BIN=sys.executable,
               DEVICE_SERIAL=module_device_setup)

    # Per attempt: playwright empties its output dir on startup, so a rerun would wipe the
    # trace of the failure that caused it -- exactly the one worth keeping.
    attempt = getattr(request.node, "execution_count", 1)
    traces = os.path.join(request.config._test_logdir, "react-viewer-e2e", f"attempt-{attempt}")
    command = ["npm", "run", "test:e2e", "--",
               "--grep", "@real-device",
               f"--global-timeout={_PW_GLOBAL_MS}",
               f"--output={traces}"]

    try:
        p = subprocess.run(command, cwd=_VIEWER_DIR, env=env,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           universal_newlines=True, timeout=_SUBPROCESS_TIMEOUT, check=False)
    except subprocess.TimeoutExpired as e:
        log.error("playwright timed out after %ss:\n%s", _SUBPROCESS_TIMEOUT,
                  (e.stdout or "").translate(_GLYPHS))
        raise

    output = p.stdout.translate(_GLYPHS)
    if p.returncode:
        log.error("playwright failed (rc=%s):\n%s", p.returncode, output)
    else:
        log.info(output)
    assert p.returncode == 0
