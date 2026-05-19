# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import logging
import pyrealsense2 as rs
import log_helpers as common

log = logging.getLogger(__name__)


def test_rolling_logger_max_1m(reset_logger, tmp_path):
    log_filename = str( tmp_path / 'rolling.log' )
    rs.log_to_file( rs.log_severity.info, log_filename )

    max_size = 1  # MB
    rs.enable_rolling_log_file( max_size )

    for i in range( 15000 ):
        rs.log( rs.log_severity.info, f'debug message {i}' )
    rs.reset_logger()

    with open( log_filename, "rb" ) as log_file:
        log_file.seek( 0, 2 )  # 0 bytes from end
        log_size = log_file.tell()
    del log_file
    log.debug( f'{log_filename} size: {log_size}' )

    old_filename = log_filename + ".old"
    with open( old_filename, "rb" ) as old_file:
        old_file.seek( 0, 2 )  # 0 bytes from end
        old_size = old_file.tell()
    del old_file
    log.debug( f'{old_filename} size: {old_size}' )

    max_size_in_bytes = max_size * 1024 * 1024
    size = log_size + old_size
    assert size <= 2 * max_size_in_bytes
