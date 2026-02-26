# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 Intel Corporation. All Rights Reserved.

#test:device D400*

import subprocess
from rspy import log, repo, test

#############################################################################################
#

# the tests in the realsense-viewer-tests executable are defined along the tester file itself at
# tools/realsense-viewer/tests/. Using the --auto flag allows us to run all tests one by one.
# If we want to run a specific test / test group, we can use the -r flag

test.start( "Run realsense-viewer GUI tests" )
viewer_tests = repo.find_built_exe( 'tools/realsense-viewer', 'realsense-viewer-tests' )
test.check( viewer_tests )
if viewer_tests:
    log.d( 'running:', viewer_tests, '--auto' )
    p = subprocess.run( [viewer_tests, '--auto'],
                        stdout=None,
                        stderr=subprocess.STDOUT,
                        timeout=300,
                        check=False )
    test.check( p.returncode == 0 )
else:
    log.e( 'realsense-viewer-tests was not found!' )
    import sys
    log.d( 'sys.path=\n    ' + '\n    '.join( sys.path ) )

test.finish()
#
#############################################################################################
test.print_results_and_exit()
