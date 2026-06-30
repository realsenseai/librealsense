# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Counterpart of pytest-env-log-level-on.py: re-execute this file as a fresh
# interpreter with LRS_LOG_LEVEL explicitly removed, so the result is
# independent of how the parent pytest was invoked.

import os
import subprocess
import sys

import pyrealsense2 as rs
import log_helpers as common


def test_without_lrs_log_level():
    from rspy import repo  # local — child does not need rspy
    env = {**os.environ}
    env.pop( 'LRS_LOG_LEVEL', None )
    pyrs_dir = repo.find_pyrs_dir()
    existing = env.get( 'PYTHONPATH', '' )
    env['PYTHONPATH'] = ( pyrs_dir + os.pathsep + existing ) if existing else pyrs_dir
    subprocess.run( [sys.executable, __file__], env=env, check=True, timeout=30 )


if __name__ == '__main__':
    rs.log_to_callback( rs.log_severity.error, common.message_counter )
    common.log_all()
    # Default minimum severity is error → callback sees exactly 1 message.
    assert common.n_messages == 1
