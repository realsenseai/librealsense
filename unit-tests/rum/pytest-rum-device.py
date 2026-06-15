# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import json
import pytest
import pyrealsense2 as rs


def _stats_enabled():
    flags = json.loads( rs.rum.get_report() ).get( "sdk", {} ).get( "cmake_flags", {} )
    return bool( flags.get( "ENABLED_STATS", False ) )


# The collector is fed only by the instrumentation hooks, which compile to no-ops when
# ENABLED_STATS is off (the build default). Without them the report stays empty, so these
# live-device checks only make sense on a stats-enabled build.
pytestmark = [
    pytest.mark.device( "D400*" ),
    pytest.mark.skipif( not _stats_enabled(), reason="SDK built with ENABLED_STATS=OFF" ),
]


def depth_z16_profile( sensor ):
    profile = next( ( p for p in sensor.get_stream_profiles()
                      if p.stream_type() == rs.stream.depth and p.format() == rs.format.z16 ), None )
    assert profile is not None, "device exposes no Z16 depth profile"
    return profile


def test_created_device_appears_in_report( test_device ):
    dev, _ = test_device
    name = dev.get_info( rs.camera_info.name )
    devices = json.loads( rs.rum.get_report() ).get( "devices", [] )
    entry = next( ( d for d in devices if d.get( "type" ) == name ), None )
    assert entry is not None
    assert entry.get( "fw_version" )
    assert entry.get( "connection" )
    assert entry.get( "count", 0 ) >= 1


def test_opened_stream_appears_in_report( test_device ):
    dev, _ = test_device
    sensor = dev.first_depth_sensor()
    sensor.open( depth_z16_profile( sensor ) )       # triggers the stream hook
    try:
        streams = json.loads( rs.rum.get_report() ).get( "streams", [] )
        depth = next( ( s for s in streams if s.get( "type" ) == "Depth" ), None )
        assert depth is not None
        assert depth.get( "format" ) == "Z16"
        assert "x" in depth.get( "resolution", "" )
        assert depth.get( "fps", 0 ) > 0
    finally:
        sensor.close()


def test_applied_filter_and_stream_duration( test_device ):
    dev, _ = test_device
    sensor = dev.first_depth_sensor()
    queue = rs.frame_queue( 8 )
    spatial = rs.spatial_filter()
    sensor.open( depth_z16_profile( sensor ) )
    sensor.start( queue )
    try:
        for _ in range( 10 ):
            spatial.process( queue.wait_for_frame() )   # run frames through the filter -> applied
    finally:
        sensor.stop()
        sensor.close()
    report = json.loads( rs.rum.get_report() )
    flt = next( ( f for f in report.get( "filters", [] ) if f.get( "name" ) == "Spatial Filter" ), None )
    assert flt is not None
    assert flt.get( "count", 0 ) >= 1
    # The depth stream above (start -> stop) accumulates duration on its stream config.
    depth = next( ( s for s in report.get( "streams", [] ) if s.get( "type" ) == "Depth" ), None )
    assert depth is not None
    assert depth.get( "duration_seconds", 0 ) > 0


def test_non_default_option_in_options_changed( test_device ):
    dev, _ = test_device
    sensor = dev.first_depth_sensor()
    opt = rs.option.laser_power
    if not sensor.supports( opt ):
        pytest.skip( "device has no Laser Power option" )
    rng = sensor.get_option_range( opt )
    newval = rng.min if rng.default != rng.min else rng.max
    sensor.set_option( opt, newval )
    try:
        changed = json.loads( rs.rum.get_report() ).get( "options_changed", [] )
        entry = next( ( o for o in changed if o.get( "option" ) == "Laser Power" ), None )
        assert entry is not None
        assert entry.get( "set_count", 0 ) >= 1
        assert entry.get( "last_value" ) == newval
    finally:
        sensor.set_option( opt, rng.default )   # restore device state
