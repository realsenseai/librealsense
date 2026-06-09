# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

import logging
log = logging.getLogger(__name__)
from pytest_check import check
from pyrsutils import running_average_i as running_average
import random

random.seed()  # seed random number generator


def signed( n ):
    s = str(n)
    if n >= 0:
        s = '+' + s
    return s


def _test_set( median, plus_minus, reps = 50 ):
    avg = running_average()
    tot = 0
    log.debug( f" #  value        | average                                                               | expected" )
    for r in range(reps):
        d = random.random()  # [0,1)
        d -= 0.5  # [-0.5,0.5)
        d *= 2    # [-1,1)
        d *= plus_minus
        d += median
        d = int(d)
        prev = avg.get()
        lo = avg.leftover()
        avg.add( d )
        tot += d
        rounding = int(avg.size() / 2)
        if prev + lo < 0:
            rounding = -rounding
        log.debug( f"{avg.size():>3} {d:>12} | {str(prev)+'+('+signed(d-prev)+signed(lo)+signed(rounding)+')/'+str(avg.size()):>35}= {str(avg.get())+signed(avg.leftover()):>15}= {avg.get_double():>15.2f} | {tot / avg.size():>12.2f}" )
    check.equal( avg.size(), reps )
    golden = tot / avg.size()
    check.almost_equal( avg.get(), golden, abs=1. )
    # We have higher expectations! We should get a pretty nice match:
    check.almost_equal( float( avg ), golden, abs=.001 )
    log.debug( '' )


def _test_around( median, plus_minus, reps = 50, sets = 10 ):
    """
    Generate random numbers around the median, keeping within 'plus_minus' of it.
    Test that, at the end, the running-average is the same as the average we calculate manually (sum(numbers)/count).
    :param median: the middle number around which we pick numbers
    :param plus_minus: how far away from the median we want to get
    :param reps: how many numbers per set of numbers
    :param sets: how many times to repeat this
    """
    for s in range(sets):
        # in case we want to reproduce, you can take the output, a tuple:
        #     random.setstate( (3, (...), None) )
        #     test_set(
        log.debug( f'random.setstate( {random.getstate()} )  &&  test_set( {median}, {plus_minus}, {reps} )' )
        _test_set( median, plus_minus, reps )


#############################################################################################
#
# These use random numbers, so the output will be different each time...
# See the above debug output if something needs to be reproduced.
#
def test_around():
    _test_around( 5000, 100 )  # positive, small range
    _test_around( 100, 99 )    # positive, range large (in comparison)
    _test_around( 0, 100 )     # positive & negative
    _test_around( -100, 150 )  # more negative
    _test_around( -10, 5 )     # negative, small range
    _test_around( 100000000, 50000000 )  # large numbers
    _test_around( 0, 50000000 )  # with negatives
