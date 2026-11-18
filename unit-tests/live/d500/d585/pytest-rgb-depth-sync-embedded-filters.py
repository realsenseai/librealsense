# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# RGB and Depth must stay internally synced on D585 while the decimation / temporal embedded filters run.
# The depth/color timestamp delta must stay within tolerance and the frames must pair up in the same frameset.

import pytest
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each( "D585" ),
    pytest.mark.device_exclude( "D585S" ),
    pytest.mark.context( "nightly" ),
]

MAX_DELTA_MS = 5.0
MIN_PAIRED_RATIO = 0.9
STREAM_SECONDS = 10
FPS = 30
COLOR_RES = ( 1280, 720 )

# Embedded filter (None is the baseline) -> depth resolution; the FW accepts decimated depth only at 640x360 next to 1280x720 color.
FILTERS = {
    "none": ( None, COLOR_RES ),
    "decimation": ( rs.embedded_filter_type.decimation, ( 640, 360 ) ),
    "temporal": ( rs.embedded_filter_type.temporal, COLOR_RES ),
}

# Over USB the decimation/temporal DPP filters are toggled through the 'enabled' field of their composite option
COMPOSITE = {
    rs.embedded_filter_type.decimation: ( rs.composite_option_id.decimation_filter_dpp, "decimation_filter_dpp" ),
    rs.embedded_filter_type.temporal: ( rs.composite_option_id.temporal_filter_dpp, "temporal_filter_dpp" ),
}


def set_filter( depth_sensor, filter_type, enabled ):
    ef = depth_sensor.get_embedded_filter( filter_type )
    if ef.supports( rs.option.embedded_filter_enabled ):
        ef.set_option( rs.option.embedded_filter_enabled, enabled )
        return
    option_id, name = COMPOSITE[filter_type]
    cfg = getattr( ef, f"get_{name}_config" )( option_id )
    cfg.enabled = enabled
    getattr( ef, f"set_{name}_config" )( option_id, cfg )


@pytest.fixture
def depth_resolution( test_device, request ):
    dev, _ = test_device
    filter_type, depth_res = FILTERS[request.param]
    depth_sensor = dev.first_depth_sensor()
    if filter_type is not None:
        try:
            set_filter( depth_sensor, filter_type, 1 )
        except Exception as e:
            pytest.skip( f"Embedded filter {request.param} not available: {e}" )
    yield depth_res
    if filter_type is not None:
        set_filter( depth_sensor, filter_type, 0 )


@pytest.mark.parametrize( "depth_resolution", FILTERS, indirect=True )
def test_rgb_depth_sync( test_device, depth_resolution ):
    _, ctx = test_device
    cfg = rs.config()
    cfg.enable_stream( rs.stream.depth, *depth_resolution, rs.format.z16, FPS )
    cfg.enable_stream( rs.stream.color, *COLOR_RES, rs.format.rgb8, FPS )
    pipe = rs.pipeline( ctx )
    pipe.start( cfg )
    deltas = []
    framesets = 0
    try:
        for _ in range( STREAM_SECONDS * FPS ):
            fs = pipe.wait_for_frames()
            framesets += 1
            depth, color = fs.get_depth_frame(), fs.get_color_frame()
            if depth and color:
                assert depth.get_frame_timestamp_domain() == color.get_frame_timestamp_domain()
                deltas.append( abs( depth.get_timestamp() - color.get_timestamp() ) )
    finally:
        pipe.stop()
    assert len( deltas ) >= MIN_PAIRED_RATIO * framesets, f"only {len( deltas )}/{framesets} framesets hold both depth and color"
    log.info( f"max depth/color delta {max( deltas ):.2f} ms over {len( deltas )} framesets" )
    assert max( deltas ) <= MAX_DELTA_MS, f"depth/color timestamp delta {max( deltas ):.2f} ms exceeds {MAX_DELTA_MS} ms"
