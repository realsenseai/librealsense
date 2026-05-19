# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# LRS_LOG_LEVEL is read by the C++ logger only at first import of pyrealsense2
# (src/log.h `try_get_log_severity`). Pyrealsense2 is already imported by the
# parent pytest, so we re-execute this file as a fresh interpreter with
# LRS_LOG_LEVEL=WARN set in advance. The bottom __main__ block is the actual
# check; the test function just spawns it.

import os
import subprocess
import sys

import pyrealsense2 as rs
import log_helpers as common


def test_with_lrs_log_level_warn():
    from rspy import repo  # local — child does not need rspy
    env = {**os.environ}
    env['LRS_LOG_LEVEL'] = 'WARN'
    # The parent's sys.path no longer contains pyrs_dir by the time tests run
    # (rspy.devices.init_hub removes it), so look it up explicitly. Same pattern
    # as unit-tests/wrappers/rest-api/pytest-rest-api-wrapper.py.
    pyrs_dir = repo.find_pyrs_dir()
    existing = env.get( 'PYTHONPATH', '' )
    env['PYTHONPATH'] = ( pyrs_dir + os.pathsep + existing ) if existing else pyrs_dir
    subprocess.run( [sys.executable, __file__], env=env, check=True, timeout=30 )


if __name__ == '__main__':
    rs.log_to_callback( rs.log_severity.error, common.message_counter )
    common.log_all()
    # Without LRS_LOG_LEVEL the result would be 1 (error).
    # With LRS_LOG_LEVEL=WARN the threshold is forced to warning, so 2 (warning + error).
    assert common.n_messages == 2
