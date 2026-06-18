# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2020 RealSense, Inc. All Rights Reserved.

import pytest
from pytest_check import check
from pyrsutils import version


#############################################################################################
#
def test_string_constructor():
    check.is_false( version() )

    check.is_false( version( "" ))
    check.is_false( version( "1" ))
    check.is_false( version( "1." ))
    check.is_false( version( "1.2" ))
    check.is_false( version( "1.2." ))
    check.is_true  (     version( "1.2.3" ))
    check.is_false( version( "1.2.3." ))
    check.is_true  (     version( "1.2.3.4" ))
    check.is_false( version( "1.2.3.4." ))
    check.is_false( version( "1 . 2.3.4" ))
    check.is_false( version( ".1.2.3.4" ))
    check.is_false( version( "0.0.0.0" ))
    check.is_true  (     version( "1.0.0.0" ))
    check.is_true  (     version( "0.1.0.0" ))
    check.is_true  (     version( "0.0.1.0" ))
    check.is_true  (     version( "0.0.0.1" ))
    check.is_false( version( ".2.3.4" ))
    check.is_false( version( "1..2.4" ))
    check.is_false( version( "1.2..4" ))

    check.is_true  (     version( "65535.2.3.4" ))
    check.is_false( version( "65536.2.3.4" ))
    check.is_true  (     version( "1.65535.3.4" ))
    check.is_false( version( "1.65536.3.4" ))
    check.is_true  (     version( "1.2.65535.4" ))
    check.is_false( version( "1.2.65536.4" ))
    check.is_true  (     version( "1.2.3.65535" ))
    check.is_false( version( "1.2.3.65536" ))

    check.is_false( version( "xxxxxxxxxxx" ))


#############################################################################################
#
def test_number_constructor():
    check.is_true( not version.from_number(0) )
    check.is_true( version( 1, 2, 3 ))
    check.is_true( version( 1, 2, 3, 4 ))
    with pytest.raises( TypeError ):
        version( -1, 2, 3, 4 )
    with pytest.raises( TypeError ):
        version( 1, -2, 3, 4 )
    with pytest.raises( TypeError ):
        version( 1, 2, -3, 4 )
    with pytest.raises( TypeError ):
        version( 1, 2, 3, -4 )
    check.is_true( version( 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF ))
    with pytest.raises( TypeError ):
        version( 0x10000, 0xFFFF, 0xFFFF, 0xFFFF )
    with pytest.raises( TypeError ):
        version( 0xFFFF, 0x10000, 0xFFFF, 0xFFFF )
    with pytest.raises( TypeError ):
        version( 0xFFFF, 0xFFFF, 0x10000, 0xFFFF )
    with pytest.raises( TypeError ):
        version( 0xFFFF, 0xFFFF, 0xFFFF, 0x10000 )

    v1234 = version.from_number( version( 1, 2, 3, 1234 ).number )
    check.equal( v1234.major(), 1 )
    check.equal( v1234.minor(), 2 )
    check.equal( v1234.patch(), 3 )
    check.equal( v1234.build(), 1234 )


#############################################################################################
#
def test_comparisons():
    v0 = version()
    vN = version.from_number( 281483566843090 )
    v1233 = version( "1.2.3.1233" )
    v1234 = version( "1.2.3.1234" )
    v1235 = version( "1.2.3.1235" )

    check.equal( v0, v0 )
    check.equal( v0, version() )
    check.equal( v0, version('') )
    check.equal( v0, version.from_number(0) )
    check.equal( v0, version( "0.0.0.0" ) )
    check.equal( v0, version( "123" ) )
    check.is_true      ( vN != v0 )

    check.equal( vN, v1234 )
    check.is_true      ( v1234 == v1234 )
    check.is_false( v1234 != v1234 )
    check.is_false( v1234 == v1235 )
    check.is_true      ( v1234 != v1235 )

    check.equal( version( 1, 2, 3 ), version( "1.2.3" ) )
    check.is_true( version( "1.2.3.1234" ) != version( "1.2.3" ) )


#############################################################################################
#
def test_leading_zeroes():
    check.is_true( str(version( "01.02.03.04" )) == "1.2.3.4" )
    check.is_true(     version( "01.0002.00000000000000003.0000000000000004" ))


