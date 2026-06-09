# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
FW-update pre-flash compatibility gate.

For a given device, picks a candidate FW version (a user-supplied custom-fw image
or the device's RECOMMENDED_FIRMWARE_VERSION) and compares it against the device's
minimum supported FW (rs2::device::get_firmware_min_version). When the candidate is
below the min, a per-device fallback FW image can be substituted instead of skipping
or failing the test.

The fallback mapping lives in fw_fallback.json next to this file so it can be
maintained without touching code. Both run-unit-tests.py and pytest-based test
runners should import these helpers rather than re-implementing the logic.
"""

import json
import os
import re
import subprocess
import urllib.request

from rspy import log, libci


_FALLBACK_JSON = os.path.join( os.path.dirname( __file__ ), 'fw_fallback.json' )


def _load_gold_recovery_fw_map():
    """Read fw_fallback.json -> dict[product_line -> URL]. Returns {} on any error.
    Gold recovery images live in a `gold_recovery_fw` section alongside `fallbacks`."""
    try:
        with open( _FALLBACK_JSON, 'r' ) as f:
            data = json.load( f )
    except (FileNotFoundError, ValueError, OSError) as e:
        log.w( f'[fw-gate] could not load {_FALLBACK_JSON}: {e}' )
        return {}
    return data.get( 'gold_recovery_fw', {} )


def download_gold_d400_fw():
    """Return a local path to the D400 gold signed FW image.

    Used as a recovery image for any D400 device in DFU mode; rs-fw-update's
    recovery (-r) path only accepts signed FW, so we can't reuse the unsigned
    --custom-fw-d400 path. The URL is configured in fw_fallback.json under
    `gold_recovery_fw.D400` so it can be updated without touching code.

    The image is cached under libci.home (persistent across reboots) and only
    re-downloaded if missing on disk. Returns the local path, or None on failure.
    """
    url = _load_gold_recovery_fw_map().get( 'D400' )
    if not url:
        log.w( "fw_fallback.json has no gold_recovery_fw.D400 entry" )
        return None
    # Co-locate with the D400 fallback signed images, under libci.home.
    cache_dir = os.path.join( libci.home, 'data', 'FW', 'D400' )
    dest = os.path.join( cache_dir, os.path.basename( url ) )
    if os.path.isfile( dest ):
        log.d( f"gold D400 FW already cached: {dest}" )
        return dest
    try:
        os.makedirs( cache_dir, exist_ok=True )
    except OSError as e:
        log.w( f"could not create cache directory {cache_dir}: {e}" )
        return None
    log.d( f"downloading gold D400 FW from {url}" )
    try:
        with urllib.request.urlopen( url ) as response, open( dest, 'wb' ) as out_file:
            out_file.write( response.read() )
    except Exception as e:
        log.w( f"failed to download gold D400 FW from S3: {e}" )
        return None
    log.d( f"saved gold D400 FW to: {dest}" )
    return dest


def reload_d4xx_driver_on_jetson(context):
    """On Jetson, the d4xx MIPI driver must be reloaded after a recovery flash so the
    re-enumerated device shows up. No-op (with a warning) if sudo requires a password
    or if we're not running under the 'jetson' context."""
    if 'jetson' not in (context or []):
        return
    log.d("Reloading d4xx driver on Jetson...")
    try:
        rm = subprocess.run(['sudo', '-n', 'modprobe', '-r', 'd4xx'], capture_output=True, text=True)
        if rm.returncode != 0:
            log.e("Failed to remove d4xx module (may require passwordless sudo):", rm.stderr)
            return
        ld = subprocess.run(['sudo', '-n', 'modprobe', 'd4xx'], capture_output=True, text=True, check=False)
        if ld.returncode != 0:
            log.e("Failed to load d4xx module (may require passwordless sudo):",
                  f"returncode={ld.returncode}, stderr={ld.stderr}")
    except Exception as e:
        log.w("Could not reload d4xx driver (passwordless sudo may not be configured):", e)


def _load_fallback_map():
    """Read fw_fallback.json -> dict[device_name -> relpath]. Returns {} on any error."""
    try:
        with open( _FALLBACK_JSON, 'r' ) as f:
            data = json.load( f )
    except (FileNotFoundError, ValueError, OSError) as e:
        log.w( f'[fw-gate] could not load {_FALLBACK_JSON}: {e}' )
        return {}
    return data.get( 'fallbacks', {} )


def fw_fallback_image_for( rspy_device, libci_home ):
    """
    Return an absolute path to the fallback FW image for `rspy_device`, or None
    if there's no mapping or the file is missing on disk. `libci_home` is the
    root under which the relative paths in fw_fallback.json resolve.
    """
    fallbacks = _load_fallback_map()
    relpath = fallbacks.get( rspy_device.name )
    if not relpath:
        return None
    path = os.path.join( libci_home, relpath )
    if not os.path.isfile( path ):
        log.w( f'[fw-gate] fallback FW for {rspy_device.name} not found on disk: {path}' )
        return None
    return path


def version_to_tuple( s ):
    parts = re.findall( r'\d+', s or '' )
    return tuple( int( p ) for p in parts[:4] ) + (0,) * max( 0, 4 - len( parts ) )


def version_from_fw_filename( path ):
    """Mirror of test-fw-update.extract_version_from_filename; returns 'a.b.c.d' or None."""
    name = os.path.basename( path or '' )
    m = re.search( r'(\d+)_(\d+)_(\d+)_(\d+)\.(?:bin|img)$', name, re.IGNORECASE )
    if not m:
        m = re.search( r'-(\d+)\.(\d+)\.(\d+)\.(\d+)\.(?:bin|img)$', name, re.IGNORECASE )
    if not m:
        return None
    return '.'.join( m.group( i ) for i in range( 1, 5 ) )


def resolve_fw_gate( rspy_device, libci_home, test_name, sn=None,
                     custom_fw_d400_path=None, custom_fw_d555_path=None ):
    """
    Combined fw-compat check and fallback resolution for test-fw-update.

    Silent when the device's FW situation is compatible -- only logs when
    there is something noteworthy (below min FW, or the gate can't decide).

    Returns (skip: bool, fw_override: str|None):
      - (False, None)   -- compatible / can't decide; run the test as-is
      - (False, <path>) -- below min FW with a fallback available; flash this image instead
      - (True,  None)   -- below min FW with NO fallback registered; do not run the test.
                          rs-fw-update -u on an unlocked camera bypasses the C++ min-FW
                          check, so letting the test run here would silently flash a
                          below-min image. Refuse instead.
    """
    label = f'{rspy_device.name}_{sn}' if sn else rspy_device.name
    status, reason = evaluate_fw_compat( rspy_device,
                                         custom_fw_d400_path=custom_fw_d400_path,
                                         custom_fw_d555_path=custom_fw_d555_path )
    if status == 'compatible':
        return False, None
    if status == 'unknown':
        log.d( f'[fw-gate] {label}: cannot decide -- {reason}; deferring to test' )
        return False, None
    # status == 'below_min'
    fallback = fw_fallback_image_for( rspy_device, libci_home )
    if fallback:
        if custom_fw_d400_path:
            log.i( f'{test_name}: {label}: {reason}; --custom-fw-d400 is below min too, overriding with fallback {fallback}' )
        else:
            log.i( f'{test_name}: {label}: {reason}; using fallback {fallback}' )
        return False, fallback
    log.e( f'{test_name}: {label}: {reason}; no fallback registered in rspy/fw_fallback.json -- refusing to flash a below-min FW' )
    return True, None


def evaluate_fw_compat( rspy_device, custom_fw_d400_path=None, custom_fw_d555_path=None ):
    """
    Decide whether test-fw-update's candidate FW image is compatible with
    `rspy_device`. Returns (status, reason) where status is one of:

      'compatible' -- candidate FW >= device minimum supported FW; run the test as-is.
      'below_min'  -- candidate FW < device minimum; caller should look up a fallback
                      image and override --custom-fw-d400.
      'unknown'    -- the gate couldn't determine compatibility (no candidate FW found,
                      no minimum FW reported by the device, or the API raised). Caller
                      should run the test anyway and let it decide.

    `rspy_device` is the rspy.devices.Device wrapper; the underlying pyrealsense2
    device is available at `.handle`.
    """
    try:
        import pyrealsense2 as rs
    except ImportError as e:
        return 'unknown', f'pyrealsense2 unavailable ({e})'

    handle = rspy_device.handle
    product_line = rspy_device.product_line
    product_name = rspy_device.name  # wrapper strips "Intel RealSense " / "RealSense " prefix

    candidate = None
    source = ''
    if product_line == 'D400' and custom_fw_d400_path:
        candidate = version_from_fw_filename( custom_fw_d400_path )
        source = f'--custom-fw-d400 {os.path.basename( custom_fw_d400_path )}'
    elif 'D555' in (product_name or '') and custom_fw_d555_path:
        candidate = version_from_fw_filename( custom_fw_d555_path )
        source = f'--custom-fw-d555 {os.path.basename( custom_fw_d555_path )}'
    elif handle.supports( rs.camera_info.recommended_firmware_version ):
        candidate = handle.get_info( rs.camera_info.recommended_firmware_version )
        source = 'RECOMMENDED_FIRMWARE_VERSION'
    if not candidate:
        return 'unknown', 'no candidate FW version available'

    try:
        min_fw = handle.get_firmware_min_version()
    except Exception as e:
        return 'unknown', f'get_firmware_min_version() raised: {e}'

    if not min_fw:
        return 'unknown', f'device reports no minimum FW (candidate {candidate})'

    if version_to_tuple( candidate ) >= version_to_tuple( min_fw ):
        return 'compatible', f'candidate {candidate} >= min {min_fw} (from {source})'
    return 'below_min', f'candidate {candidate} < min {min_fw} (from {source})'
