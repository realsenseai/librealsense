# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Tests for the D585 2C config variants (rspy.d585_config + rspy.devices). No hardware.

A toggleable D585 gets a second device entry named like the camera in 2C, selectable only by a name pattern that
says "Dual RGB"; the rest of the harness keeps seeing exactly the physical cameras.
"""

import types
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


def fake_device( sn, name, port = 3, mode = 0 ):
    return types.SimpleNamespace( serial_number = sn, name = name, port = port, handle = FakeHandle( mode ),
                                  enabled = True, product_line = 'D500', real_serial = None )


@pytest.fixture
def lab( monkeypatch ):
    """A D585 Prototype, a D585S and a D455 -- only the first can toggle"""
    monkeypatch.setattr( d585_config, '_option', lambda: 'sensors_config_mode' )
    devices = { '541': fake_device( '541', 'D585 Prototype' ),
                '353': fake_device( '353', 'D585S', port = 4 ),
                '207': fake_device( '207', 'D455', port = 1 ) }
    devices['207'].product_line = 'D400'
    monkeypatch.setattr( dev, '_device_by_sn', devices )
    dev.add_config_variants()
    return devices


def test_only_the_toggleable_d585_gets_a_variant( lab ):
    assert sorted( lab ) == ['207', '353', '541', '541-2c']
    variant = dev.get( '541-2c' )
    assert variant.name == 'D585 Proto Dual RGB'
    assert variant.port == 3 and variant.handle is lab['541'].handle
    assert dev.real_serial( '541-2c' ) == '541' and dev.real_serial( '541' ) == '541'


def test_physical_views_do_not_list_the_variant( lab ):
    assert dev.all() == { '541', '353', '207' }
    assert dev.by_product_line( 'D500', [] ) == { '541', '353' }


def test_variant_answers_only_to_dual_rgb_names( lab ):
    assert dev.by_name( 'Dual RGB', [] ) == { '541-2c' }
    assert dev.by_name( 'D585 Proto Dual RGB', [] ) == { '541-2c' }
    assert dev.by_name( 'D585 Dual RGB', [] ) == set()
    assert dev.by_name( 'D585', [] ) == { '541', '353' }
    assert dev.by_name( 'Proto', [] ) == { '541' }
    assert set( dev.by_spec( 'D500*', [] ) ) == { '541', '353' }


def test_no_variant_without_the_option( monkeypatch, lab ):
    monkeypatch.setattr( d585_config, '_option', lambda: None )
    monkeypatch.setattr( dev, '_device_by_sn', { '541': fake_device( '541', 'D585 Prototype' ) } )
    dev.add_config_variants()
    assert sorted( dev.get( '541' ) and dev._device_by_sn ) == ['541']


def test_ensure_switches_and_resets_only_when_needed( monkeypatch, lab ):
    resets = []
    monkeypatch.setattr( dev, 'hw_reset', lambda sns, **kw: resets.append( sns ) or True )
    assert d585_config.ensure( '541', '3c' ) is False and resets == []
    assert d585_config.ensure( '541', '2c' ) is True and resets == [['541']]
    assert d585_config.current( lab['541'].handle ) == '2c'
    assert d585_config.ensure( '541', '3c' ) is True and len( resets ) == 2


def test_ensure_fails_when_the_camera_does_not_come_back( monkeypatch, lab ):
    monkeypatch.setattr( dev, 'hw_reset', lambda sns, **kw: False )
    with pytest.raises( RuntimeError, match='did not come back' ):
        d585_config.ensure( '541', '2c' )


def test_excluding_the_camera_excludes_its_variant( lab ):
    from rspy.pytest.device_helpers import _build_sn_filter
    passes = _build_sn_filter( [], cli_excludes = ['D585 Prototype'] )
    assert not passes( '541' ) and not passes( '541-2c' )
    assert passes( '353' ) and passes( '207' )
