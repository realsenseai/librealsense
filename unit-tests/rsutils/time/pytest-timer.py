# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2020 RealSense, Inc. All Rights Reserved.

import time
from pytest_check import check
from pyrsutils import timer


TEST_DELTA_TIME_MS = 500
TEST_DELTA_TIME = TEST_DELTA_TIME_MS / 1000  # seconds

# Test the timer main functions
# Verify the timer expired only when the timeout is reached.
# Verify restart process
def test_timer():
    t = timer( TEST_DELTA_TIME )

    check.is_false( t.has_expired() )

    t.start()
    check.is_false( t.has_expired() )

    time.sleep( TEST_DELTA_TIME + 0.1 )

    # test has_expired() function - expect time expiration
    check.is_true( t.has_expired() )

    # test start() function and verify expiration behavior
    t.start()
    check.is_false( t.has_expired() )

    time.sleep( TEST_DELTA_TIME / 2 )

    # Verify time has not expired yet
    check.is_false( t.has_expired() )

    time.sleep( TEST_DELTA_TIME )

    # Verify time expired
    check.is_true( t.has_expired() )


#############################################################################################
# Verify the we can force the time expiration
def test_force_time_expiration():
    t = timer( TEST_DELTA_TIME )

    check.is_false( t.has_expired() )
    t.set_expired()
    check.is_true( t.has_expired() )
