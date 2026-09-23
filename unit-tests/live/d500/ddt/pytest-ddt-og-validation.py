# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Not frequently changing, no need to test for each commit

"""DDT occupancy-grid regression: the same stored depth frame through the same pipeline.

The camera replays three recorded cases from `corpus/` and the published occupancy grid is
compared against the golden recorded with it:

  gt              -> must MATCH   (nothing changed)
  depth_changed   -> must DIFFER  (one depth frame edited)
  config_changed  -> must DIFFER  (one configuration field edited)

Everything the replay depends on travels in the case itself - the depth frame, the safety
preset and the depth calibration - so the result is decided by the firmware under test and
not by the scene in front of the camera or by which unit it runs on. The configuration is
backed up before the run and restored afterwards, whatever happens.

The work is done by `ddt_ci_validation.py`, which drives the camera through the
self-contained `ddt.pyz`; nothing has to be installed. The same command can be run by hand:

    cd unit-tests/live/d500/ddt && python3 ddt_ci_validation.py corpus --slot occg_out
"""

import os
import subprocess
import sys

import pytest
import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy.pytest.device_helpers import require_min_fw_version
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D585"),
    pytest.mark.device_exclude("D585S"),   # the safety camera has no DDT core in its image
    pytest.mark.context("nightly"),
    pytest.mark.skipif(sys.platform != "linux", reason="Linux only: filesrc_host is an ELF binary"),
]

HERE = os.path.dirname(os.path.abspath(__file__))
RUNNER = os.path.join(HERE, "ddt_ci_validation.py")
CORPUS = os.path.join(HERE, "corpus")

# The goldens in corpus/gt/expected were recorded on a firmware that publishes the
# occupancy grid as a pure payload, with its attributes in the UVC metadata rather than in
# a MAP1 header. An older firmware publishes a different buffer and cannot match them.
MIN_FW = rsutils.version(7, 59, 46404, 15675)

TIMEOUT_S = 600


def test_ddt_occupancy_grid_replay(test_device):
    device, _ = test_device
    require_min_fw_version(device, MIN_FW, "DDT pure-payload occupancy grid")

    log.info("DDT occupancy replay on %s S/N %s FW %s",
             device.get_info(rs.camera_info.name),
             device.get_info(rs.camera_info.serial_number),
             device.get_info(rs.camera_info.firmware_version))

    r = subprocess.run([sys.executable, RUNNER, CORPUS, "--slot", "occg_out"],
                       cwd=HERE, capture_output=True, text=True, timeout=TIMEOUT_S)
    for line in (r.stdout + r.stderr).splitlines():
        log.info(line)
    if r.returncode != 0:
        pytest.fail(f"DDT occupancy validation failed (exit {r.returncode}):\n"
                    f"{(r.stdout + r.stderr).strip()}")
