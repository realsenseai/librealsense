# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import os
import sys
import json
import subprocess
import pytest
import pyrealsense2 as rs
from rspy import config_file


MAX_REPORT_BYTES = 64 * 1024   # the size rule the server enforces; mirrors max_report_bytes in the SDK


def compact_size( path ):
    """Bytes the uploader would send: the report is stored indented and compacted on the wire."""
    with open( path, encoding="utf-8" ) as f:
        return len( json.dumps( json.load( f ), separators=( ",", ":" ) ) )


def test_rum_submodule_is_exposed():
    assert hasattr( rs, "rum" )


def test_cloud_consent_round_trips():
    if os.environ.get( "RS2_RUM_CLOUD_ENABLED" ):
        pytest.skip( "RS2_RUM_CLOUD_ENABLED is set; env overrides the config value" )
    # Leave the config as we found it: restore the value if the key was there, drop the key if only
    # the file was there, or remove the file entirely if the test created it (e.g. a fresh CI runner).
    key = "rum_cloud_enabled"
    cfg_path = config_file.get_config_path()
    had_file = os.path.exists( cfg_path )
    had_key = had_file and key in config_file.get_config_file()
    saved = rs.rum.is_cloud_enabled()
    try:
        rs.rum.set_cloud_enabled( True )
        assert rs.rum.is_cloud_enabled()
        rs.rum.set_cloud_enabled( False )
        assert not rs.rum.is_cloud_enabled()
    finally:
        if had_key:
            rs.rum.set_cloud_enabled( saved )
        elif had_file:
            cfg = config_file.get_config_file()
            cfg.pop( key, None )
            with open( cfg_path, "w", encoding="utf-8" ) as f:
                json.dump( cfg, f )
        elif os.path.exists( cfg_path ):
            os.remove( cfg_path )


def test_report_is_valid_json_with_expected_fields( rum_report ):
    report = rum_report()
    assert report.get( "schema_version" ) == 1
    source_id = report.get( "source_id", "" )
    assert isinstance( source_id, str )
    assert len( source_id ) == 36
    session_id = report.get( "session_id", "" )
    assert isinstance( session_id, str )
    assert len( session_id ) == 36
    assert isinstance( report.get( "generated_at" ), int )
    assert report.get( "sdk", {} ).get( "version" )
    assert isinstance( report.get( "sdk", {} ).get( "cmake_flags" ), dict )
    assert report.get( "sdk", {} ).get( "backend" )
    assert report.get( "system", {} ).get( "os" )
    assert report.get( "system", {} ).get( "arch" )
    # devices is a keyed object (streams/options/filters nested under each); notifications and apps top-level.
    assert isinstance( report.get( "devices" ), dict )
    assert isinstance( report.get( "notifications" ), list )
    assert isinstance( report.get( "apps" ), dict )


def test_source_id_is_stable_across_calls( rum_report ):
    first = rum_report().get( "source_id" )
    again = rum_report().get( "source_id" )
    assert first == again


def test_oversized_report_is_capped( rum_report ):
    # The saved report is folded in once per process, so the capping flush has to happen in a fresh one.
    rum_report()
    path = rs.rum.get_report_path()
    with open( path, encoding="utf-8" ) as f:
        report = json.load( f )
    report["devices"]["Oversized-USB"] = {
        "connection": "USB", "fw_version": "0.0.0.0", "mipi_driver_version": "", "count": 1,
        "streams": { f"Depth-Z16-{width}x480@30": { "count": 1, "duration_seconds": 1.0 }
                     for width in range( 2000 ) },
        "options_changed": {}, "filters": {} }
    with open( path, "w", encoding="utf-8" ) as f:
        json.dump( report, f )
    assert compact_size( path ) > MAX_REPORT_BYTES

    # Hand the child the very module this process loaded: an inherited PYTHONPATH may name another
    # build of pyrealsense2, and conftest appends the fresh one behind it.
    env = dict( os.environ, PYTHONPATH=os.path.dirname( rs.__file__ ) + os.pathsep + os.environ.get( "PYTHONPATH", "" ) )
    subprocess.run( [ sys.executable, "-c", "import pyrealsense2 as rs; c = rs.context(); del c" ],
                    check=True, env=env )
    assert compact_size( path ) <= MAX_REPORT_BYTES


def test_processing_block_option_excluded_from_options_changed( rum_report ):
    # A processing-block option must never land in options_changed (only device options do).
    th = rs.threshold_filter()
    th.set_option( rs.option.min_distance, 0.5 )
    names = []
    for d in rum_report().get( "devices", {} ).values():
        names += list( d.get( "options_changed", {} ).keys() )
    assert "Min Distance" not in names


def record_one_frame( filename ):
    # The recorder is released when this returns, closing the file before playback.
    sd = rs.software_device()
    sd.register_info( rs.camera_info.name, "RUM Recorded Device" )
    vs = rs.video_stream()
    vs.type, vs.width, vs.height, vs.fps, vs.bpp, vs.fmt = rs.stream.depth, 640, 480, 30, 2, rs.format.z16
    sensor = sd.add_sensor( "Synthetic" )
    profile = sensor.add_video_stream( vs )
    recorder = rs.recorder( filename, sd )
    sensor.open( profile )
    sensor.start( lambda f: None )
    frame = rs.software_video_frame()   # device info is written to the file only with a frame
    frame.pixels, frame.bpp, frame.stride, frame.profile = bytearray( 640 * 480 * 2 ), 2, 640 * 2, profile.as_video_stream_profile()
    sensor.on_video_frame( frame )
    sensor.stop()
    sensor.close()


def test_playback_device_not_in_report( tmp_path, rum_report ):
    filename = str( tmp_path / "rum.db3" )
    record_one_frame( filename )
    ctx = rs.context()
    playback = ctx.load_device( filename )   # the context lists it only while this is alive
    # Creating it from the context's list (needs the sw_only mask) runs the RUM device hook.
    devices = ctx.query_devices( int( rs.product_line.sw_only ) | int( rs.product_line.any ) )
    recorded = [ d for d in devices if d.get_info( rs.camera_info.name ) == "RUM Recorded Device" ]
    assert recorded
    # Stream it as the Viewer would; nothing from it may reach the report.
    sensor = recorded[0].query_sensors()[0]
    queue = rs.frame_queue( 8 )
    sensor.open( sensor.get_stream_profiles()[0] )
    sensor.start( queue )
    try:
        queue.wait_for_frame()
    finally:
        sensor.stop()
        sensor.close()
    names = [ key.rsplit( "-", 1 )[0] for key in rum_report().get( "devices", {} ) ]
    assert "RUM Recorded Device" not in names
    assert "" not in names
