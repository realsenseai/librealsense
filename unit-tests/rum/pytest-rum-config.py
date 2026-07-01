# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import json
import pyrealsense2 as rs


def test_rum_submodule_is_exposed():
    assert hasattr( rs, "rum" )


def test_cloud_consent_round_trips():
    rs.rum.set_cloud_enabled( True )
    assert rs.rum.is_cloud_enabled()
    rs.rum.set_cloud_enabled( False )
    assert not rs.rum.is_cloud_enabled()


def test_report_is_valid_json_with_expected_fields():
    report = json.loads( rs.rum.get_report() )
    assert report.get( "schema_version" ) == 2
    source_id = report.get( "source_id", "" )
    assert isinstance( source_id, str )
    assert len( source_id ) == 36
    assert report.get( "sdk", {} ).get( "version" )
    assert isinstance( report.get( "sdk", {} ).get( "cmake_flags" ), dict )
    assert report.get( "sdk", {} ).get( "backend" )
    assert report.get( "system", {} ).get( "os" )
    assert report.get( "system", {} ).get( "arch" )
    # Aggregation arrays are always present (possibly empty) so the schema is stable.
    for key in ( "devices", "streams", "options_changed", "filters", "notifications" ):
        assert isinstance( report.get( key ), list )


def test_source_id_is_stable_across_calls():
    first = json.loads( rs.rum.get_report() ).get( "source_id" )
    again = json.loads( rs.rum.get_report() ).get( "source_id" )
    assert first == again


def test_processing_block_option_excluded_from_options_changed():
    # A processing-block option must never land in options_changed (only device options do).
    th = rs.threshold_filter()
    th.set_option( rs.option.min_distance, 0.5 )
    names = [ o.get( "option" ) for o in json.loads( rs.rum.get_report() ).get( "options_changed", [] ) ]
    assert "Min Distance" not in names
