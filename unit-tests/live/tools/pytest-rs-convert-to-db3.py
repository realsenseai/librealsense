# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import subprocess, os, tempfile
import logging
import pyrealsense2 as rs
from rspy import repo

log = logging.getLogger(__name__)


def start_pipeline( filename ):
    pipe = rs.pipeline()
    cfg = rs.config()
    cfg.enable_all_streams()
    cfg.enable_device_from_file( filename, repeat_playback=False )
    pipeline_profile = pipe.start( cfg )
    pipeline_profile.get_device().as_playback().set_real_time( False )
    return pipe


def collect_frames( pipe ):
    # drain the playback, keying each frame's data by (stream, frame number) so the
    # comparison is robust to how frames are batched into framesets
    frames = {}
    while True:
        ok, fset = pipe.try_wait_for_frames( 1000 )
        if not ok:
            break
        for f in fset:
            frames[( f.get_profile().stream_type(), f.get_frame_number() )] = bytes( f.get_data() )
    return frames


def test_rs_convert_bag_to_db3():
    rs_convert = repo.find_built_exe( 'tools/convert', 'rs-convert' )
    assert rs_convert, "rs-convert not found"

    bag_file = os.path.join( repo.build, 'unit-tests', 'recordings', 'single_depth_color_640x480.bag' )
    temp_dir = tempfile.mkdtemp( prefix='bag_to_db3_' )
    db3_file = os.path.join( temp_dir, 'converted.db3' )
    bag_pipe = db3_pipe = None
    try:
        p = subprocess.run( [rs_convert, '-i', bag_file, '-D', db3_file],
                            capture_output=True, text=True, timeout=60 )
        assert p.returncode == 0
        log.debug( 'converted to %s', db3_file )

        bag_pipe = start_pipeline( bag_file )
        db3_pipe = start_pipeline( db3_file )
        bag_frames = collect_frames( bag_pipe )
        db3_frames = collect_frames( db3_pipe )

        assert len( bag_frames ) > 0
        for key, data in bag_frames.items():
            assert key in db3_frames, f'frame {key} present in bag but missing in db3'
            assert data == db3_frames[key], f'frame {key} data differs after conversion'
        log.debug( '%s frames compared', len( bag_frames ) )
    finally:
        # stop in finally so pipelines are released even if collect_frames raises
        if bag_pipe:
            bag_pipe.stop()
        if db3_pipe:
            db3_pipe.stop()
        if os.path.isfile( db3_file ):
            os.remove( db3_file )
        if os.path.isdir( temp_dir ):
            os.rmdir( temp_dir )


def test_playback_legacy_bag():
    # back-compat: play the legacy .bag as-is, no conversion
    bag_file = os.path.join( repo.build, 'unit-tests', 'recordings', 'single_depth_color_640x480.bag' )
    pipe = start_pipeline( bag_file )
    try:
        frames = collect_frames( pipe )
    finally:
        pipe.stop()  # stop in finally so the pipeline is released even if collection raises
    assert len( frames ) > 0, "no frames played back from legacy bag"
