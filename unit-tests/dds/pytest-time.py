# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
from pytest_check import check
import pyrealdds as dds

pytestmark = [
    pytest.mark.dds,
]

def test_default_ctor():
    check.equal( dds.time().seconds, 0 )
    check.equal( dds.time().nanosec, 0 )

def test_seconds_nanoseconds():
    check.equal( dds.time( 1, 2 ).seconds, 1 )
    check.equal( dds.time( 1, 2 ).nanosec, 2 )

def test_limits():
    check.raises( TypeError, lambda: dds.time( 0xffffffff, 0 ) )
    check.raises( TypeError, lambda: dds.time( 0, 0x100000000 ) )
    check.raises( ValueError, lambda: repr( dds.time( 0, 0xffffffff )) )  # nsec > 9 digits don't make sense...
    check.raises( ValueError, lambda: repr( dds.time( 0, 1000000000 )) )
    dds.time( 0, 0xffffffff )  # invalid, but not checked on construction!
    dds.time( 0, 999999999 )  # valid
    # infinite time is 0x7fffffff, 0xffffffff, but is not represented yet
    check.raises( ValueError, lambda: repr( dds.time( 0x7fffffff, 0xffffffff )) )

def test_nanoseconds():
    check.equal( dds.time().to_ns(), 0 )
    check.equal( dds.time( 0 ).to_ns(), 0 )               # nsec
    check.equal( dds.time( 1 ).to_ns(), 1 )
    check.equal( dds.time( 1, 2 ).to_ns(),  1000000002 )  # sec, nsec

def test_double_is_inexact():
    check.equal( dds.time().to_double(), 0. )
    check.equal( dds.time( 1.1 ).to_double(), 1.1 )
    check.equal( dds.time( 1.1 ).to_ns(),   1100000000 )  # double is ~sec.nsec
    check.equal( dds.time( 1.001 ).to_ns(), 1000999999 )  # but inexact!
    check.equal( dds.time( 1682486031.2499189 ).to_ns(), 1682486031249918937 )    # !?
    check.equal( dds.time( 1682486031.2499189 ).to_double(), 1682486031.249919 )  # !?

def test_negatives_do_not_work():
    # NOTE:
    # RTPS, and therefore DDS, time is using the NTP representation, according to RTPS 9.3.2.2.
    # This means it's not really meant to convey negative values: in fact, RTPS defines Time_t
    # as unsigned; it's Duration_t that's signed. Even then, only the seconds can be negative,
    # so -1.1 seconds should really be encoded as -2 seconds, plus 0.9, or -2.9!
    check.equal( dds.time( -1, 0 ).to_ns(), -1000000000 )  # -1 seconds is fairly simple
    check.equal( repr( dds.time( -1, 0 )), '-1.0' )
    check.raises( TypeError, lambda: dds.time( 0, -1 ) )   # nanosec is unsigned
    check.equal( repr( dds.time( -1 )), '-1.999999999' )   # ?!
    check.equal( dds.time( -0, 1 ).to_ns(), 1 )            # doesn't work - this is 1ns!
    check.equal( dds.time( -1, 1 ).to_ns(), -999999999 )   # nope
    check.equal( dds.time( -1, 999999999 ).to_ns(), -1 )   # yep!
    check.equal( repr( dds.time( -1, 999999999 )), '-1.999999999' )  # Oh boy...

def test_operators():
    check.equal( dds.time.from_double( 1.1 ), dds.time( 1.1 ))
    check.is_false( dds.time.from_double( 1.1 ) == 1.1 )
    check.is_true( dds.time.from_double( 1.1 ) != 1.1 )
    check.is_false( dds.time.from_double( 1.1 ) != dds.time( 1.1 ))
    check.raises( TypeError, lambda: dds.time( 1 ) < dds.time( 2 ) )    # '<' not supported
    check.raises( TypeError, lambda: dds.time( 2 ) > dds.time( 2 ) )

def test_string_repr():
    check.raises( TypeError, lambda: dds.time( '0.0' ) )  # no string parsing
    check.equal( str( dds.time() ), '0.0' )               # repr is same as to_double()
    check.equal( repr( dds.time() ), '0.0' )
    check.equal( str( dds.time( 1.1 )), '1.1' )
