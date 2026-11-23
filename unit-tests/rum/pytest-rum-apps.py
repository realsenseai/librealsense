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


def saved_report():
    # Read the file as the child left it - flushing from this process would overwrite it
    with open( rs.rum.get_report_path(), encoding="utf-8" ) as f:
        return json.load( f )


def saved_apps():
    return saved_report().get( "apps", {} )


def test_last_app_name_wins( rum_report ):
    rs.rum.set_app_name( "pytest-rum-lower" )
    rs.rum.set_app_name( "pytest-rum-upper" )
    apps = rum_report()["apps"]
    assert apps.get( "pytest-rum-upper" ) == 1
    assert "pytest-rum-lower" not in apps


def test_python_default_app_name( rum_report ):
    rum_report()   # this process folds the saved report in now, not after the child writes
    before = saved_apps().get( "pyrealsense2", 0 )
    run_child( "c = rs.context(); del c" )
    assert saved_apps().get( "pyrealsense2" ) == before + 1


def test_app_runs_accumulate_across_sessions( rum_report ):
    rum_report()
    for _ in range( 2 ):
        run_child( "rs.rum.set_app_name( 'pytest-rum-child-lower' ); rs.rum.set_app_name( 'pytest-rum-child' ); "
                   "c = rs.context(); del c" )
    apps = saved_apps()
    assert apps.get( "pytest-rum-child" ) == 2
    assert "pytest-rum-child-lower" not in apps


@pytest.mark.parametrize( "name", [ "", "x" * 33, "has space", "slash/name", "ünicode" ] )
def test_invalid_app_name_rejected( name ):
    with pytest.raises( RuntimeError ):
        rs.rum.set_app_name( name )


def test_longest_valid_app_name_accepted():
    rs.rum.set_app_name( "A-z_0.9" + "x" * 25 )


def test_distinct_app_names_capped( rum_report ):
    rum_report()
    report = saved_report()
    report["apps"] = { f"cap-{i}": 1 for i in range( MAX_APPS ) }
    with open( rs.rum.get_report_path(), "w", encoding="utf-8" ) as f:
        json.dump( report, f )
    run_child( "rs.rum.set_app_name( 'cap-new' ); c = rs.context(); del c" )
    assert "cap-new" not in saved_apps()   # a new name does not fit
    run_child( "rs.rum.set_app_name( 'cap-0' ); c = rs.context(); del c" )
    apps = saved_apps()
    assert len( apps ) == MAX_APPS and apps["cap-0"] == 2   # a saved name still counts
