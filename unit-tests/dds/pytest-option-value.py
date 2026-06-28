# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import re
import pyrealdds as dds
from pytest_check import check

pytestmark = [
    pytest.mark.dds,
]

# Test dds.option mechanism and logic

def test_read_only_options():
    check.is_true( dds.option.from_json( ['1', 0, 'desc'] ).is_read_only() )
    check.is_false( dds.option.from_json( ['1', 0, 0, 'desc'] ).is_read_only() )  # default value -> not read-only
    check.is_true( dds.option.from_json(
        ['Asic Temperature',None,-40.0,125.0,0.0,0.0,'Current Asic Temperature (degree celsius)',['optional','read-only']] ).is_read_only() )
    check.equal( dds.option.from_json(  # default==min==max, step==0 -> read-only!
        ['Stereo Baseline',50.20994567871094,50.20994567871094,50.20994567871094,0.0,50.20994567871094,'...',['read-only']] ).to_json(),
        ['Stereo Baseline',50.20994567871094,'...'] )
    check.equal(  # 1.1 => 1.10000002 as float, => 1.1000000000000001 as double
        dds.option.from_json( ['a', 1.1, 'desc'] ).get_value(),
        1.1 )

def test_boolean():
    check.equal( dds.option.from_json( ['b', True, 'bool'] ).value_type(), 'boolean' )
    check.equal( dds.option.from_json( ['b', False, 'bool'] ).value_type(), 'boolean' )
    check.equal( dds.option.from_json( ['b', 0, 'bool', ['boolean']] ).value_type(), 'boolean' )
    check.equal( dds.option.from_json( ['b', 0, 'bool', ['boolean']] ).get_value(), False )
    check.equal( dds.option.from_json( ['b', 1, 'bool', ['boolean']] ).get_value(), True )
    check.equal(
        dds.option.from_json( ['b', 1, 'bool', ['boolean']] ).to_json(),
        ['b', True, 'bool'] )
    with pytest.raises( RuntimeError, match=re.escape( 'not convertible to a boolean: 1.0' ) ):
        dds.option.from_json( ['b', 1., 'bool', ['boolean']] )
    with pytest.raises( RuntimeError, match=re.escape( 'not convertible to a boolean: 2' ) ):
        dds.option.from_json( ['b', 2, 'bool', ['boolean']] )
    check.equal( dds.option.from_json( ['b', False, True, 'bool'] ).value_type(), 'boolean' )
    check.equal( dds.option.from_json( ['b', False, None, 'bool', ['optional']] ).value_type(), 'boolean' )

def test_enum():
    check.equal( dds.option.from_json( ['e1', 'a', ['a','b','c'], 'c', 'enum'] ).value_type(), 'enum' )
    check.equal( dds.option.from_json( ['e1', 'a', ['a','a','c'], 'c', 'enum'] ).value_type(), 'enum' )
    with pytest.raises( RuntimeError, match=re.escape( 'invalid enum value: "c"' ) ):
        dds.option.from_json( ['e1', 'd', [], 'c', 'enum'] )
    with pytest.raises( RuntimeError, match=re.escape( 'enum option requires a choices array' ) ):
        dds.option.from_json( ['e1', 'a', None, 'c', 'enum'] )
    with pytest.raises( RuntimeError, match=re.escape( 'invalid enum value: "d"' ) ):
        dds.option.from_json( ['e1', 'd', ['a','b','c'], 'c', 'enum'] )
    with pytest.raises( RuntimeError, match=re.escape( 'invalid enum value: "d"' ) ):
        dds.option.from_json( ['e1', 'a', ['a','b','c'], 'd', 'enum'] )
    with pytest.raises( RuntimeError, match=re.escape( 'enum choices must be strings' ) ):
        dds.option.from_json( ['e1', 'a', [None,'b','c'], 'd', 'enum'] )
    with pytest.raises( RuntimeError, match=re.escape( 'non-string enum values' ) ):
        dds.option.from_json( ['e1', 1, [1,2,3], 3, 'enum'] )
    with pytest.raises( RuntimeError, match=re.escape( 'value is not optional' ) ):
        dds.option.from_json( ['e1', None, ['a','b','c'], 'c', 'enum'] )
    check.equal( dds.option.from_json( ['e1', None, ['a','b','c'], 'c', 'enum', ['optional']] ).value_type(), 'enum' )
    check.equal(
        dds.option.from_json( ['e1', None, ['a','b','c'], 'c', 'enum', ['optional']] ).to_json(),
        ['e1', None, ['a','b','c'], 'c', 'enum', ['optional']] )

