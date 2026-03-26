# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#test:donotrun:gha

import subprocess, os, tempfile
import numpy as np
import pyrealsense2 as rs
from rspy import log, repo, test


def start_pipeline( filename ):
    pipe = rs.pipeline()
    cfg = rs.config()
    cfg.enable_all_streams()
    cfg.enable_device_from_file( filename, repeat_playback=False )
    pipeline_profile = pipe.start( cfg )
    pipeline_profile.get_device().as_playback().set_real_time( False )
    return pipe


#############################################################################################
#
test.start( "Run rs-convert bag-to-db3 and verify frames" )
rs_convert = repo.find_built_exe( 'tools/convert', 'rs-convert' )
test.check( rs_convert )
if rs_convert:
    bag_file = os.path.join( repo.build, 'unit-tests', 'recordings', 'recording_deadlock.bag' )
    temp_dir = tempfile.mkdtemp( prefix='bag_to_db3_' )
    db3_file = os.path.join( temp_dir, 'converted.db3' )
    p = subprocess.run( [rs_convert, '-i', bag_file, '-D', db3_file],
                        capture_output=True, text=True, timeout=60 )
    test.check( p.returncode == 0 )
    log.d( 'converted to', db3_file )

    bag_pipe = start_pipeline( bag_file )
    db3_pipe = start_pipeline( db3_file )

    frame_count = 0
    while True:
        bag_ok, bag_fset = bag_pipe.try_wait_for_frames( 1000 )
        db3_ok, db3_fset = db3_pipe.try_wait_for_frames( 1000 )
        if not bag_ok and not db3_ok:
            log.d( f'no more frames in either pipeline: bag_ok={bag_ok} db3_ok={db3_ok}' )
            break
        if not test.check( bag_ok == db3_ok ):
            break
        test.check_equal( bag_fset.size(), db3_fset.size() )
        for bag_f, db3_f in zip( bag_fset, db3_fset ):
            frame_count += 1
            test.check( np.array_equal( np.asarray( bag_f.get_data() ), np.asarray( db3_f.get_data() ) ) )

    bag_pipe.stop()
    db3_pipe.stop()

    log.d( frame_count, 'frames compared' )
    test.check( frame_count > 0 )
else:
    log.e( 'rs-convert not found!' )
    import sys
    log.d( 'sys.path=\n    ' + '\n    '.join( sys.path ) )

test.finish()
#
#############################################################################################
test.print_results_and_exit()
