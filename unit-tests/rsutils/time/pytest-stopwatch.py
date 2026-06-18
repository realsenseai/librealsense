# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2020 RealSense, Inc. All Rights Reserved.

import time
from pytest_check import check
from pyrsutils import stopwatch


TEST_DELTA_TIME_MS = 500
TEST_DELTA_TIME = TEST_DELTA_TIME_MS / 1000  # seconds


def test_stopwatch():
    sw = stopwatch()

    # Verify constructor set the time.
    check.greater( sw.get_start(), 0 )

    # Test elapsed() function
    check.less( sw.get_elapsed(), TEST_DELTA_TIME )

    # Sleep for verifying progress of time
    time.sleep( TEST_DELTA_TIME + 0.1 ) # At GHA scope sleep might be less accurate and wakeup just before TEST_DELTA_TIME have passed

    # Check elapsed() function
    check.greater( sw.get_elapsed(), TEST_DELTA_TIME )

    # Check elapsed_ms() function
    check.greater( sw.get_elapsed_ms(), TEST_DELTA_TIME_MS )

    # Check reset() function
    sw.reset()

    # Verify reset cause the elapsed time to be smaller
    check.less( sw.get_elapsed(), TEST_DELTA_TIME )
    check.less( sw.get_elapsed_ms(), TEST_DELTA_TIME_MS )
