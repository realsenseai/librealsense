# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#
# test:device each(D400*)
# test:timeout 300
# test:donotrun:!nightly
#
# Validates firmware downgrade functionality on D400 devices by verifying:
# 1. Firmware can be downgraded using rs-fw-update tool
# 2. Firmware version is correctly updated to the target version
# 3. Firmware can be restored to the recommended version
#
# Test methodology:
# - Fetches the latest released FW version from the firmware releases page
# - Downloads the FW binary from Artifactory
# - Downgrades firmware to the latest released version
# - Validates the firmware version matches expected version after downgrade
# - Restores firmware to the original camera version (downloaded from Artifactory)
#
# Configuration:
# - Version discovery: https://dev.realsenseai.com/docs/firmware-releases-d400
# - Binary download: Artifactory (rsartifactory.realsenseai.com)
# - Restore version: device's original firmware_version before downgrade (downloaded from Artifactory)
#
# Requirements:
# - rs-fw-update tool must be available in PATH or build directory
# - Network access to firmware releases page and Artifactory for firmware download
#
# Skip conditions:
# - Test is skipped if device firmware version <= latest released version
# - Test aborts if downgrade firmware fails check_firmware_compatibility()
#   (uses d400_device_to_fw_min_version from src/ds/d400/d400-private.h internally)

import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy import test, log, file, repo
import subprocess
import time
import os
import re
import tempfile
import platform
import urllib.request
import zipfile
from urllib.error import URLError, HTTPError

# Firmware configuration
FW_RELEASES_URL = "https://dev.realsenseai.com/docs/firmware-releases-d400"
# Artifactory path to D400 firmware builds (ED/NIGHTLY hosts both nightly and release builds as RELEASE_DS5_*.zip)
ARTIFACTORY_FW_BASE_URL = "https://rsartifactory.realsenseai.com/artifactory/realsense_generic_dev-il-local/FW/RS400/ED/NIGHTLY"

def check_fw_compatibility(device, fw_image_path):
    """
    Check if a firmware image is compatible with the device using the SDK's
    built-in check_firmware_compatibility() API.  Internally this uses
    d400_device_to_fw_min_version from src/ds/d400/d400-private.h, so it
    stays in sync with the SDK without hardcoding version tables.

    Args:
        device: pyrealsense2 device object
        fw_image_path: path to the firmware .bin file

    Returns:
        True if the firmware image is compatible, False otherwise.
        Returns None if the check could not be performed.
    """
    try:
        updatable = device.as_updatable()
    except Exception as e:
        log.w(f"Device cannot be cast to updatable — skipping compatibility check: {e}")
        return None
    try:
        with open(fw_image_path, 'rb') as f:
            fw_image = bytearray(f.read())
        compatible = updatable.check_firmware_compatibility(fw_image)
        return compatible
    except Exception as e:
        # check_firmware_compatibility() only supports signed images;
        # unsigned FlashGeneratedImage binaries are expected to fail here
        log.d(f"check_firmware_compatibility() not available for this image: {e}")
        return None


