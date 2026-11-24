# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Tests for rspy.d585_config: selecting a dedicated sensor D585 by its dual RGB name, and switching it. No hardware."""

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
    connection_type = 'USB'


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
    assert d585_config.split( '541-dual-rgb' ) == ( '541', 'dual_rgb' )
    assert d585_config.split( '541' ) == ( '541', 'dedicated_sensor' )
    assert d585_config.split( '__SKIP__:D585 Proto Dual RGB' ) == ( '__SKIP__:D585 Proto Dual RGB', 'dedicated_sensor' )


def test_only_dual_rgb_patterns_select_the_switchable_camera( monkeypatch, lab ):
    assert d585_config.switchable_to( 'Dual RGB' ) == ['541']
    assert d585_config.switchable_to( 'D585 Proto Dual RGB' ) == ['541']
    assert d585_config.switchable_to( 'D585 Dual RGB' ) == []
    assert d585_config.switchable_to( 'D585' ) == []
    assert d585_config.switchable_to( 'D500*' ) == []
    assert d585_config.switchable_to( '541' ) == []
    monkeypatch.setattr( d585_config, '_option', lambda: None )
    assert d585_config.switchable_to( 'Dual RGB' ) == []


def test_can_toggle( monkeypatch, lab ):
    assert d585_config.can_toggle( lab['541'] )
    assert not d585_config.can_toggle( lab['353'] ) and not d585_config.can_toggle( lab['207'] )
    monkeypatch.setattr( d585_config, '_option', lambda: None )
    assert not d585_config.can_toggle( lab['541'] )


def test_camera_left_in_dual_rgb_is_listed_as_its_dedicated_sensor_self( lab ):
    lab['541'] = fake_device( '541', 'D585 Proto Dual RGB', mode = 1 )
    d585_config.adopt_dedicated_sensor_names()
    assert lab['541'].name == 'D585 Prototype' and lab['353'].name == 'D585S'
    assert d585_config.switchable_to( 'Dual RGB' ) == ['541'] and d585_config.can_toggle( lab['541'] )


def test_dual_rgb_only_camera_keeps_its_name( monkeypatch, lab ):
    lab['900'] = fake_device( '900', 'D585 Dual RGB' )
    lab['900'].handle.sensor.supports = lambda opt: False
    d585_config.adopt_dedicated_sensor_names()
    assert lab['900'].name == 'D585 Dual RGB' and not d585_config.can_toggle( lab['900'] )


def test_ensure_switches_and_resets_only_when_needed( monkeypatch, lab ):
    resets = []
    monkeypatch.setattr( dev, 'hw_reset', lambda sns, **kw: resets.append( sns ) or True, raising = False )
    assert d585_config.ensure( '541', 'dedicated_sensor' ) is False and resets == []
    assert d585_config.ensure( '541', 'dual_rgb' ) is True and resets == [['541']]
    assert d585_config.current( lab['541'].handle ) == 'dual_rgb'
    assert d585_config.ensure( '541', 'dedicated_sensor' ) is True and len( resets ) == 2


def test_ensure_fails_when_the_camera_does_not_come_back( monkeypatch, lab ):
    monkeypatch.setattr( dev, 'hw_reset', lambda sns, **kw: False, raising = False )
    with pytest.raises( RuntimeError, match='did not come back' ):
        d585_config.ensure( '541', 'dual_rgb' )


def test_every_switchable_camera_gets_a_dual_rgb_instance_unless_an_exclude_pattern_covers_it( lab ):
    assert d585_config.extra_instances( '541', [] ) == ['541-dual-rgb']
    assert d585_config.extra_instances( '541', ['Dual RGB'] ) == []        # device_exclude marker or --exclude-device
    assert d585_config.extra_instances( '541', ['D585S'] ) == ['541-dual-rgb']
    assert d585_config.extra_instances( '353', [] ) == [] and d585_config.extra_instances( '207', [] ) == []


def test_no_dual_rgb_instance_on_a_connection_type_that_is_excluded( lab ):
    lab['541'].connection_type = 'GMSL'
    assert d585_config.extra_instances( '541', [], {'GMSL'} ) == []
    assert d585_config.extra_instances( '541', [], {'USB'} ) == ['541-dual-rgb']
    assert d585_config.extra_instances( '541', [] ) == ['541-dual-rgb']


def test_every_switched_camera_is_restored_at_session_end( monkeypatch, lab ):
    monkeypatch.setattr( dev, 'hw_reset', lambda sns, **kw: True, raising = False )
    monkeypatch.setattr( dev, 'enable_only', lambda sns, **kw: None, raising = False )
    d585_config.setup( '541-dual-rgb' )
    assert d585_config.current( lab['541'].handle ) == 'dual_rgb'
    d585_config.restore_all()
    assert d585_config.current( lab['541'].handle ) == 'dedicated_sensor'
    d585_config.restore_all()      # nothing left to restore


def test_a_failed_switch_is_still_restored_at_session_end( monkeypatch, lab ):
    monkeypatch.setattr( dev, 'hw_reset', lambda sns, **kw: False, raising = False )
    monkeypatch.setattr( dev, 'enable_only', lambda sns, **kw: None, raising = False )
    with pytest.raises( RuntimeError ):
        d585_config.setup( '541-dual-rgb' )
    monkeypatch.setattr( dev, 'hw_reset', lambda sns, **kw: True, raising = False )
    d585_config.restore_all()
    assert d585_config.current( lab['541'].handle ) == 'dedicated_sensor'


def test_mode_changes_until_the_camera_is_in_the_instances_mode( monkeypatch, lab ):
    monkeypatch.setattr( dev, 'hw_reset', lambda sns, **kw: True, raising = False )
    monkeypatch.setattr( dev, 'enable_only', lambda sns, **kw: None, raising = False )
    assert not d585_config.mode_changes( '541' )
    assert d585_config.mode_changes( '541-dual-rgb' )
    assert not d585_config.mode_changes( ['541', '542'] )
    d585_config.setup( '541-dual-rgb' )
    assert not d585_config.mode_changes( '541-dual-rgb' )
    assert d585_config.mode_changes( '541' )
    d585_config.restore_all()
    assert not d585_config.mode_changes( '541' )
