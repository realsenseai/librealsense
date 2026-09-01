# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs


# device_each supplies the camera; the ENABLE_STATS gate lives in the rum_report fixture (conftest),
# which skips when the build can't produce a report.
pytestmark = [ pytest.mark.device_each( "D400*" ), pytest.mark.device_each( "D500*" ) ]


def streamable_depth_profile( sensor ):
    # The device's default depth profile - the one it actually streams - rather than an arbitrary
    # format/resolution that may exist but not deliver frames on every model (e.g. D585S).
    depth = [ p for p in sensor.get_stream_profiles() if p.stream_type() == rs.stream.depth ]
    profile = next( ( p for p in depth if p.is_default() ), depth[0] if depth else None )
    assert profile is not None, "device exposes no depth profile"
    return profile


def recorded_stream( entry, profile ):
    # Match the report entry against the profile we actually opened, instead of a hardcoded label.
    vp = profile.as_video_stream_profile()
    res = f"{vp.width()}x{vp.height()}"
    fps = f"@{profile.fps()}"
    return next( ( s for lbl, s in ( entry or {} ).get( "streams", {} ).items()
                   if lbl.startswith( "Depth-" ) and res in lbl and fps in lbl ), None )


def change_an_option( sensor ):
    # Set a non-default value on the first option the device actually accepts one for, instead of
    # hardcoding a control that some models advertise as writable but reject on set (e.g. Laser Power
    # on D585S). Returns (option, range, value_set).
    for opt in sensor.get_supported_options():
        if sensor.is_option_read_only( opt ):
            continue
        rng = sensor.get_option_range( opt )
        if rng.min >= rng.max:
            continue
        newval = rng.min if rng.default != rng.min else rng.max
        try:
            sensor.set_option( opt, newval )
        except Exception:
            continue   # advertised writable but the device rejects the write - try the next
        return opt, rng, newval
    return None, None, None


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
    profile = streamable_depth_profile( sensor )
    sensor.open( profile )                            # triggers the stream hook
    try:
        stream = recorded_stream( device_entry( rum_report(), dev ), profile )
        assert stream is not None                     # the profile we opened is in the report
        assert stream.get( "count", 0 ) >= 1
    finally:
        sensor.close()


def test_applied_filter_and_stream_duration( test_device, rum_report ):
    dev, _ = test_device
    sensor = dev.first_depth_sensor()
    queue = rs.frame_queue( 8 )
    spatial = rs.spatial_filter()
    profile = streamable_depth_profile( sensor )
    sensor.open( profile )
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
    # The depth stream above (start -> stop) accumulates duration on the profile we streamed.
    depth = recorded_stream( entry, profile )
    assert depth is not None
    assert depth.get( "duration_seconds", 0 ) > 0


def test_non_default_option_in_options_changed( test_device, rum_report ):
    dev, _ = test_device
    sensor = dev.first_depth_sensor()
    opt, rng, newval = change_an_option( sensor )
    assert opt is not None, "device exposes no settable option"
    try:
        # options_changed is per device, keyed by option name (str(opt) == the report's key).
        changed = ( device_entry( rum_report(), dev ) or {} ).get( "options_changed", {} )
        entry = changed.get( str( opt ) )
        assert entry is not None
        assert entry.get( "set_count", 0 ) >= 1
        assert entry.get( "last_value" ) == newval
    finally:
        sensor.set_option( opt, rng.default )   # restore device state
