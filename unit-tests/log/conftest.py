# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs

import log_helpers


# rs.log_to_callback / log_to_file register handlers on a global C++ singleton
# and the message-counter globals in log_helpers persist across tests in a
# single pytest session. Tests opt in by naming `reset_logger` in their
# signature (same shape as `test_device` in the parent conftest); the legacy
# framework got equivalent isolation for free by running each test in its own
# subprocess.
@pytest.fixture
def reset_logger():
    rs.reset_logger()
    log_helpers.n_messages = 0
    log_helpers.n_messages_2 = 0
    yield
    rs.reset_logger()
