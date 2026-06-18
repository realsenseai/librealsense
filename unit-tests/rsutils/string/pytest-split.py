# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2020 RealSense, Inc. All Rights Reserved.

from pyrsutils import split
from pytest_check import check


#############################################################################################
#
def test_split():
    check.equal( len( split( "" , '\n' )), 0 )
    check.equal( len( split( "abc" , '\n' )), 1 )
    check.equal( len( split( "abc\nabc" , '\n' )), 2 )

    check.equal( split( "a\nbc\nabc"   , '\n' ), ['a', 'bc', 'abc' ] )
    check.equal( split( "a\nbc\nabc\n" , '\n' ), ['a', 'bc', 'abc' ] )

    check.equal( split( "1-12-123-1234" , '-' ), ['1', '12', '123', '1234' ] )
