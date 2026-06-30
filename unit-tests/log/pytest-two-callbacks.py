# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pyrealsense2 as rs
import log_helpers as common


def test_logging_to_two_callbacks(reset_logger):
    rs.log_to_callback( rs.log_severity.error, common.message_counter )
    rs.log_to_callback( rs.log_severity.error, common.message_counter_2 )
    assert common.n_messages == 0
    assert common.n_messages_2 == 0
    common.log_all()
    assert common.n_messages == 1
    assert common.n_messages_2 == 1
