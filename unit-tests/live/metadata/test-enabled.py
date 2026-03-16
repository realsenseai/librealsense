# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

#test:device each(D400*)
#test:device each(D500*)
#Setting priority to run after test-fw-update
#test:priority 2
#test:flag windows

import pyrealsense2 as rs
from rspy import test, log

test.start("checking metadata is enabled")
dev, _ = test.find_first_device_or_exit()
test.check( dev.is_metadata_enabled() )

test.finish()
test.print_results_and_exit()
