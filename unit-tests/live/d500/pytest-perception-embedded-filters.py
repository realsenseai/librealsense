# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# The perception stream and the embedded filters can run at the same time.
#
# The decimation and temporal embedded filters (DPP) run on the camera CPU, so they no longer
# compete with object detection for the DSP. The SDK must not reject either order: enabling a
# filter and then starting perception, or toggling a filter while perception is opened or streaming.
#
# The test adapts to the device at runtime: it skips when the perception stream or a
# given embedded filter is not present. Older FW runs DPP on the DSP, so DPP filters need DPP_MIN_FW.

import time
import pytest
import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy.pytest.device_helpers import require_min_fw_version
import logging
log = logging.getLogger(__name__)


pytestmark = [
    pytest.mark.device_each( "D555" ),
    pytest.mark.device_each( "D585" ),
    pytest.mark.device_exclude( "D585S" ),
    pytest.mark.context( "nightly" ),
]


EMBEDDED_FILTERS = [
    rs.embedded_filter_type.decimation,
    rs.embedded_filter_type.temporal,
    rs.embedded_filter_type.improved_close_range_depth,
]

ENABLED = rs.option.embedded_filter_enabled
DPP_MIN_FW = rsutils.version( "7.59.46680" )  # first FW that runs DPP on the CPU by default


@pytest.fixture
def depth_sensor(test_device):
    dev, _ = test_device
    return dev.first_depth_sensor()


@pytest.fixture
def perception_sensor(test_device):
    dev, _ = test_device
    try:
        return dev.first_perception_sensor()  # throws if the device has no perception sensor
    except RuntimeError:
        pytest.skip( "Device has no perception sensor" )


@pytest.fixture
def perception_profile(perception_sensor):
    profile = next( ( p for p in perception_sensor.get_stream_profiles()
                      if p.stream_type() == rs.stream.object_detection ), None )
    if profile is None:
        pytest.skip( "Perception sensor has no object-detection profile" )
    return profile


# Over USB the decimation/temporal DPP filters are toggled through the 'enabled' field of their composite
# option; over DDS (and for the other filters) through the embedded_filter_enabled option.
DPP_COMPOSITE = {
    rs.embedded_filter_type.decimation: ( rs.composite_option_id.decimation_filter_dpp,
                                          'get_decimation_filter_dpp_config', 'set_decimation_filter_dpp_config' ),
    rs.embedded_filter_type.temporal: ( rs.composite_option_id.temporal_filter_dpp,
                                        'get_temporal_filter_dpp_config', 'set_temporal_filter_dpp_config' ),
}


class FilterToggle:
    def __init__( self, ef, filter_type ):
        self._ef = ef
        self._composite = None
        if not ef.supports( ENABLED ) and filter_type in DPP_COMPOSITE:
            option_id, getter, setter = DPP_COMPOSITE[filter_type]
            if option_id in ef.get_supported_composite_options():
                self._composite = ( option_id, getattr( ef, getter ), getattr( ef, setter ) )
        if not self._composite and not ef.supports( ENABLED ):
            pytest.skip( f"Embedded filter {filter_type} has no enable control on this device" )

    def get( self ):
        if self._composite:
            option_id, getter, _ = self._composite
            return float( getter( option_id ).enabled )
        return self._ef.get_option( ENABLED )

    def set( self, value ):
        if self._composite:
            option_id, getter, setter = self._composite
            cfg = getter( option_id )
            cfg.enabled = int( value )
            setter( option_id, cfg )
        else:
            self._ef.set_option( ENABLED, value )


@pytest.fixture
def embedded_filter(test_device, depth_sensor, filter_type):
    if filter_type in DPP_COMPOSITE:
        require_min_fw_version( test_device[0], DPP_MIN_FW, "DPP filters with object detection" )
    # get_embedded_filter throws (rather than returning falsy) when the filter is not present.
    try:
        ef = depth_sensor.get_embedded_filter( filter_type )
    except Exception:
        pytest.skip( f"Embedded filter {filter_type} not present on this device" )
    toggle = FilterToggle( ef, filter_type )
    initial = toggle.get()
    yield toggle
    toggle.set( initial )


def start_perception(perception_sensor, perception_profile):
    perception_sensor.open( perception_profile )
    try:
        perception_sensor.start( lambda f: None )
    except Exception:
        perception_sensor.close()  # start never took effect; undo the open before propagating
        raise


def stop_perception(perception_sensor):
    perception_sensor.stop()
    perception_sensor.close()


@pytest.mark.parametrize( "filter_type", EMBEDDED_FILTERS, ids=lambda t: t.name )
def test_perception_starts_with_filter_enabled(perception_sensor, perception_profile, embedded_filter):
    embedded_filter.set( 1.0 )
    start_perception( perception_sensor, perception_profile )
    try:
        assert embedded_filter.get() == 1.0
    finally:
        stop_perception( perception_sensor )


