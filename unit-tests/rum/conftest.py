# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import os
import json
import pytest
import pyrealsense2 as rs


@pytest.fixture( scope="session", autouse=True )
def preserve_rum_report():
    # These tests read/write the production report path (the SDK persists there on every context
    # teardown). Back up the user's real report, clear it so the tests start empty, and restore it
    # afterward so running the suite never destroys or pollutes real telemetry.
    path = rs.rum.get_report_path()
    backup = None
    if os.path.exists( path ):
        with open( path, "rb" ) as f:
            backup = f.read()
        os.remove( path )
    yield
    if backup is not None:
        os.makedirs( os.path.dirname( path ), exist_ok=True )
        with open( path, "wb" ) as f:
            f.write( backup )
    elif os.path.exists( path ):
        os.remove( path )


def _read_report():
    # The report is written to disk when a context is destroyed; create+destroy a throwaway context
    # to flush the process-wide collector, then read the file the SDK points us at.
    c = rs.context(); del c   # refcount drops to 0 -> destructor runs -> report flushed
    with open( rs.rum.get_report_path(), encoding="utf-8" ) as f:
        return json.load( f )


@pytest.fixture
def rum_report():
    """A callable returning the current on-disk RUM report as a dict. Skips the test on a build that
    can't produce one: when ENABLE_STATS is off the persist hook is a no-op, so no file materializes."""
    try:
        report = _read_report()
    except FileNotFoundError:
        pytest.skip( "SDK built with ENABLE_STATS=OFF" )
    if not report.get( "sdk", {} ).get( "cmake_flags", {} ).get( "ENABLE_STATS", False ):
        pytest.skip( "SDK built with ENABLE_STATS=OFF" )
    return _read_report
