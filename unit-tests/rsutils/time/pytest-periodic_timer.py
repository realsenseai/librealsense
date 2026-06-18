# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2020 RealSense, Inc. All Rights Reserved.

import time
from pytest_check import check
from pyrsutils import periodic_timer


TEST_DELTA_TIME_MS = 500
TEST_DELTA_TIME = TEST_DELTA_TIME_MS / 1000  # seconds

# Verify the timer expired and restart itself
def test_periodic_time_expiration():
    pt = periodic_timer( TEST_DELTA_TIME )
    for i in range( 5 ):
        check.is_false( bool( pt ) )
        time.sleep( TEST_DELTA_TIME + 0.1 ) # At GHA scope sleep might be less accurate and wakeup just before TEST_DELTA_TIME have passed
        check.is_true( bool( pt ) )

    check.is_false( bool( pt ) )

# Verify the we can force the time expiration
def test_force_time_expiration():
    pt = periodic_timer( TEST_DELTA_TIME )

    check.is_false( bool( pt ) )
    pt.set_expired()
    check.is_true( bool( pt ) )
