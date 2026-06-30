# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import logging
import pyrealsense2 as rs
import log_helpers as common

log = logging.getLogger(__name__)


def test_double_file_logging(reset_logger, tmp_path):
    filename1 = str( tmp_path / "two-files-1.log" )
    filename2 = str( tmp_path / "two-files-2.log" )
    log.debug( f'Filename1 logging to: {filename1}' )
    log.debug( f'Filename2 logging to: {filename2}' )
    rs.log_to_file( rs.log_severity.error, filename1 )
    rs.log_to_file( rs.log_severity.error, filename2 )

    # Following should log to only the latter!
    common.log_all()

    rs.reset_logger()  # Should flush!
    #el::Loggers::flushAll();   // requires static!

    assert common.count_lines( filename1 ) == 0
    assert common.count_lines( filename2 ) == 1
