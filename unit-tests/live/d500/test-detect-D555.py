# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

# test:device D555
# test:donotrun:!dds

import pyrealsense2 as rs
from rspy import test, log

# Make sure D555 is detected on CI machines (DDS connection)
# To run locally with other devices use `--device` flag

with test.closure( "Detect D555 DDS device" ):
    if log.is_debug_on():
        rs.log_to_console( rs.log_severity.debug )
    dev, ctx = test.find_first_device_or_exit()
    is_dds = dev.supports(rs.camera_info.connection_type) and dev.get_info(rs.camera_info.connection_type) == "DDS"
    test.check( is_dds )

test.print_results_and_exit()