def get_latest_fw_version_from_releases():
    """
    Fetch the latest released D400 firmware version from the firmware releases page.
    Parses the HTML table to find the first (newest) FW version entry.
    
    Returns:
        Version string (e.g., "5.17.0.10") or None if parsing fails
    """
    try:
        log.i(f"Fetching firmware releases from {FW_RELEASES_URL}...")
        req = urllib.request.Request(FW_RELEASES_URL)
        req.add_header('User-Agent', 'Mozilla/5.0')  # some servers require a user-agent
        with urllib.request.urlopen(req, timeout=30) as response:
            html = response.read().decode('utf-8')
        
        # The table has FW version in a column with format like "5.17.0.10"
        # We look for version patterns in table rows — the first match is the newest
        # Table format: | Version-tag | release name | date | FW_VERSION | SDK | devices | notes |
        # We need the FW version column (4th column), which has dotted format
        # Match versions that appear after a date pattern (Month Year) in table context
        version_pattern = re.compile(r'\|\s*(?:\d+\.\d+\.\d+\.\d+)\s*\|.*?\|.*?\|\s*(\d+\.\d+\.\d+\.\d+)\s*\|')
        match = version_pattern.search(html)
        if match:
            version = match.group(1)
            log.i(f"Latest released FW version: {version}")
            return version
        
        # Fallback: find all 4-part version strings and take the highest
        all_versions = re.findall(r'(?<!Version-)(?<!-)\b(\d+\.\d+\.\d+\.\d+)\b', html)
        if all_versions:
            # Parse and sort versions to find the highest
            def version_tuple(v):
                return tuple(int(x) for x in v.split('.'))
            all_versions = list(set(all_versions))  # deduplicate
            all_versions.sort(key=version_tuple, reverse=True)
            version = all_versions[0]
            log.i(f"Latest released FW version (fallback parse): {version}")
            return version
        
        log.e("Could not parse any firmware version from releases page")
        return None
        
    except (URLError, HTTPError) as e:
        log.e(f"Failed to fetch firmware releases page: {e}")
        return None
    except Exception as e:
        log.e(f"Failed to parse firmware releases page: {e}")
        return None


def get_fw_download_url(version):
    """
    Build Artifactory download URL for a firmware image.
    
    Args:
        version: Firmware version string (e.g., "5.17.0.10")
    
    Returns:
        Artifactory URL for the firmware ZIP archive
    """
    version_underscored = version.replace('.', '_')
    return f"{ARTIFACTORY_FW_BASE_URL}/RELEASE_DS5_{version_underscored}.zip"


def download_firmware_file(url, dest_path):
    """
    Download firmware file from URL to destination path.
    
    Args:
        url: URL to download from
        dest_path: Local path to save the file
    
    Returns:
        True if download successful, False otherwise
    """
    try:
        dest_dir = os.path.dirname(dest_path)
        if dest_dir and not os.path.exists(dest_dir):
            os.makedirs(dest_dir, exist_ok=True)
        
        log.i(f"Downloading firmware from {url}...")
        import shutil
        req = urllib.request.Request(url)
        with urllib.request.urlopen(req, timeout=120) as response, open(dest_path, 'wb') as out_file:
            shutil.copyfileobj(response, out_file)
        log.i(f"Downloaded firmware to {dest_path}")
        return True
    except (URLError, HTTPError) as e:
        log.e(f"Failed to download firmware: {e}")
        return False
    except Exception as e:
        log.e(f"Failed to download firmware: {e}")
        return False


def download_and_extract_fw(version):
    """
    Download firmware ZIP from Artifactory and extract the unsigned .bin file.
    Uses cached .bin if already extracted from a previous run.
    
    Args:
        version: Firmware version string (e.g., "5.17.0.10")
    
    Returns:
        Path to the extracted .bin file, or None if download/extraction fails
    """
    version_underscored = version.replace('.', '_')
    fw_bin = f"FlashGeneratedImage_RELEASE_DS5_{version_underscored}.bin"
    fw_path = os.path.join(tempfile.gettempdir(), fw_bin)
    if os.path.exists(fw_path) and os.path.getsize(fw_path) > 0:
        log.i(f"Using cached firmware: {fw_path} ({os.path.getsize(fw_path)} bytes)")
        return fw_path
    elif os.path.exists(fw_path):
        log.w(f"Removing invalid cached firmware (0 bytes): {fw_path}")
        os.remove(fw_path)
    
    fw_url = get_fw_download_url(version)
    fw_zip = os.path.join(tempfile.gettempdir(), f"RELEASE_DS5_{version_underscored}.zip")
    if not download_firmware_file(fw_url, fw_zip):
        log.e(f"Failed to download firmware from {fw_url}")
        return None
    
    log.i(f"Extracting firmware from {fw_zip}...")
    try:
        with zipfile.ZipFile(fw_zip, 'r') as zf:
            log.d(f"ZIP contents: {zf.namelist()}")
            expected_bin = f"RELEASE_DS5_{version_underscored}/{fw_bin}"
            if expected_bin in zf.namelist():
                bin_name = expected_bin
            else:
                # Fallback: look for any FlashGeneratedImage .bin file
                flash_bins = [n for n in zf.namelist()
                             if 'FlashGeneratedImage' in n and n.lower().endswith('.bin')]
                if flash_bins:
                    bin_name = flash_bins[0]
                    log.w(f"Expected {expected_bin} not found, using {bin_name}")
                else:
                    log.e(f"No FlashGeneratedImage .bin file found inside {fw_zip}")
                    log.e(f"Available files: {zf.namelist()}")
                    return None
            log.i(f"Extracting {bin_name} from ZIP...")
            with zf.open(bin_name) as src, open(fw_path, 'wb') as dst:
                dst.write(src.read())
        log.i(f"Extracted firmware to {fw_path}")
        os.remove(fw_zip)
        return fw_path
    except zipfile.BadZipFile:
        log.e(f"Downloaded file is not a valid ZIP archive: {fw_zip}")
        return None
    except Exception as e:
        log.e(f"Failed to extract firmware from ZIP: {e}")
        return None

