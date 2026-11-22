# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# RGB and Depth must stay internally synced on D585 while the decimation / temporal embedded filters run.
# Without filters the depth/color timestamp delta must meet the 2 ms sync KPI; with filters it must stay within one frame period.

import statistics
import pytest
import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy.pytest.device_helpers import require_min_fw_version
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each( "D585" ),
    pytest.mark.device_exclude( "D585S" ),
    pytest.mark.context( "nightly" ),
]

SYNC_KPI_MS = 2.0  # synchronized pair, no embedded filter
MIN_PAIRED_RATIO = 0.9
STREAM_SECONDS = 10
WARMUP_FRAMESETS = 15
FPS = 30
FRAME_PERIOD_MS = 1000.0 / FPS  # with embedded filters the pair must at least arrive within one frame
COLOR_RES = ( 1280, 720 )
# First FW stamping the frame timestamp with its capture time, so the temporal filter latency no longer shifts the depth/color pairing
TEMPORAL_MIN_FW = { "USB": rsutils.version( "7.59.47005.17570" ), "GMSL": rsutils.version( "7.62.50000.0" ) }  # GMSL: placeholder until FW names the version

# name -> ( embedded filter, its 'applied' bit in the depth frame metadata, depth resolution ). None is the baseline.
# The FW accepts decimated depth only at 640x360 next to 1280x720 color.
FILTERS = {
    "none": ( None, 0, COLOR_RES ),
    "decimation": ( rs.embedded_filter_type.decimation, 1 << 0, ( 640, 360 ) ),
    "temporal": ( rs.embedded_filter_type.temporal, 1 << 2, COLOR_RES ),
}

# Over USB the decimation/temporal DPP filters are toggled through the 'enabled' field of their composite option
COMPOSITE = {
    rs.embedded_filter_type.decimation: ( rs.composite_option_id.decimation_filter_dpp, "decimation_filter_dpp" ),
    rs.embedded_filter_type.temporal: ( rs.composite_option_id.temporal_filter_dpp, "temporal_filter_dpp" ),
}


class FilterToggle:
    def __init__( self, ef, filter_type ):
        self._ef = ef
        self._composite = None if ef.supports( rs.option.embedded_filter_enabled ) else COMPOSITE[filter_type]

    def get( self ):
        if not self._composite:
            return int( self._ef.get_option( rs.option.embedded_filter_enabled ) )
        option_id, name = self._composite
        return getattr( self._ef, f"get_{name}_config" )( option_id ).enabled

    def set( self, enabled ):
        if not self._composite:
            return self._ef.set_option( rs.option.embedded_filter_enabled, enabled )
        option_id, name = self._composite
        cfg = getattr( self._ef, f"get_{name}_config" )( option_id )
        cfg.enabled = enabled
        getattr( self._ef, f"set_{name}_config" )( option_id, cfg )


@pytest.fixture
def filter_setup( test_device, request ):
    dev, _ = test_device
    if dev.get_info( rs.camera_info.connection_type ) == "GMSL":
        require_min_fw_version( dev, rsutils.version( "7.62.0.0" ), "temporal DPP and pipeline start/stop on GMSL" )
    if request.param == "temporal":
        transport = "GMSL" if dev.get_info( rs.camera_info.connection_type ) == "GMSL" else "USB"
        require_min_fw_version( dev, TEMPORAL_MIN_FW[transport], f"capture-time frame timestamps with the temporal filter over {transport}" )
    filter_type, applied_bit, depth_res = FILTERS[request.param]
    toggles = {}  # only the filter under test is enabled; the others are off, whatever the FW default is
    for t in COMPOSITE:
        try:
            toggles[t] = FilterToggle( dev.first_depth_sensor().get_embedded_filter( t ), t )  # throws when not present
        except RuntimeError:
            if t == filter_type:
                pytest.skip( f"Embedded filter {request.param} not present on this device" )
    initial = { t: toggle.get() for t, toggle in toggles.items() }
    for t, toggle in toggles.items():
        toggle.set( int( t == filter_type ) )
    yield applied_bit, depth_res
    for t, toggle in toggles.items():
        toggle.set( initial[t] )


@pytest.mark.parametrize( "filter_setup", FILTERS, indirect=True )
def test_rgb_depth_sync( test_device, filter_setup ):
    dev, ctx = test_device
    applied_bit, depth_res = filter_setup
    cfg = rs.config()
    cfg.enable_device( dev.get_info( rs.camera_info.serial_number ) )
    cfg.enable_stream( rs.stream.depth, *depth_res, rs.format.z16, FPS )
    cfg.enable_stream( rs.stream.color, *COLOR_RES, rs.format.rgb8, FPS )
    pipe = rs.pipeline( ctx )
    profile = pipe.start( cfg )
    for s in profile.get_device().query_sensors():  # pipeline sensors start with global time on; frame timestamps must be the HW clock
        if s.supports( rs.option.global_time_enabled ):
            s.set_option( rs.option.global_time_enabled, 0 )
    deltas = []
    framesets = 0
    filtered = 0
    md = rs.frame_metadata_value.embedded_filters
    hw_md = rs.frame_metadata_value.frame_timestamp
    try:
        for i in range( WARMUP_FRAMESETS + STREAM_SECONDS * FPS ):
            fs = pipe.wait_for_frames()
            if i < WARMUP_FRAMESETS:
                continue
            framesets += 1
            depth, color = fs.get_depth_frame(), fs.first_or_default( rs.stream.color )  # dual RGB: color has index 1
            if not depth or not color:
                continue
            for f in ( depth, color ):
                assert f.get_frame_timestamp_domain() == rs.timestamp_domain.hardware_clock, "timestamp is not the HW clock"
                assert f.supports_frame_metadata( hw_md ), "frame has no HW timestamp metadata"
            deltas.append( abs( depth.get_frame_metadata( hw_md ) - color.get_frame_metadata( hw_md ) ) / 1000.0 )  # usec -> ms
            if depth.supports_frame_metadata( md ) and depth.get_frame_metadata( md ) & applied_bit:
                filtered += 1
    finally:
        pipe.stop()
    assert len( deltas ) >= MIN_PAIRED_RATIO * framesets, f"only {len( deltas )}/{framesets} framesets hold both depth and color"
    if applied_bit:
        assert filtered > len( deltas ) // 2, "depth frames do not report the filter as applied"
    ordered = sorted( deltas )
    median, p95 = statistics.median( ordered ), ordered[int( 0.95 * ( len( ordered ) - 1 ) )]
    stats = f"depth/color delta over {len( deltas )} framesets: median {median:.2f} ms, p95 {p95:.2f} ms, max {ordered[-1]:.2f} ms"
    log.info( stats )
    if applied_bit:
        assert ordered[-1] < FRAME_PERIOD_MS, f"{stats}; max exceeds one frame period ({FRAME_PERIOD_MS:.1f} ms)"
    else:
        assert median < SYNC_KPI_MS, f"{stats}; median exceeds the {SYNC_KPI_MS} ms sync KPI"
