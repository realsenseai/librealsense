# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Device-map health check: compare the cameras expected on this machine (from the
inventory given with --device-map) against what actually enumerated, and fail
with one attributable INFRA message when a camera is missing.

Two roles in CI, same test:

1. Run on its own before the main session, it writes health.json, health.txt
   and health-exclude.txt (the missing products, comma-separated) next to
   pytest-results.xml. The pipeline feeds that list to --exclude-device so the
   rest of the suite skips the dead camera cleanly instead of cascading.
2. Inside the main session, at priority 2 (right after pytest-fw-update), it is
   the verdict: a camera still in recovery at this point failed to recover.

No map, or no entry for this machine: warn and pass, nothing else changes.
"""

import logging
import os

import pytest

from rspy import device_map
from rspy.pytest.device_helpers import split_cli_patterns

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.infra,        # no device requirement, but must run under --live
    pytest.mark.priority(2),  # after pytest-fw-update (1), before everything else
]

HEALTH_JSON = 'health.json'
EXCLUDE_FILE = 'health-exclude.txt'
HTML_FILE = 'health.html'

# Per-camera colour in the HTML line (Jenkins report / build description)
_COLOR = {
    device_map.OK: 'Green',
    device_map.MISSING: 'Red',
    device_map.DEGRADED: 'Red',
    device_map.IN_RECOVERY: 'DarkOrange',
    device_map.PRE_EXCLUDED: 'Gray',
    device_map.UNEXPECTED: 'Gray',
}


def unusable_products( report ):
    """Products the rest of the run should exclude: missing or degraded cameras"""
    return [c['product'] for c in report['cameras']
            if c['state'] in (device_map.MISSING, device_map.DEGRADED)]


def verdict( report ):
    """
    :return: the failure message for a report, or None when the machine is healthy
             (or has no map entry)
    """
    status = report['status']
    node = report['node']
    if status == device_map.STATUS_NO_MAP:
        return None
    if status == device_map.STATUS_TOTAL_ENUMERATION_FAILURE:
        return f'INFRA: no devices enumerated on {node}; expected {len( report["cameras"] )} ' \
               f'(hub or machine-level failure)'
    problems = []
    for c in report['cameras']:
        if c['state'] == device_map.MISSING:
            problems.append( f'{c["product"]} (SN {c["sn"]}) not detected' )
        elif c['state'] == device_map.DEGRADED:
            problems.append( f'{c["product"]} (SN {c["sn"]}) degraded: {c.get( "detail", "?" )}' )
    if not problems:
        return None
    return f'INFRA: camera problem on {node}: ' + '; '.join( problems ) + \
           '. Excluded from this run; the remaining results are valid'


def render_html( report ):
    """
    The health line as HTML, one coloured span per camera:
    <machine>: <green>D455 ok</green> | <red>D435 MISSING</red> | ...
    """
    node = report['node']
    status = report['status']
    if status == device_map.STATUS_NO_MAP:
        return f'{node}: no device map'
    if status == device_map.STATUS_TOTAL_ENUMERATION_FAILURE:
        return f'{node}: <span style="color:Red"><b>NO DEVICES ENUMERATED</b></span> (expected {len( report["cameras"] )})'
    parts = []
    for c in report['cameras']:
        state = c['state']
        bad = state in (device_map.MISSING, device_map.DEGRADED)
        text = f'{c["product"]} {state.upper() if bad else state}'
        if state == device_map.DEGRADED and c.get( 'detail' ):
            text += f' ({c["detail"]})'
        span = f'<span style="color:{_COLOR.get( state, "Black" )}">' + (f'<b>{text}</b>' if bad else text) + '</span>'
        parts.append( span )
    return f'{node}: ' + ' | '.join( parts )


def write_reports( report, logdir ):
    device_map.write_health_json( report, os.path.join( logdir, HEALTH_JSON ) )
    with open( os.path.join( logdir, EXCLUDE_FILE ), 'w' ) as f:
        f.write( ','.join( unusable_products( report ) ) )
    with open( os.path.join( logdir, HTML_FILE ), 'w' ) as f:
        f.write( render_html( report ) + '\n' )


def test_map_check( request ):
    config = request.config
    map_file = config.getoption( '--device-map', default = None ) or None
    excludes = split_cli_patterns( config.getoption( '--exclude-device', default = [] ) )

    report = device_map.check( exclude_specs = excludes, runner = 'pytest', map_file = map_file )

    log.info( device_map.render_line( report ) )
    for c in report['cameras']:
        log.debug( '%s', c )

    logdir = getattr( config, '_test_logdir', None )
    if logdir:
        write_reports( report, logdir )

    if report['status'] == device_map.STATUS_NO_MAP:
        log.warning( 'no device-map entry for %s; health check skipped', report['node'] )
        return

    message = verdict( report )
    if message:
        pytest.fail( message )
