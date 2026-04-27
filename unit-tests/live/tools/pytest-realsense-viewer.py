# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 Intel Corporation. All Rights Reserved.

import os, platform, re, shutil, subprocess, sys
import logging
import pytest
from rspy import repo

log = logging.getLogger(__name__)

ansi_escape = re.compile( rb'\x1b\[[0-9;]*m' )
frame_prefix = re.compile( rb'^\[\d{4}\] ', re.MULTILINE )

# the tests in the realsense-viewer-tests executable are defined along the tester file itself at
# tools/realsense-viewer/tests/. Using the --auto flag allows us to run all tests one by one.
# If we want to run a specific test / test group, we can use the -r flag

pytestmark = [
    pytest.mark.device("D400*"),
    pytest.mark.context("nightly"),
    pytest.mark.context("gui"),
]


def test_realsense_viewer_gui(module_device_setup):
    viewer_tests = repo.find_built_exe('tools/realsense-viewer', 'realsense-viewer-tests')
    assert viewer_tests, "realsense-viewer-tests not found"

    cmd = []
    env = None

    # On headless Linux (no $DISPLAY), wraps the executable with xvfb-run for a virtual display.
    if platform.system() == 'Linux' and not os.environ.get( 'DISPLAY' ):
        xvfb = shutil.which( 'xvfb-run' )
        if not xvfb:
            pytest.skip( 'No DISPLAY and xvfb-run not found; install xvfb (apt install xvfb)' )
        log.debug( 'no DISPLAY set; using xvfb-run with software rendering' )
        cmd += [xvfb, '-a']
        env = dict( os.environ, LIBGL_ALWAYS_SOFTWARE='1' )

    # On some Windows machines OpenGL is not available, so we use Mesa's software renderer to provide it
    # On those machines we expect Mesa's OpenGL implementation dll files under C:\mesa
    # URL: https://github.com/pal1000/mesa-dist-win/releases
    if platform.system() == 'Windows':
        mesa_dir = r'C:\mesa'
        exe_dir = os.path.dirname( viewer_tests )
        for dll in ['opengl32.dll', 'libgallium_wgl.dll']:
            src = os.path.join( mesa_dir, dll )
            dst = os.path.join( exe_dir, dll )
            if os.path.isfile( src ) and not os.path.isfile( dst ):
                log.debug( 'copying Mesa %s to %s', dll, exe_dir )
                shutil.copy2( src, dst )

    cmd += [viewer_tests, '--auto']
    log.debug( 'running: %s', ' '.join( cmd ) )
    # Cap the child below the global pytest-timeout (200s default in conftest.py) so the
    # subprocess is reaped here rather than leaked when the outer test thread is killed.
    child_timeout = 180
    p = subprocess.Popen( cmd,
                          stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE,
                          env=env )
    try:
        stdout, stderr = p.communicate( timeout=child_timeout )
    except subprocess.TimeoutExpired:
        p.kill()
        stdout, stderr = p.communicate()
        # Strip ANSI / frame prefix before logging so the failure message is readable
        stdout = ansi_escape.sub( b'', stdout )
        stdout = frame_prefix.sub( b'', stdout )
        sys.stdout.buffer.write( stdout )
        sys.stderr.buffer.write( stderr )
        pytest.fail( f'realsense-viewer-tests did not complete within {child_timeout}s' )

    # Strip ANSI color codes and imgui test engine frame-count prefix
    stdout = ansi_escape.sub( b'', stdout )
    stdout = frame_prefix.sub( b'', stdout )
    sys.stdout.buffer.write( stdout )
    # Filter Mesa glCopyTexImage2D warnings that are irrelevant with software rendering
    for line in stderr.split( b'\n' ):
        if line and b'glCopyTexImage2D' not in line:
            sys.stderr.buffer.write( line + b'\n' )
    if p.returncode != 0:
        log.error( 'realsense-viewer-tests exited with code %s', p.returncode )
    assert p.returncode == 0, f'realsense-viewer-tests exited with code {p.returncode}'
