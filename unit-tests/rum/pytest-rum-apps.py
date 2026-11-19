# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import os
import sys
import json
import subprocess
import pytest
import pyrealsense2 as rs


MAX_APPS = 32   # mirrors max_apps in the SDK collector


def run_child( code ):
    # The saved report is folded in once per process, so cross-session behavior needs a fresh process.
    # Hand the child the very module this process loaded (see pytest-rum-config.py).
    env = dict( os.environ, PYTHONPATH=os.path.dirname( rs.__file__ ) + os.pathsep + os.environ.get( "PYTHONPATH", "" ) )
    subprocess.run( [ sys.executable, "-c", "import pyrealsense2 as rs\n" + code ], check=True, env=env )


def saved_apps():
    # Read the file as the child left it - flushing from this process would overwrite it
    with open( rs.rum.get_report_path(), encoding="utf-8" ) as f:
        return json.load( f ).get( "apps", {} )


def test_app_name_counted_once_per_process( rum_report ):
    rs.rum.set_app_name( "pytest-rum-apps" )
    rs.rum.set_app_name( "pytest-rum-apps" )
    assert rum_report()["apps"].get( "pytest-rum-apps" ) == 1


def test_app_runs_accumulate_across_sessions( rum_report ):
    rum_report()   # this process folds the saved report in now, not after the children write
    for _ in range( 2 ):
        run_child( "rs.rum.set_app_name( 'pytest-rum-child' ); c = rs.context(); del c" )
    assert saved_apps().get( "pytest-rum-child" ) == 2


@pytest.mark.parametrize( "name", [ "", "x" * 33, "has space", "slash/name", "ünicode" ] )
def test_invalid_app_name_rejected( name ):
    with pytest.raises( RuntimeError ):
        rs.rum.set_app_name( name )


def test_longest_valid_app_name_accepted():
    rs.rum.set_app_name( "A-z_0.9" + "x" * 25 )


def test_distinct_app_names_capped( rum_report ):
    rum_report()
    run_child( f"for i in range( {MAX_APPS + 8} ): rs.rum.set_app_name( f'cap-{{i}}' )\n"
               "c = rs.context(); del c" )
    assert len( [ name for name in saved_apps() if name.startswith( "cap-" ) ] ) == MAX_APPS