@pytest.mark.parametrize( "filter_type", EMBEDDED_FILTERS, ids=lambda t: t.name )
def test_filter_toggles_while_perception_streams(perception_sensor, perception_profile, embedded_filter):
    embedded_filter.set( 0.0 )
    start_perception( perception_sensor, perception_profile )
    try:
        embedded_filter.set( 1.0 )
        assert embedded_filter.get() == 1.0
        embedded_filter.set( 0.0 )
        assert embedded_filter.get() == 0.0
    finally:
        stop_perception( perception_sensor )


@pytest.mark.parametrize( "filter_type", EMBEDDED_FILTERS, ids=lambda t: t.name )
def test_filter_enables_while_perception_opened_not_streaming(perception_sensor, perception_profile, embedded_filter):
    # Covers the open()->start() window: the perception sensor is opened but not yet streaming
    embedded_filter.set( 0.0 )
    perception_sensor.open( perception_profile )
    try:
        embedded_filter.set( 1.0 )
        assert embedded_filter.get() == 1.0
        perception_sensor.start( lambda f: None )
        perception_sensor.stop()
    finally:
        perception_sensor.close()


# The DPP filters, as reported per depth frame in the embedded-filters metadata
DPP_METADATA_BIT = {
    rs.embedded_filter_type.decimation: 1 << 0,
    rs.embedded_filter_type.temporal: 1 << 2,
}
STREAM_SECONDS = 4


def video_profile(sensor, stream, width, height, fmt, fps=30):
    return next( ( p for p in sensor.get_stream_profiles()
                   if p.stream_type() == stream and p.format() == fmt and p.fps() == fps
                   and p.as_video_stream_profile().width() == width and p.as_video_stream_profile().height() == height ),
                 None )


@pytest.mark.parametrize( "filter_type", list( DPP_METADATA_BIT ), ids=lambda t: t.name )
def test_perception_streams_while_filter_processes_depth(test_device, depth_sensor, perception_sensor, perception_profile,
                                                        embedded_filter, filter_type):
    # Depth must actually stream for the filter to run on the camera. With decimation on, the FW accepts depth only at
    # 640x360 next to 1280x720 color. Color must start before perception, which runs on it.
    dev, _ = test_device
    depth_w, depth_h = ( 640, 360 ) if filter_type == rs.embedded_filter_type.decimation else ( 1280, 720 )
    depth_profile = video_profile( depth_sensor, rs.stream.depth, depth_w, depth_h, rs.format.z16 )
    color_sensor = depth_sensor  # dual-RGB devices carry color on the stereo module
    color_profile = video_profile( depth_sensor, rs.stream.color, 1280, 720, rs.format.rgb8 )
    if color_profile is None:
        color_sensor = next( ( s for s in dev.query_sensors() if s.is_color_sensor() ), None )
        color_profile = color_sensor and video_profile( color_sensor, rs.stream.color, 1280, 720, rs.format.rgb8 )
    if depth_profile is None or color_profile is None:
        pytest.skip( "No depth/color profile pair at 30 fps for this filter" )

    start = time.time()
    od_times = []
    depth_bits = []
    md = rs.frame_metadata_value.embedded_filters

    def on_frame( f ):
        if f.get_profile().stream_type() == rs.stream.object_detection:
            od_times.append( time.time() - start )
        elif f.get_profile().stream_type() == rs.stream.depth and f.supports_frame_metadata( md ):
            depth_bits.append( f.get_frame_metadata( md ) )

    if color_sensor is depth_sensor:
        stream_order = [( depth_sensor, [depth_profile, color_profile] )]
    else:
        stream_order = [( depth_sensor, depth_profile ), ( color_sensor, color_profile )]
    stream_order.append( ( perception_sensor, perception_profile ) )

    embedded_filter.set( 1.0 )
    opened = []
    started = []
    try:
        for sensor, profiles in stream_order:
            if sensor is perception_sensor:
                time.sleep( 1 )
            sensor.open( profiles )
            opened.append( sensor )
            sensor.start( on_frame )
            started.append( sensor )
        time.sleep( STREAM_SECONDS )
    finally:
        for sensor in reversed( started ):
            sensor.stop()
        for sensor in reversed( opened ):
            sensor.close()
        time.sleep( 2 )  # D555 rejects filter changes ("Option value error") for a moment after its streams close

    second_half = 1 + STREAM_SECONDS / 2
    log.info( f"{filter_type.name}: {len( od_times )} perception frames, {len( depth_bits )} depth frames with metadata" )
    assert any( t > second_half for t in od_times ), "perception frames stopped while the filter was running"
    if not depth_bits:
        log.warning( "No embedded-filters metadata on depth frames; cannot check the filter was applied" )
        return
    bit = DPP_METADATA_BIT[filter_type]
    assert sum( 1 for b in depth_bits if b & bit ) > len( depth_bits ) // 2, "depth frames do not report the filter as applied"
