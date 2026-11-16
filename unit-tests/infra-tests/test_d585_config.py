# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Tests for rspy.d585_config: selecting a 3C D585 by its 2C name, and switching it. No hardware."""

import pytest

import rspy.devices as dev
from rspy import d585_config


class FakeSensor:
    def __init__( self, mode ):
        self.mode = mode

    def supports( self, opt ):
        return True

    def get_option( self, opt ):
        return self.mode

    def set_option( self, opt, value ):
        self.mode = value


class FakeHandle:
    def __init__( self, mode = 0 ):
        self.sensor = FakeSensor( mode )

    def first_depth_sensor( self ):
        return self.sensor


class FakeDevice:
    def __init__( self, sn, name, mode = 0 ):
        self.serial_number, self._name, self.handle = sn, name, FakeHandle( mode )

    name = property( lambda self: self._name )


fake_device = FakeDevice


@pytest.fixture
def lab( monkeypatch ):
    """A D585 Prototype, a D585S and a D455 -- only the first can switch"""
    monkeypatch.setattr( d585_config, '_option', lambda: 'sensors_config_mode' )
    devices = { '541': fake_device( '541', 'D585 Prototype' ),
                '353': fake_device( '353', 'D585S' ),
                '207': fake_device( '207', 'D455' ) }
    monkeypatch.setattr( dev, '_device_by_sn', devices )
    return devices


def test_split():
    assert d585_config.split( '541-2c' ) == ( '541', '2c' )
    assert d585_config.split( '541' ) == ( '541', '3c' )
    assert d585_config.split( '__SKIP__:D585 Proto Dual RGB' ) == ( '__SKIP__:D585 Proto Dual RGB', '3c' )


def test_only_dual_rgb_patterns_select_the_switchable_camera( lab ):
    assert d585_config.switchable_to( 'Dual RGB' ) == ['541']
    assert d585_config.switchable_to( 'D585 Proto Dual RGB' ) == ['541']
    assert d585_config.switchable_to( 'D585 Dual RGB' ) == []
    assert d585_config.switchable_to( 'D585' ) == []
    assert d585_config.switchable_to( 'D500*' ) == []
    assert d585_config.switchable_to( '541' ) == []


def test_can_toggle( monkeypatch, lab ):
    assert d585_config.can_toggle( lab['541'] )
    assert not d585_config.can_toggle( lab['353'] ) and not d585_config.can_toggle( lab['207'] )
    monkeypatch.setattr( d585_config, '_option', lambda: None )
    assert not d585_config.can_toggle( lab['541'] )


def test_camera_left_in_2c_is_listed_as_its_3c_self( lab ):
    lab['541'] = fake_device( '541', 'D585 Proto Dual RGB', mode = 1 )
    d585_config.adopt_3c_names()
    assert lab['541'].name == 'D585 Prototype' and lab['353'].name == 'D585S'
    assert d585_config.switchable_to( 'Dual RGB' ) == ['541'] and d585_config.can_toggle( lab['541'] )


def test_2c_only_camera_keeps_its_name( monkeypatch, lab ):
    lab['900'] = fake_device( '900', 'D585 Dual RGB' )
    lab['900'].handle.sensor.supports = lambda opt: False
    d585_config.adopt_3c_names()
    assert lab['900'].name == 'D585 Dual RGB' and not d585_config.can_toggle( lab['900'] )


def test_ensure_switches_and_resets_only_when_needed( monkeypatch, lab ):
    resets = []
    monkeypatch.setattr( dev, 'hw_reset', lambda sns, **kw: resets.append( sns ) or True, raising = False )
    assert d585_config.ensure( '541', '3c' ) is False and resets == []
    assert d585_config.ensure( '541', '2c' ) is True and resets == [['541']]
    assert d585_config.current( lab['541'].handle ) == '2c'
    assert d585_config.ensure( '541', '3c' ) is True and len( resets ) == 2


def test_ensure_fails_when_the_camera_does_not_come_back( monkeypatch, lab ):
    monkeypatch.setattr( dev, 'hw_reset', lambda sns, **kw: False, raising = False )
    with pytest.raises( RuntimeError, match='did not come back' ):
        d585_config.ensure( '541', '2c' )
