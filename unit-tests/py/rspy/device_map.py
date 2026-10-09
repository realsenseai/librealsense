# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Per-machine device inventory ("device map") and health classification.

The map is a YAML file listing, per CI machine, the cameras that are expected to
be connected. The health check compares it against what actually enumerated (the
rspy.devices state after devices.query()) and classifies each expected camera:

    ok            enumerated in normal mode
    in-recovery   enumerated in DFU/recovery mode (recovering it is fw-update's job)
    missing       not found in any form
    degraded      reserved for link-quality checks (set at verdict time)
    pre-excluded  excluded before the run (--exclude-device / exceptions.specs);
                  not checked and never affects the verdict

Map schema (see also the inventory file in the deploy repo):

    nodes:
      <machine hostname>:
        cameras:
          - product: D455           # spec-matching name (devices.by_spec semantics)
            sn: "213622252410"      # serial_number in normal mode
            fwid: "213323050512"    # firmware_update_id: the identity a device in
                                    # DFU/recovery enumerates under
            connection: USB         # optional: USB (default) | DDS | GMSL

Which hub port a camera sits on is a lab wiring detail and is deliberately not
part of the map: only the camera set is required.

This module only classifies and reports; it never touches hub ports and never
flashes. Call check() after devices.query().
"""

import json
import os
import socket
import time

from rspy import log

# Per-camera states
OK = 'ok'
IN_RECOVERY = 'in-recovery'
MISSING = 'missing'
DEGRADED = 'degraded'
PRE_EXCLUDED = 'pre-excluded'
UNEXPECTED = 'unexpected'

# Run-level statuses
STATUS_OK = 'ok'
STATUS_DEGRADED = 'degraded'
STATUS_TOTAL_ENUMERATION_FAILURE = 'total-enumeration-failure'
STATUS_NO_MAP = 'no-map'

SCHEMA_VERSION = 1

# Last report produced by check(), for consumers in the same process (e.g. the
# map-check verdict test)
_report = None


def default_map_path():
    """The map lives with the machine's other CI collaterals (like exceptions.specs)"""
    from rspy import libci
    return os.path.join( libci.home, 'device-map.yaml' )


def load_map( path = None ):
    """
    :return: the 'nodes' dictionary of the inventory, or None if there is no map
    """
    path = path or default_map_path()
    if not path or not os.path.isfile( path ):
        log.d( f'no device map at {path}' )
        return None
    import yaml
    with open( path ) as f:
        data = yaml.safe_load( f )
    nodes = data.get( 'nodes' ) if isinstance( data, dict ) else None
    if not nodes:
        log.w( f'device map {path} has no "nodes" section' )
        return None
    # YAML parses unquoted serial numbers as integers; everything downstream
    # compares strings
    for entry in nodes.values():
        if not isinstance( entry, dict ):
            continue  # a node with nothing under it parses as None
        for camera in entry.get( 'cameras' ) or []:
            for key in ('sn', 'fwid'):
                if camera.get( key ) is not None:
                    camera[key] = str( camera[key] )
    return nodes


def node_name():
    return socket.gethostname()


def resolve_node( nodes, name = None ):
    """
    :return: this machine's map entry, or None if it has none
    """
    key = resolve_node_key( nodes, name )
    return nodes[key] if key is not None else None


def resolve_node_key( nodes, name = None ):
    """
    :return: the map key for this machine (the inventory's spelling of its name), or None
    """
    if not nodes:
        return None
    name = name or node_name()
    if name in nodes:
        return name
    # Map keys may differ from the hostname in case, and one side may be an FQDN
    # while the other is the short name (Jenkins node names vs socket.gethostname())
    short = name.split( '.' )[0].lower()
    for key in nodes:
        if key.split( '.' )[0].lower() == short:
            return key
    return None


def _product_line_of( product ):
    """D455 -> D400; D555/D585S -> D500"""
    p = str( product ).upper()
    if p.startswith( 'D4' ):
        return 'D400'
    if p.startswith( 'D5' ):
        return 'D500'
    return None


def spec_matches_camera( spec, camera ):
    """
    Match an exclusion spec against MAP data, not against enumerated devices --
    a pre-excluded camera may also be unplugged, in which case it would never
    enumerate and devices.by_spec() could not see it.
    Same semantics as devices.by_spec: a trailing '*' means product line, an
    exact serial/fwid matches, anything else is a substring of the product name.
    """
    product = str( camera.get( 'product' ) or '' )
    if spec in (camera.get( 'sn' ), camera.get( 'fwid' )):
        return True
    if spec.endswith( '*' ):
        prefix = spec[:-1].upper()
        return product.upper().startswith( prefix ) or _product_line_of( product ) == prefix
    return bool( spec ) and spec.upper() in product.upper()


def _record_fwid( result, camera, device ):
    """
    Report the camera's firmware_update_id: confirm the one in the map, or
    surface an observed one so a map entry missing it can be filled in.
    """
    observed = observed_fwid( device )
    if not observed:
        return
    expected = camera.get( 'fwid' )
    if not expected:
        result['fwid'] = observed
        result['fwid_source'] = 'observed'  # copy this into the inventory
    elif expected != observed:
        result['fwid'] = observed
        result['fwid_source'] = f'observed, map says {expected}'
        log.w( f"{camera.get( 'product' )} {camera.get( 'sn' )}: map fwid {expected} "
               f"but device reports {observed}" )
    else:
        result['fwid'] = expected


def observed_fwid( device ):
    """
    A device's firmware_update_id: the identity it enumerates under when in
    DFU/recovery. Normal-mode devices expose it too (D400 registers its asic
    serial, D500 its optical-module SN), so a healthy run can report the fwid
    for cameras whose map entry doesn't have one yet.
    """
    from rspy import devices
    rs = getattr( devices, 'rs', None )
    if not rs or not device:
        return None
    try:
        handle = device.handle
        if handle.supports( rs.camera_info.firmware_update_id ):
            return handle.get_info( rs.camera_info.firmware_update_id )
    except Exception as e:
        log.d( f'could not read firmware_update_id: {e}' )
    return None


def check( exclude_specs = None, node = None, runner = None, map_file = None ):
    """
    Classify this machine's expected cameras against what actually enumerated.
    Call after devices.query(). The report is cached for get_report().

    :param exclude_specs: specs already excluded from the run (--exclude-device,
                          exceptions.specs); matching cameras are reported
                          pre-excluded and are not checked
    :param node: override the machine name (default: hostname)
    :param runner: 'legacy' / 'pytest', recorded in the report
    :param map_file: override the map path (default: see default_map_path())
    :return: the health report dictionary (health.json schema)
    """
    global _report
    from rspy import devices

    name = node or node_name()
    nodes = load_map( map_file )
    key = resolve_node_key( nodes, name )
    entry = nodes[key] if key is not None else None
    if not isinstance( entry, dict ):
        entry = None  # an empty node entry is the same as no entry
    if key is not None:
        name = key  # report under the inventory's spelling, not the FQDN the OS returns
    report = {
        'schema': SCHEMA_VERSION,
        'node': name,
        'runner': runner,
        'timestamp': time.strftime( '%Y-%m-%dT%H:%M:%SZ', time.gmtime() ),
        'status': STATUS_NO_MAP,
        'cameras': [],
    }
    if not entry:
        log.w( f'no device-map entry for machine "{name}"; health check skipped' )
        _report = report
        return report

    expected = entry.get( 'cameras' ) or []
    enumerated = set( devices.all() )
    recovery_sns = set( devices.recovery() )
    matched = set()
    unconfirmed = []  # cameras with no exact match, candidates for the recovery heuristic

    for camera in expected:
        sn = camera.get( 'sn' )
        fwid = camera.get( 'fwid' )
        result = { 'product': camera.get( 'product' ), 'sn': sn }
        report['cameras'].append( result )

        matching_spec = next( (spec for spec in exclude_specs or [] if spec_matches_camera( spec, camera )), None )
        if matching_spec is not None:
            result['state'] = PRE_EXCLUDED
            result['by'] = matching_spec
        elif sn in enumerated and sn not in recovery_sns:
            result['state'] = OK
            matched.add( sn )
            _record_fwid( result, camera, devices.get( sn ) )
        elif sn in recovery_sns or (fwid and fwid in enumerated):
            # A device in DFU/recovery has no serial_number and enumerates under
            # its firmware_update_id (see devices.query())
            result['state'] = IN_RECOVERY
            matched.add( sn if sn in recovery_sns else fwid )
        else:
            result['state'] = MISSING  # may be downgraded to in-recovery below
            unconfirmed.append( (camera, result) )

    # Heuristic for map entries without fwid: an unmatched recovery-mode device of
    # the same product line as an unaccounted-for expected camera is most likely
    # that camera in DFU -- report in-recovery (unconfirmed) rather than missing
    unmatched_recovery = recovery_sns - matched
    for camera, result in unconfirmed:
        if camera.get( 'fwid' ):
            continue  # had an exact DFU identity to match against; missing is missing
        line = _product_line_of( camera.get( 'product' ) )
        if line is None:
            continue  # unknown product line: nothing to match a recovery device against
        for rec_sn in sorted( unmatched_recovery ):
            device = devices.get( rec_sn )
            if device and device.product_line == line:
                result['state'] = IN_RECOVERY
                result['note'] = 'unconfirmed -- matched by product line only (no fwid in map)'
                unmatched_recovery.discard( rec_sn )
                matched.add( rec_sn )
                break

    # Extra connected devices not in the map: report for drift visibility, never fail
    known = matched           | { c['sn'] for c in expected if c.get( 'sn' ) }           | { c['fwid'] for c in expected if c.get( 'fwid' ) }
    for sn in sorted( enumerated - known ):
        device = devices.get( sn )
        report['cameras'].append( {
            'product': device.name if device else None,
            'sn': sn,
            'state': UNEXPECTED,
        } )

    n_checked = sum( 1 for c in report['cameras'] if c['state'] not in (PRE_EXCLUDED, UNEXPECTED) )
    n_missing = sum( 1 for c in report['cameras'] if c['state'] == MISSING )
    if n_checked > 0 and not enumerated:
        report['status'] = STATUS_TOTAL_ENUMERATION_FAILURE
    elif n_missing > 0:
        report['status'] = STATUS_DEGRADED
    else:
        report['status'] = STATUS_OK

    _report = report
    return report


def get_report():
    """The report from the last check() in this process, or None"""
    return _report


def missing_products( report = None ):
    """Products classified missing -- feeds the run's device-exclusion mechanism"""
    report = report or _report
    if not report:
        return []
    return [c['product'] for c in report['cameras'] if c['state'] == MISSING]


def missing_serials( report = None ):
    report = report or _report
    if not report:
        return []
    return [c['sn'] for c in report['cameras'] if c['state'] == MISSING]


def render_line( report = None ):
    """One-line human summary, e.g. for a Jenkins build description:
    <machine>: D455 ok | D585S MISSING | D555 in-recovery
    (ASCII only: Jenkins agents read the file with their platform charset)
    """
    report = report or _report
    if not report:
        return ''
    prefix = report.get( 'node' )
    if report['status'] == STATUS_NO_MAP:
        return f'{prefix}: no device map'
    if report['status'] == STATUS_TOTAL_ENUMERATION_FAILURE:
        return f'{prefix}: NO DEVICES ENUMERATED (expected {len( report["cameras"] )})'
    parts = []
    for camera in report['cameras']:
        state = camera['state']
        if state == MISSING:
            state = 'MISSING'
        elif state == DEGRADED:
            state = 'DEGRADED(' + camera.get( 'detail', '?' ) + ')'
        parts.append( f'{camera["product"]} {state}' )
    return f'{prefix}: ' + ' | '.join( parts )


def write_health_json( report = None, path = None ):
    """
    Write the report as health.json, plus a pre-rendered health.txt one-liner
    next to it (consumed by the Jenkins build-description step, which has no
    JSON parser guarantees).
    """
    report = report or _report
    if not report or not path:
        return
    os.makedirs( os.path.dirname( path ) or '.', exist_ok = True )
    with open( path, 'w' ) as f:
        json.dump( report, f, indent = 2 )
    txt_path = os.path.splitext( path )[0] + '.txt'
    with open( txt_path, 'w' ) as f:
        f.write( render_line( report ) + '\n' )
    log.d( f'wrote {path}' )