def test_ro_options_are_still_settable():
    # NOTE: the DDS options do not enforce logic post initialization; they serve only to COMMUNICATE any state and limits
    check.equal( dds.option.from_json( ['1', 0, 'desc'] ).value_type(), 'int' )
    dds.option.from_json( ['1', 0, 'desc'] ).set_value( 20. )  # OK because 20.0 can be expressed as int
    with pytest.raises( RuntimeError, match=re.escape( 'not convertible to a signed integer: 20.5' ) ):
        dds.option.from_json( ['1', 0, 'desc'] ).set_value( 20.5 )

def test_optional_default_value():
    check.is_false( dds.option.from_json( ['1', 0, 'desc'] ).is_optional() )
    check.is_true( dds.option.from_json( ['Asic Temperature',None,-40.0,125.0,0.0,0.0,'Current Asic Temperature (degree celsius)',['optional','read-only']] ).is_optional() )
    dds.option.from_json( ['4', 'string-value', None, 'desc', ['optional']] )
    dds.option.from_json( ['5', None, 'default-string-value', 'desc', ['optional']] )  # string type is deduced
    with pytest.raises( RuntimeError, match=re.escape( 'cannot deduce value type: ["a",null,"desc",["optional"]]' ) ):
        dds.option.from_json( ['a', None, 'desc', ['optional']] )
    check.equal(
        dds.option.from_json( ['Integer Option', None, None, 'Something', ['optional', 'int']] ).to_json(),
        ['Integer Option', None, None, 'Something', ['optional', 'int']] )

def test_mixed_types():
    check.equal( dds.option.from_json( ['i', 0, 'desc'] ).value_type(), 'int' )
    check.equal( dds.option.from_json( ['f', 0, 'desc', ['float']] ).value_type(), 'float' )
    check.equal( dds.option.from_json( ['1', 0., 0, 1, 1, 0, 'desc'] ).value_type(), 'float' )
    check.equal( dds.option.from_json( ['2', 0, 0., 1, 1, 0, 'desc'] ).value_type(), 'float' )
    check.equal( dds.option.from_json( ['3', 0, 0, 1., 1, 0, 'desc'] ).value_type(), 'float' )
    check.equal( dds.option.from_json( ['4', 0, 0, 1, 1., 0, 'desc'] ).value_type(), 'float' )
    check.equal( dds.option.from_json( ['5', 0, 0, 1, 1, 0., 'desc'] ).value_type(), 'float' )
    check.equal( dds.option.from_json( ['-1', 0, -1, 4, 1, 0, 'desc'] ).value_type(), 'int' )
    check.equal(           # float, but forced to an int
        dds.option.from_json( ['3f', 3, 1, 5, 1, 2., '', ['int']] ).value_type(), 'int' )
    with pytest.raises( RuntimeError, match=re.escape( 'not convertible to a signed integer: 2.2' ) ):  # same, but with an actual float
        dds.option.from_json( ['3f', 3, 1, 5, 1, 2.2, '', ['int']] )

    dds.option.from_json( ['a', 9223372036854775807, 'desc'] )       # max int64
    with pytest.raises( RuntimeError, match=re.escape( 'not convertible to a signed integer: 9223372036854775808' ) ):
        dds.option.from_json( ['a', 9223372036854775808, 'desc'] )   # uint64
    dds.option.from_json( ['a', -9223372036854775808, 'desc'] )      # min int64
    # -9223372036854775809 cannot be converted to json

    check.equal( dds.option.from_json( ['Brightness',0,-64,64,1,0,'UVC image brightness'] ).value_type(), 'int' )

def test_range():
    with pytest.raises( RuntimeError, match=re.escape( 'default value 0 > -1 maximum' ) ):
        dds.option.from_json( ['3x', 0, 0, -1, 1, 0, ''] )
    with pytest.raises( RuntimeError, match=re.escape( 'default value 0 < 2 minimum' ) ):
        dds.option.from_json( ['3x', 0, 2, 3, 1, 0, ''] )
    with pytest.raises( RuntimeError, match=re.escape( 'default value 3 > 2 maximum' ) ):
        dds.option.from_json( ['3y', -1, -2, 2, 1, 3, 'bad default'] )
    dds.option.from_json( ['3', -1, -2, 2, 1, 0, ''] )
    dds.option.from_json( ['Backlight Compensation', 0, 0, 1, 1, 0, 'Backlight custom description'] )
    dds.option.from_json( ['Custom Option', 5., -10, 10, 1, -5., 'Description'] )

