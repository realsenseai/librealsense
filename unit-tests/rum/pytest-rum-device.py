# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs


# device_each supplies the camera; the ENABLE_STATS gate lives in the rum_report fixture (conftest),
# which skips when the build can't produce a report.
pytestmark = [ pytest.mark.device_each( "D400*" ), pytest.mark.device_each( "D500*" ), pytest.mark.context( "nightly" ) ]


def depth_z16_profile( sensor ):
    profile = next( ( p for p in sensor.get_stream_profiles()
                      if p.stream_type() == rs.stream.depth and p.format() == rs.format.z16 ), None )
    assert profile is not None, "device exposes no Z16 depth profile"
    return profile


def device_entry( report, dev ):
    name = dev.get_info( rs.camera_info.name )
    conn = dev.get_info( rs.camera_info.connection_type ) if dev.supports( rs.camera_info.connection_type ) else ""
    return report.get( "devices", {} ).get( f"{name}-{conn}" )


def test_created_device_appears_in_report( test_device, rum_report ):
    dev, _ = test_device
    entry = device_entry( rum_report(), dev )
    assert entry is not None
    assert entry.get( "fw_version" )
    assert entry.get( "connection" )
    assert entry.get( "count", 0 ) >= 1


def test_opened_stream_appears_in_report( test_device, rum_report ):
    dev, _ = test_device
    sensor = dev.first_depth_sensor()
    sensor.open( depth_z16_profile( sensor ) )       # triggers the stream hook
    try:
        # streams is an object keyed by "<type>-<format>-<WxH>@<fps>"; type/format/res live in the label.
        streams = ( device_entry( rum_report(), dev ) or {} ).get( "streams", {} )
        label = next( ( lbl for lbl in streams if lbl.startswith( "Depth-Z16-" ) ), None )
        assert label is not None
        assert "x" in label and "@" in label         # resolution and fps encoded in the label
        assert streams[ label ].get( "count", 0 ) >= 1
    finally:
        sensor.close()


def test_applied_filter_and_stream_duration( test_device, rum_report ):
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
    entry = device_entry( rum_report(), dev )
    assert entry is not None
    # filters are tallied per device (name -> use count).
    assert entry.get( "filters", {} ).get( "Spatial Filter", 0 ) >= 1
    # The depth stream above (start -> stop) accumulates duration on its stream config.
    depth = next( ( s for lbl, s in entry.get( "streams", {} ).items() if lbl.startswith( "Depth-Z16-" ) ), None )
    assert depth is not None
    assert depth.get( "duration_seconds", 0 ) > 0


def test_non_default_option_in_options_changed( test_device, rum_report ):
    dev, _ = test_device
    sensor = dev.first_depth_sensor()
    opt = rs.option.laser_power
    if not sensor.supports( opt ):
        pytest.skip( "device has no Laser Power option" )
    rng = sensor.get_option_range( opt )
    newval = rng.min if rng.default != rng.min else rng.max
    sensor.set_option( opt, newval )
    try:
        # options_changed is per device, keyed by option name.
        changed = ( device_entry( rum_report(), dev ) or {} ).get( "options_changed", {} )
        entry = changed.get( "Laser Power" )
        assert entry is not None
        assert entry.get( "set_count", 0 ) >= 1
        assert entry.get( "last_value" ) == newval
    finally:
        sensor.set_option( opt, rng.default )   # restore device state
