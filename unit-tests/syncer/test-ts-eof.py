# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

import pyrealsense2 as rs
from rspy import log, test
import sw


# The timestamp jumps are closely correlated to the FPS passed to the video streams:
# syncer expects frames to arrive every 1000/FPS milliseconds!
sw.fps_c = sw.fps_d = 60
sw.init()

import tempfile, os
temp_dir = tempfile.TemporaryDirectory( prefix = 'recordings_' )
filename = os.path.join( temp_dir.name, 'rec' )
recorder = rs.recorder( filename, sw.device )

sw.start()

#############################################################################################
#
test.start( "Init" )

# It can take a few frames for the syncer to actually produce a matched frameset (it doesn't
# know what to match to in the beginning)

# D  C  @timestamp  comment
# -- -- ----------- ----------------
# 0     @0          so next expected frame timestamp is at 0+16.67
#    0  @0
#
sw.generate_depth_and_color( frame_number = 0, timestamp = 0 )
sw.expect( depth_frame = 0 )                          # syncer doesn't know about C yet, so releases right away
sw.expect( color_frame = 0, nothing_else = True )     # no hope for a match: D@0 is already out, so it's released
#
# NOTE: if the syncer queue wasn't 100 (see above) then we'd only get the color frame!
# (it will output D to the queue, then C to the queue, but the queue size is 1 so we lose D)
#
# 1     @16
#    1  @16
#
sw.generate_depth_and_color( 1, sw.gap_d * 1 )
sw.expect( depth_frame = 1, color_frame = 1, nothing_else = True )  # frameset 1

test.finish()
#
#############################################################################################
#
test.start( "Keep going" )

# 2     @33
#    2  @33
#
sw.generate_depth_and_color( 2, sw.gap_d * 2 )
sw.expect( depth_frame = 2, color_frame = 2, nothing_else = True )  # frameset 2

test.finish()
#
#############################################################################################
#
test.start( "Stop giving color; nothing output" )

# 3     @50
#
sw.generate_depth_frame( 3, sw.gap_d * 3 )

# The depth frame will be kept in the syncer, and never make it out (no matching color frame
# and we're not going to push additional frames that would cause it to eventually flush):
#
sw.expect_nothing()
#
# ... BUT the file should still have it!

test.finish()
#
#############################################################################################
#
test.start( "Dump the file" )

recorder.pause()
recorder = None  # otherwise the file will be open when we exit
log.d( "filename=", filename )
sw.stop()
sw.reset()
#
# Dump it... should look like:
#     [Depth/0 #0 @0.000000]
#     [Color/1 #0 @0.000000]
#     [Depth/0 #1 @16.666667]
#     [Color/1 #1 @16.666667]
#     [Depth/0 #2 @33.333333]
#     [Color/1 #2 @33.333333]
#     [Depth/0 #3 @50.000000]    <--- the frame that was "lost"
#
from rspy import repo
rs_convert = repo.find_built_exe( 'tools/convert', 'rs-convert' )
if rs_convert:
    import subprocess
    subprocess.run( [rs_convert, '-i', filename, '-T'],
                    stdout=None,
                    stderr=subprocess.STDOUT,
                    universal_newlines=True,
                    timeout=10,
                    check=False )  # don't fail on errors
else:
    log.w( 'no rs-convert was found!' )
    import sys
    log.d( 'sys.path=\n    ' + '\n    '.join( sys.path ) )

test.finish()
#
#############################################################################################
#
test.start( "Play it back, with syncer -- lose last frame" )

sw.playback( filename + ".db3" )
sw.start()

sw.expect( depth_frame = 0 )                          # syncer doesn't know about color yet
sw.expect( color_frame = 0 )                          # less than next expected of D
sw.expect( depth_frame = 1, color_frame = 1 )
sw.expect( depth_frame = 2, color_frame = 2 )

# We know there should be another frame in the file:
#     [Depth/0 #3 @50.000000]
# ... but the syncer is keeping it inside, waiting for a matching color frame, and does not
# know that we've reached the EOF. There is a flush when we reach the EOF, but not on the
# syncer -- the playback device knows not that its client is a syncer!
#
#sw.expect( depth_frame = 3 )
sw.expect_nothing()
#
# There is no API to flush the syncer, but it can easily be added. Or we can implement a
# special frame type, an "end-of-file frame", which would cause the syncer to flush...

sw.stop()
sw.reset()

test.finish()
#
#############################################################################################
#
test.start( "Play it back, without syncer -- and now expect the lost frame" )

sw.playback( filename + ".db3", use_syncer = False )
sw.start()

sw.expect( depth_frame = 0 )  # none of these is synced (no syncer)
sw.expect( color_frame = 0 )
sw.expect( depth_frame = 1 )
sw.expect( color_frame = 1 )
sw.expect( depth_frame = 2 )
sw.expect( color_frame = 2 )

# This line is the difference from the last test:
#
sw.expect( depth_frame = 3 )

sw.expect_nothing()
sw.stop()
sw.reset()

test.finish()
#
#############################################################################################
test.print_results_and_exit()
