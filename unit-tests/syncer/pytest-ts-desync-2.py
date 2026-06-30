# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import logging
import pytest
import pyrealsense2 as rs
import sw_syncer as sw

log = logging.getLogger(__name__)


#
# This is a reproduction of a problem we saw with externally-synced cameras, where the master
# camera is OK but the slave cameras generate delayed color frames (Depth at time X arrives 4ms
# sooner than Color at time X-18ms) with exaggerated next-expected because of low-calculated-fps
# (29 versus the nominal 30 that's supposed to be seen).
#
# This should no longer happen as recorded because the captured FPS, and therefore estimated
# next-expected, is much more accurate: part of the problem was that ACTUAL_FPS was crudely
# misrepresented (29.98 became 29). Nonetheless, with the below capture times, this is the behavior
# we expect.
#
# See [RSDSO-19336]
#


# Module-scope autouse fixture: pytest runs the code before `yield` once before
# the first test in this file, the tests run at the `yield` point, and the code
# after `yield` runs once after the last test. autouse=True wires it into every
# test in the module without needing to declare it as a parameter.
@pytest.fixture(scope="module", autouse=True)
def _sw_session():
    sw.fps_d = 30
    sw.fps_c = 30
    sw.init()
    sw.start()
    yield  # tests in this module run here
    sw.stop()
    sw.reset()


#############################################################################################
def test_ts_desync_2():
    log.debug( "Init" )
    # It can take a few frames for the syncer to actually produce a matched frameset (it doesn't
    # know what to match to in the beginning)

    sw.generate_color_frame( frame_number=1730, timestamp=63198.01542, next_expected=63231.348758 )
    sw.expect( color_frame=1730 )   # syncer doesn't know about D yet, so releases right away
    sw.generate_depth_frame( frame_number=1742, timestamp=63214.73 )
    sw.expect_nothing()  # should be waiting for next color

    #############################################################################################
    log.debug( "Sync D1742 & C1731" )
    sw.generate_depth_frame( frame_number=1743, timestamp=63248.07 )
    sw.expect_nothing()  # still waiting for next color with 1742
    sw.generate_color_frame( frame_number=1731, timestamp=63230.22, next_expected=63264.698758 )
    sw.expect( depth_frame=1742, color_frame=1731, nothing_else=True )     # no hope for a match: D1743 is already out, so it's released

    #############################################################################################
    log.debug( "Sync D1743 & C1732" )
    sw.generate_depth_frame( frame_number=1744, timestamp=63281.41 )
    sw.expect_nothing()  # should be waiting for next color
    sw.generate_color_frame( frame_number=1732, timestamp=63263.57, next_expected=63298.049758 )
    sw.expect( depth_frame=1743, color_frame=1732, nothing_else=True )

    #############################################################################################
    log.debug( "Sync D1744 & C1733" )
    sw.generate_depth_frame( frame_number=1745, timestamp=63314.76 )
    sw.expect_nothing()  # should be waiting for next color
    sw.generate_color_frame( frame_number=1733, timestamp=63296.92, next_expected=63331.399758 )
    sw.expect( depth_frame=1744, color_frame=1733, nothing_else=True )

    #############################################################################################
    log.debug( "Sync D1745 & C1734" )
    sw.generate_depth_frame( frame_number=1746, timestamp=63348.10 )
    sw.expect_nothing()  # should be waiting for next color
    sw.generate_color_frame( frame_number=1734, timestamp=63330.27, next_expected=63364.750758 )
    sw.expect( depth_frame=1745, color_frame=1734, nothing_else=True )

    #############################################################################################
    log.debug( "Sync D1746 & C1735" )
    sw.generate_depth_frame( frame_number=1747, timestamp=63381.45 )
    sw.expect_nothing()  # should be waiting for next color
    sw.generate_color_frame( frame_number=1735, timestamp=63363.62, next_expected=63398.100758 )
    sw.expect( depth_frame=1746, color_frame=1735, nothing_else=True )

    #############################################################################################
    log.debug( "Sync D1747 & C1736" )
    sw.generate_depth_frame( frame_number=1748, timestamp=63414.79 )
    sw.expect_nothing()  # should be waiting for next color
    sw.generate_color_frame( frame_number=1736, timestamp=63396.97, next_expected=63431.451758 )
    sw.expect( depth_frame=1747, color_frame=1736, nothing_else=True )

    #############################################################################################
    log.debug( "Sync D1748 & C1737" )
    sw.generate_depth_frame( frame_number=1749, timestamp=63448.13 )
    sw.expect_nothing()  # should be waiting for next color
    sw.generate_color_frame( frame_number=1737, timestamp=63430.32, next_expected=63464.801758 )
    sw.expect( depth_frame=1748, color_frame=1737 )

    #############################################################################################
    log.debug( "LONE D1749!" )
    sw.expect( depth_frame=1749, nothing_else=True )

    #############################################################################################
    log.debug( "No sync on D1750 & C1738" )
    sw.generate_depth_frame( frame_number=1750, timestamp=63481.48 )
    sw.expect_nothing()  # should be waiting for next color
    sw.generate_color_frame( frame_number=1738, timestamp=63463.67, next_expected=63498.152758 )
    sw.expect( color_frame=1738 )
    sw.expect( depth_frame=1750 )
