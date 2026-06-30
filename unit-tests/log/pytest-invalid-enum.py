# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
import log_helpers as common


def test_logging_with_invalid_enum(reset_logger):
    rs.log_to_callback( rs.log_severity.debug, common.message_counter )
    assert common.n_messages == 0
    # Following will throw a recoverable_exception, which will issue a log by itself!
    with pytest.raises( RuntimeError, match='invalid enum value for argument "severity"' ):
        rs.log( rs.log_severity( 10 ), "10" )
    # Following does not work in Python 3.6:
    #     TypeError: __init__(): incompatible constructor arguments. The following argument types are supported:
    #         1. pyrealsense2.pyrealsense2.log_severity(value: int)
    #     Invoked with: -1
    # I.e., instead of getting a RuntimeError from the C++, we get a TypeError from Python...
    #with pytest.raises( RuntimeError, match='invalid enum value for argument "severity"' ):
    #    rs.log( rs.log_severity( -1 ), "-1" )
    assert common.n_messages == 1