def test_ip_address():
    dds.option.from_json( ['ip', None, 'desc', ['IPv4', 'optional']] )
    with pytest.raises( RuntimeError, match=re.escape( 'not an IP address: ""' ) ):
        dds.option.from_json( ['ip', '', 'desc', ['IPv4']] )
    with pytest.raises( RuntimeError, match=re.escape( 'not an IP address: "0.0.0.0"' ) ):  # 0.0.0.0 is used to denote an "invalid" IP
        dds.option.from_json( ['ip', '0.0.0.0', 'desc', ['IPv4']] )
    dds.option.from_json( ['ip', '0.0.0.1', 'desc', ['IPv4']] )
    dds.option.from_json( ['ip', '255.255.255.255', 'desc', ['IPv4']] )
    with pytest.raises( RuntimeError, match=re.escape( 'not an IP address: "255.255.255.256"' ) ):
        dds.option.from_json( ['ip', '255.255.255.256', 'desc', ['IPv4']] )
    with pytest.raises( RuntimeError, match=re.escape( 'not an IP address: "255.255.256.255"' ) ):
        dds.option.from_json( ['ip', '255.255.256.255', 'desc', ['IPv4']] )
    with pytest.raises( RuntimeError, match=re.escape( 'not an IP address: "255.256.255.255"' ) ):
        dds.option.from_json( ['ip', '255.256.255.255', 'desc', ['IPv4']] )
    with pytest.raises( RuntimeError, match=re.escape( 'not an IP address: "256.255.255.255"' ) ):
        dds.option.from_json( ['ip', '256.255.255.255', 'desc', ['IPv4']] )
    with pytest.raises( RuntimeError, match=re.escape( 'not an IP address: "1.2.3.4a"' ) ):
        dds.option.from_json( ['ip', '1.2.3.4a', 'desc', ['IPv4']] )
    with pytest.raises( RuntimeError, match=re.escape( 'not an IP address: "1.2.3.4."' ) ):
        dds.option.from_json( ['ip', '1.2.3.4.', 'desc', ['IPv4']] )
    with pytest.raises( RuntimeError, match=re.escape( 'not an IP address: "1.2..4"' ) ):
        dds.option.from_json( ['ip', '1.2..4', 'desc', ['IPv4']] )
    with pytest.raises( RuntimeError, match=re.escape( 'not an IP address: "1.2.3."' ) ):
        dds.option.from_json( ['ip', '1.2.3.', 'desc', ['IPv4']] )
    with pytest.raises( RuntimeError, match=re.escape( 'not an IP address: "1.2.3"' ) ):
        dds.option.from_json( ['ip', '1.2.3', 'desc', ['IPv4']] )

def test_rect():
    check.equal( dds.option.from_json( ['r', [0,0,1,1], 'r/o'] ).value_type(), 'rect' )
    check.equal( dds.option.from_json( ['r', None, 'r/o', ['rect','optional']] ).value_type(), 'rect' )
    check.equal( dds.option.from_json( ['r', [0,1,2,3], 'r/o'] ).get_value(), [0,1,2,3] )
    check.equal( dds.option.from_json( ['r', [0,1,2,3], [1,2,3,4], 'r/w'] ).value_type(), 'rect' )
    check.equal( dds.option.from_json( ['r', [0,1,2,3], [1,2,3,4], 'r/w'] ).get_default_value(), [1,2,3,4] )
    with pytest.raises( RuntimeError, match=re.escape( 'cannot deduce value type: ["r",[0,1,2,3],[1,2,3.0,4],"r/w"]' ) ):  # non-integer inside the default value
        dds.option.from_json( ['r', [0,1,2,3], [1,2,3.,4], 'r/w'] )
    with pytest.raises( RuntimeError, match=re.escape( 'not [x1,y1,x2,y2]: 0' ) ):  # no range for rect
        dds.option.from_json( ['r', [0,1,2,3], 0, 2, 1, [1,2,3,4], 'r/w'] )
    with pytest.raises( RuntimeError, match=re.escape( 'non-string enum values' ) ):  # with 5 args, it looks like an enum
        dds.option.from_json( ['r', [0,1,2,3], [1,2,3,4], [1,2,3,4], 'r/w'] )
    # With a range supplied, operator<() for JSON takes effect
    check.equal( dds.option.from_json( ['r', [1,2,3,4], [0,1,2,3], [2,3,4,5], [3,4,5,6], [2,3,4,5], 'r/w'] ).value_type(), 'rect' )
    with pytest.raises( RuntimeError, match=re.escape( 'non-integers found: [0,1,2,3.0]' ) ):
        dds.option.from_json( ['r', [1,2,3,4], [0,1,2,3.], [2,3,4,5], [3,4,5,6], [2,3,4,5], 'r/w'] )

#############################################################################################
