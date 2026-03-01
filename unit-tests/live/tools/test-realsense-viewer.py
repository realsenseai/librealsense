# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 Intel Corporation. All Rights Reserved.

#test:device D400*

import os, platform, shutil, subprocess
from rspy import log, repo, test

#############################################################################################
#

# the tests in the realsense-viewer-tests executable are defined along the tester file itself at
# tools/realsense-viewer/tests/. Using the --auto flag allows us to run all tests one by one.
# If we want to run a specific test / test group, we can use the -r flag

# Headless Linux: use xvfb-run for a virtual display and Mesa software rendering
cmd = []
env = None
# Headless Linux: use xvfb-run for a virtual display and Mesa software rendering
if platform.system() == 'Linux' and not os.environ.get( 'DISPLAY' ):
    xvfb = shutil.which( 'xvfb-run' )
    if not xvfb:
        log.f( 'No DISPLAY and xvfb-run not found; install xvfb (apt install xvfb)' )
        test.print_results_and_exit()
    log.d( 'no DISPLAY set; using xvfb-run with software rendering' )
    cmd += [xvfb, '-a']
    env = dict( os.environ, LIBGL_ALWAYS_SOFTWARE='1' )

test.start( "Run realsense-viewer GUI tests" )
viewer_tests = repo.find_built_exe( 'tools/realsense-viewer', 'realsense-viewer-tests' )
test.check( viewer_tests )
if viewer_tests:
    cmd += [viewer_tests, '--auto']
    log.d( 'running:', *cmd )
    p = subprocess.run( cmd,
                        stdout=None,
                        stderr=subprocess.STDOUT,
                        timeout=300,
                        check=False,
                        env=env )
    if p.returncode != 0:
        log.e( 'realsense-viewer-tests exited with code', p.returncode )
    test.check( p.returncode == 0 )
else:
    log.e( 'realsense-viewer-tests was not found!' )
    import sys
    log.d( 'sys.path=\n    ' + '\n    '.join( sys.path ) )

test.finish()
#
#############################################################################################
test.print_results_and_exit()
