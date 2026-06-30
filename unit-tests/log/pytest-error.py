# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pyrealsense2 as rs
import log_helpers as common


def test_log_error(reset_logger):
    rs.log_to_callback( rs.log_severity.error, common.message_counter )
    assert common.n_messages == 0
    common.log_all()
    assert common.n_messages == 1