#############################################################################################
#
def test_operator_gt():
    check.is_true( version( "1.2.3.4" ) > version( "0.2.3.4" ))
    check.is_true( version( "1.2.3.4" ) > version( "1.1.3.4" ))
    check.is_true( version( "1.2.3.4" ) > version( "1.2.2.4" ))
    check.is_true( version( "1.2.3.4" ) > version( "1.2.3.3" ))

    check.is_false( version( "1.2.3.4" ) > version( "2.2.3.4" ))
    check.is_false( version( "1.2.3.4" ) > version( "1.3.3.4" ))
    check.is_false( version( "1.2.3.4" ) > version( "1.2.4.4" ))
    check.is_false( version( "1.2.3.4" ) > version( "1.2.3.5" ))
    check.is_false( version( "1.2.3.4" ) > version( "1.2.3.4" ))


#############################################################################################
#
def test_operator_lt():
    check.is_true( version( "1.2.3.4" ) < version( "2.2.3.4" ))
    check.is_true( version( "1.2.3.4" ) < version( "1.3.3.4" ))
    check.is_true( version( "1.2.3.4" ) < version( "1.2.4.4" ))
    check.is_true( version( "1.2.3.4" ) < version( "1.2.3.5" ))

    check.is_false( version( "1.2.3.4" ) < version( "0.2.3.4" ))
    check.is_false( version( "1.2.3.4" ) < version( "1.1.3.4" ))
    check.is_false( version( "1.2.3.4" ) < version( "1.2.2.4" ))
    check.is_false( version( "1.2.3.4" ) < version( "1.2.3.3" ))

    check.is_false( version( "1.2.3.4" ) < version( "1.2.3.4" ))


#############################################################################################
#
def test_operator_ge():
    check.is_true( version( "1.2.3.4" ) >= version( "1.2.3.4" ))

    check.is_true( version( "2.2.3.4" ) >= version( "1.2.3.4" ))
    check.is_true( version( "1.3.3.4" ) >= version( "1.2.3.4" ))
    check.is_true( version( "1.2.4.4" ) >= version( "1.2.3.4" ))
    check.is_true( version( "1.2.3.5" ) >= version( "1.2.3.4" ))

    check.is_false( version( "1.2.3.4" ) >= version( "2.2.3.4" ))
    check.is_false( version( "1.2.3.4" ) >= version( "1.3.3.4" ))
    check.is_false( version( "1.2.3.4" ) >= version( "1.2.4.4" ))
    check.is_false( version( "1.2.3.4" ) >= version( "1.2.3.5" ))


#############################################################################################
#
def test_operator_le():
    check.is_true( version( "1.2.3.4" ) <= version( "1.2.3.4" ))

    check.is_true( version( "1.2.3.4" ) <= version( "2.2.3.4" ))
    check.is_true( version( "1.2.3.4" ) <= version( "1.3.3.4" ))
    check.is_true( version( "1.2.3.4" ) <= version( "1.2.4.4" ))
    check.is_true( version( "1.2.3.4" ) <= version( "1.2.3.5" ))

    check.is_false( version( "2.2.3.4" ) <= version( "1.2.3.4" ))
    check.is_false( version( "1.3.3.4" ) <= version( "1.2.3.4" ))
    check.is_false( version( "1.2.4.4" ) <= version( "1.2.3.4" ))
    check.is_false( version( "1.2.3.5" ) <= version( "1.2.3.4" ))


#############################################################################################
#
def test_is_between():
    v1 = version( "01.02.03.04" )
    v2 = version( "01.03.03.04" )
    v3 = version( "02.01.03.04" )

    check.is_true( v2.is_between( v1, v3 ))
    check.is_true( v3.is_between( v1, v3 ))
    check.is_true( v2.is_between( v2, v3 ))
    check.is_true( v2.is_between( v2, v2 ))


#############################################################################################
#
def test_without_build():
    check.equal( version(1,2,3,1234).without_build(), version(1,2,3))


#############################################################################################
#
def test_string_conversion():
    check.equal( str(version(1,2,3,4)), "1.2.3.4" )
    check.equal( str(version(1,2,3,0)), "1.2.3" )
    check.equal( str(version(1,2,0,0)), "1.2.0" )
