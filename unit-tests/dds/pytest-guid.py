# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
from pytest_check import check
import pyrealdds as dds

pytestmark = [
    pytest.mark.dds,
]

def test_default_ctor():
    check.is_false( dds.guid() )

def test_from_string_eprosima_format():
    check.equal( repr( dds.guid.from_string( '112233445566.5.100' )), '112233445566.5.100' )
    check.is_false( dds.guid.from_string( '1122334455.5.100' ) )
    check.is_false( dds.guid.from_string( '00112233445566778899001122.5.100' ) )
    check.equal( repr( dds.guid.from_string( 'aabbccddeeff.5.100' ) ), 'aabbccddeeff.5.100' )  # hex digits
    check.equal( repr( dds.guid.from_string( '112233445566.b.100' ) ), '112233445566.b.100' )
    check.equal( repr( dds.guid.from_string( '112233445566.5.10a' ) ), '112233445566.5.10a' )
    check.equal( repr( dds.guid.from_string( 'aaBBccDDeeFF.5.100' ) ), 'aabbccddeeff.5.100' )  # uppercase OK, but output is lowercase
    check.equal( repr( dds.guid.from_string( '112233445566.CDE.100' ) ), '112233445566.cde.100' )
    check.equal( repr( dds.guid.from_string( '112233445566.5.1A0' ) ), '112233445566.5.1a0' )
    check.is_false( dds.guid.from_string( '112233g45566.5.100' ) )  # invalid
    check.is_false( dds.guid.from_string( '112233445566.U.100' ) )
    check.is_false( dds.guid.from_string( '112233445566.5.1X0' ) )
    check.is_false( dds.guid.from_string( '112233445566.-1.100' ) )
    check.is_false( dds.guid.from_string( '112233445566.0.100-' ) )

def test_from_string_generic_format():
    check.is_false( dds.guid.from_string( '' ) )
    check.equal( repr( dds.guid.from_string( '001122334455667788990011.100' ) ), '001122334455667788990011.100' )
    check.equal( repr( dds.guid.from_string( '010f22334455667788990000.100' ) ), '223344556677.9988.100' )  # eProsima vendor ID
