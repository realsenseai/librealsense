# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import logging
import pyrealsense2 as rs
import log_helpers as common

log = logging.getLogger(__name__)


def test_mixed_file_and_callback_logging(reset_logger, tmp_path):
    filename = str( tmp_path / "mixed-file-callback.log" )
    log.debug( f'Filename logging to: {filename}' )
    rs.log_to_file( rs.log_severity.error, filename )
    rs.log_to_callback( rs.log_severity.debug, common.message_counter )

    common.log_all()

    rs.reset_logger()  # Should flush!
    #el::Loggers::flushAll();   // requires static!

    assert common.count_lines( filename ) == 1
    assert common.n_messages == 4
