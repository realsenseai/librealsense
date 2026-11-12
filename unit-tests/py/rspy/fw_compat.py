# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
FW-update pre-flash compatibility gate.

For a given device, picks a candidate FW version (a user-supplied custom-fw image
or the device's RECOMMENDED_FIRMWARE_VERSION) and compares it against the device's
minimum supported FW (rs2::device::get_firmware_min_version). When the candidate is
below the min, the device's golden FW is substituted instead of skipping or failing the test.

Golden FW URLs come from the caller (pytest-fw-update's --gold-fw-<sku> options, which LibCI fills
from fw-versions.properties in the deploy repo). Both run-unit-tests.py and pytest-based test
runners should import these helpers rather than re-implementing the logic.
"""

import os
import re
import subprocess
import urllib.request

from rspy import log, libci


def download_gold_fw( url ):
    """Return a local path to the golden FW image at `url` (or `url` itself if it is a local file).

    Used to recover a device in DFU and as the below-min-FW fallback. The image is cached under
    libci.home (persistent across reboots) and only re-downloaded if missing on disk. Returns the
    local path, or None on failure (never raises).
    """
    if not url:
        return None
    if os.path.isfile( url ):
        return url
    cache_dir = os.path.join( libci.home, 'data', 'FW', 'golden' )
    dest = os.path.join( cache_dir, os.path.basename( url ) )
    if os.path.isfile( dest ):
        log.d( f"golden FW already cached: {dest}" )
        return dest
    try:
        os.makedirs( cache_dir, exist_ok=True )
    except OSError as e:
        log.w( f"could not create cache directory {cache_dir}: {e}" )
        return None
    log.d( f"downloading golden FW from {url}" )
    try:
        with urllib.request.urlopen( url, timeout=120 ) as response:  # don't hang CI forever
            content = response.read()
    except Exception as e:
        log.w( f"failed to download golden FW: {e}" )
        return None
    # Sanity-check before caching: a real FW image is well over 512 KB, so a tiny payload
    # (an HTML error page, a redirect body, or a truncated transfer) means a bad download that
    # we must not cache and hand to rs-fw-update.
    if len( content ) < 512 * 1024:
        log.w( f"downloaded golden FW is suspiciously small ({len( content )} bytes); discarding" )
        return None
    try:
        with open( dest, 'wb' ) as out_file:
            out_file.write( content )
    except OSError as e:
        log.w( f"could not write golden FW to {dest}: {e}" )
        return None
    log.d( f"saved golden FW to: {dest}" )
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


def resolve_fw_gate( rspy_device, test_name, sn=None,
                     custom_fw_d400_path=None, custom_fw_d555_path=None, gold_fw_url=None ):
    """
    Combined fw-compat check and fallback resolution for test-fw-update.

    Silent when the device's FW situation is compatible -- only logs when
    there is something noteworthy (below min FW, or the gate can't decide).

    Returns (skip: bool, fw_override: str|None):
      - (False, None)   -- compatible / can't decide; run the test as-is
      - (False, <path>) -- below min FW; flash this golden image instead
      - (True,  None)   -- below min FW with NO golden available; do not run the test.
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
    fallback = download_gold_fw( gold_fw_url )
    if fallback:
        if custom_fw_d400_path:
            log.i( f'{test_name}: {label}: {reason}; --custom-fw-d400 is below min too, overriding with fallback {fallback}' )
        else:
            log.i( f'{test_name}: {label}: {reason}; using fallback {fallback}' )
        return False, fallback
    log.e( f'{test_name}: {label}: {reason}; no golden FW available (--gold-fw-*) -- refusing to flash a below-min FW' )
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
