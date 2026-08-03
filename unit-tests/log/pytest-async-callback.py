# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import time

import pyrealsense2 as rs
import log_helpers as common


def wait_for(predicate, timeout=5.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline and not predicate():
        time.sleep(0.05)


def test_async_callback_receives_messages(reset_logger):
    received = []
    rs.log_to_callback(rs.log_severity.warn,
                       lambda severity, message: received.append((severity, message)),
                       asynchronous=True)
    common.log_all()
    wait_for(lambda: len(received) >= 2)
    assert len(received) == 2  # warning, error
    assert received[0] == (rs.log_severity.warn, "warn message")
    assert received[1] == (rs.log_severity.error, "error message")


def test_async_and_sync_coexist(reset_logger):
    received = []
    rs.log_to_callback(rs.log_severity.error,
                       lambda severity, message: received.append(message),
                       asynchronous=True)
    rs.log_to_callback(rs.log_severity.error, common.message_counter_2)  # sync
    common.log_all()
    assert common.n_messages_2 == 1  # sync path: delivered before log_all returns
    wait_for(lambda: len(received) >= 1)
    assert received == ["error message"]