# Timeout settings
FW_UPDATE_TIMEOUT = 280  # Maximum time to wait for firmware update completion (seconds)
DEVICE_STABILIZE_TIME = 5  # Time to wait after firmware update for device to stabilize (seconds)
POST_RESTORE_STABILIZE_TIME = 15  # Extra time to allow device to settle after restore (seconds)

def find_rs_fw_update_tool():
    """
    Locate rs-fw-update tool in build directory or PATH.
    
    Returns:
        Path to rs-fw-update executable, or None if not found
    """
    # Search in the repo build directory (same as test-fw-update.py)
    fw_updater_exe_regex = r'(^|/)rs-fw-update'
    if platform.system() == 'Windows':
        fw_updater_exe_regex += r'\.exe'
    fw_updater_exe_regex += '$'
    for tool in file.find( repo.build, fw_updater_exe_regex ):
        tool_path = os.path.join( repo.build, tool )
        log.d(f"Found rs-fw-update in build directory: {tool_path}")
        return tool_path
    
    # Fallback: check if rs-fw-update is in PATH
    try:
        which_cmd = 'where' if platform.system() == 'Windows' else 'which'
        result = subprocess.run([which_cmd, 'rs-fw-update'], 
                              capture_output=True, text=True, timeout=5)
        if result.returncode == 0:
            tool_path = result.stdout.strip().split('\n')[0]
            if tool_path:
                log.d(f"Found rs-fw-update in PATH: {tool_path}")
                return tool_path
    except Exception as e:
        log.d(f"Could not find rs-fw-update in PATH: {e}")
    
    log.w("rs-fw-update tool not found")
    return None

def get_device_serial_number(device):
    """Get device serial number for tracking."""
    try:
        return device.get_info(rs.camera_info.serial_number)
    except Exception as e:
        log.w(f"Could not get device serial number: {e}")
        return None




################################################################################################

# Get initial device
device, _ = test.find_first_device_or_exit()
product_line = device.get_info(rs.camera_info.product_line)
product_name = device.get_info(rs.camera_info.name)

device_serial = get_device_serial_number(device)
if not device_serial:
    log.f("Could not get device serial number")

log.i(f"Found device: {product_name}, serial={device_serial}")

# Get current firmware version
current_fw_version = device.get_info(rs.camera_info.firmware_version)
current_version = rsutils.version(current_fw_version)
log.i(f"Current firmware version: {current_fw_version}")

# Discover the latest released FW version from the firmware releases page
downgrade_fw_version = get_latest_fw_version_from_releases()
if not downgrade_fw_version:
    log.f("Could not determine latest released firmware version from releases page")

downgrade_version = rsutils.version(downgrade_fw_version)
log.i(f"Latest released firmware version (downgrade target): {downgrade_fw_version}")

# Skip if device FW is not newer than the latest released version
if current_version <= downgrade_version:
    log.i(f"Device firmware {current_fw_version} <= latest released {downgrade_fw_version} - skipping test")
    test.print_results_and_exit()

log.i(f"Device firmware {current_fw_version} > {downgrade_fw_version} - proceeding with downgrade test")

# Save the original camera FW version for restore after downgrade
restore_image_file = None
restore_fw_version = current_fw_version
restore_version = current_version
log.i(f"Restore target (original camera version): {restore_fw_version}")

# Find rs-fw-update tool
rs_fw_update_tool = find_rs_fw_update_tool()
if not rs_fw_update_tool:
    log.f("rs-fw-update tool not found. Please ensure it is built and in PATH.")

log.i(f"Using rs-fw-update tool: {rs_fw_update_tool}")

# Download downgrade firmware from Artifactory
downgrade_fw_path = download_and_extract_fw(downgrade_fw_version)
if not downgrade_fw_path:
    log.f(f"Failed to download/extract downgrade firmware {downgrade_fw_version} from Artifactory")

# Download original camera firmware from Artifactory for restore after downgrade
restore_image_file = download_and_extract_fw(restore_fw_version)
if not restore_image_file:
    log.f(f"Failed to download original camera firmware {restore_fw_version} from Artifactory — cannot guarantee restore")

# Safety check: use the SDK's check_firmware_compatibility() to verify the downgrade
# image is above the device's minimum supported FW (d400_device_to_fw_min_version in d400-pr
compat = check_fw_compatibility(device, downgrade_fw_path)
if compat is False:
    log.f(f"Downgrade firmware {downgrade_fw_version} is not compatible with {product_name} "
          f"— aborting to avoid bricking the device")
elif compat is True:
    log.i(f"Firmware compatibility check passed for {downgrade_fw_version}")
else:
    log.w(f"Could not verify firmware compatibility — proceeding with caution")

################################################################################################

test.start("Perform firmware downgrade using rs-fw-update")
# Test 1: Execute firmware downgrade using rs-fw-update tool
#
# This test validates that:
# - rs-fw-update tool can be executed successfully
# - Firmware downgrade process completes without errors
# - Tool returns success exit code

log.i("Starting firmware downgrade...")
log.i(f"Command: {rs_fw_update_tool} -s {device_serial} -u -f {downgrade_fw_path}")

# Release device and context before firmware update (same as test-fw-update.py)
del device

try:
    result = subprocess.run(
        [rs_fw_update_tool, '-s', device_serial, '-u', '-f', downgrade_fw_path],
        capture_output=True,
        text=True,
        timeout=FW_UPDATE_TIMEOUT
    )
    
    if result.stdout:
        log.i("rs-fw-update stdout:")
        for line in result.stdout.strip().split('\n'):
            if line.strip():
                log.i(f"  {line}")
    
    if result.stderr:
        log.i("rs-fw-update stderr:")
        for line in result.stderr.strip().split('\n'):
            if line.strip():
                log.i(f"  {line}")
    
    if result.returncode != 0:
        log.e(f"Firmware downgrade failed with exit code {result.returncode}")
    
    test.check(result.returncode == 0,
              f"rs-fw-update should complete successfully (exit code 0), got {result.returncode}")
    
    if result.returncode == 0:
        log.i("Firmware downgrade completed successfully")
        
except subprocess.TimeoutExpired:
    log.e(f"Firmware downgrade timed out after {FW_UPDATE_TIMEOUT}s")
    test.check(False, "Firmware downgrade should complete within timeout")
except Exception as e:
    log.e(f"Firmware downgrade failed with exception: {e}")
    test.check(False, f"Firmware downgrade should not raise exception: {e}")

test.finish()

################################################################################################

test.start("Verify firmware version after downgrade")
# Test 2: Verify the device has the expected firmware version after downgrade
#
# This test validates that:
# - Device can be queried after firmware downgrade
# - Firmware version matches the expected (older) version

log.i("Waiting for device to be ready after firmware downgrade...")
time.sleep(DEVICE_STABILIZE_TIME)

try:
    ctx = rs.context()
    devices = ctx.query_devices()
    
    # Find our device by serial number (in case multiple devices are connected)
    device = None
    for d in devices:
        if d.get_info(rs.camera_info.serial_number) == device_serial:
            device = d
            break

    if not device:
        log.e(f"Device with serial {device_serial} not found after firmware downgrade")
        test.check(False, f"Device {device_serial} should be available after firmware downgrade")
    else:
        actual_fw_version = device.get_info(rs.camera_info.firmware_version)
        device_name = device.get_info(rs.camera_info.name)
        
        log.i(f"Device: {device_name}, serial={device_serial}")
        log.i(f"Firmware Version: {actual_fw_version}")
        log.i(f"Expected Version: {downgrade_fw_version}")
        
        version_matches = (rsutils.version(actual_fw_version) == downgrade_version)
        test.check(version_matches,
                  f"Firmware version should be {downgrade_fw_version}, got {actual_fw_version}")
        
        if version_matches:
            log.i("Firmware version verification passed")
        else:
            log.w(f"Firmware version mismatch: expected {downgrade_fw_version}, got {actual_fw_version}")

except Exception as e:
    log.e(f"Could not verify firmware version: {e}")
    test.check(False, f"Should be able to verify device firmware version: {e}")

test.finish()

################################################################################################

test.start("Restore firmware to original camera version")
# Test 3: Restore firmware to the original camera version
#
# This test validates that:
# - Firmware can be restored to the version the camera had before the test
# - Device returns to the original firmware version

log.i(f"Restoring firmware to original camera version: {restore_fw_version}")
log.i(f"Using image: {restore_image_file}")

try:
    result = subprocess.run(
        [rs_fw_update_tool, '-s', device_serial, '-u', '-f', restore_image_file],
        capture_output=True,
        text=True,
        timeout=FW_UPDATE_TIMEOUT
    )
    
    if result.stdout:
        log.i("rs-fw-update stdout:")
        for line in result.stdout.strip().split('\n'):
            if line.strip():
                log.i(f"  {line}")
    
    if result.stderr:
        log.i("rs-fw-update stderr:")
        for line in result.stderr.strip().split('\n'):
            if line.strip():
                log.i(f"  {line}")
    
    test.check(result.returncode == 0,
              f"Firmware restore should complete successfully (exit code 0), got {result.returncode}")
    
    if result.returncode == 0:
        log.i("Firmware restore completed successfully")
        
        # Verify restored version
        time.sleep(DEVICE_STABILIZE_TIME)
        ctx = rs.context()
        devices = ctx.query_devices()
        
        # Find our device by serial number
        restored_device = None
        for d in devices:
            if d.get_info(rs.camera_info.serial_number) == device_serial:
                restored_device = d
                break

        if restored_device:
            restored_fw_version = restored_device.get_info(rs.camera_info.firmware_version)
            log.i(f"Restored firmware version: {restored_fw_version}")
            test.check(rsutils.version(restored_fw_version) == restore_version,
                      f"Firmware should be restored to {restore_fw_version}, got {restored_fw_version}")
        else:
            test.check(False, f"Device {device_serial} should be available after firmware restore")

        log.i(f"Waiting {POST_RESTORE_STABILIZE_TIME}s for device to stabilize after restore...")
        time.sleep(POST_RESTORE_STABILIZE_TIME)

        stable_ctx = rs.context()
        stable_device = None
        for d in stable_ctx.query_devices():
            if d.get_info(rs.camera_info.serial_number) == device_serial:
                stable_device = d
                break

        if stable_device:
            stable_fw_version = stable_device.get_info(rs.camera_info.firmware_version)
            log.i(f"Device stable after restore; FW={stable_fw_version}")
        else:
            log.w(f"Device {device_serial} not found after stabilization wait")

        # Release all device/context references so the next test starts clean
        del stable_device, stable_ctx
        del restored_device, devices, ctx

except subprocess.TimeoutExpired:
    log.e(f"Firmware restore timed out after {FW_UPDATE_TIMEOUT}s")
    test.check(False, "Firmware restore should complete within timeout")
except Exception as e:
    log.e(f"Firmware restore failed with exception: {e}")
    test.check(False, f"Firmware restore should not raise exception: {e}")

test.finish()

################################################################################################
test.print_results_and_exit()
