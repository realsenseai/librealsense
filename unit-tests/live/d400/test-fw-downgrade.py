# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#
# test:device each(D400*)
# test:timeout 300
# test:donotrun:!nightly
#
# Validates firmware downgrade functionality on D400 devices by verifying:
# 1. Firmware can be downgraded using rs-fw-update tool with -u flag
# 2. Firmware version is correctly updated to the target version
#
# Test methodology:
# - Uses rs-fw-update command-line tool with -u flag for unsigned firmware
# - Downgrades firmware to specified older version
# - Validates the firmware version matches expected version after downgrade
#
# Configuration:
# - FW_BINARY_PATH: Path to firmware binary file for downgrade (required)
#   Can be set via environment variable or modified directly in code
# - FW_VERSION: Expected firmware version after downgrade (required)
#   Can be set via environment variable or modified directly in code
#
# Requirements:
# - rs-fw-update tool must be available in PATH or build directory
# - Valid firmware binary file (older version) must be provided
# - Device must support firmware downgrade

import pyrealsense2 as rs
from rspy import test, log
import subprocess
import time
import os
import sys

# Configuration: Path to firmware binary for downgrade
# Override via environment variable: FW_BINARY_PATH=/path/to/firmware.bin python test-fw-downgrade.py
FW_BINARY_PATH = os.environ.get('FW_BINARY_PATH', '/home/tri/Firmware/FlashGeneratedImage_RELEASE_DS5_5_17_0_12.bin')

# Configuration: Expected firmware version after downgrade
# Override via environment variable: FW_VERSION=5.17.0.12 python test-fw-downgrade.py
FW_VERSION = os.environ.get('FW_VERSION', '5.17.0.12')
# Timeout settings
FW_UPDATE_TIMEOUT = 360  # Maximum time to wait for firmware update completion (seconds)
DEVICE_RESET_TIMEOUT = 60  # Maximum time to wait for device to reset and reconnect (seconds)
DEVICE_ENUMERATION_TIMEOUT = 30  # Maximum time to wait for device enumeration (seconds)

# Retry settings
ENUMERATION_RETRY_INTERVAL = 2  # Time between device enumeration attempts (seconds)

def find_rs_fw_update_tool():
    """
    Locate rs-fw-update tool in PATH or build directory.
    
    Returns:
        Path to rs-fw-update executable, or None if not found
    """
    # Check if rs-fw-update is in PATH
    try:
        result = subprocess.run(['which', 'rs-fw-update'], 
                              capture_output=True, text=True, timeout=5)
        if result.returncode == 0:
            tool_path = result.stdout.strip()
            if tool_path:
                log.d(f"Found rs-fw-update in PATH: {tool_path}")
                return tool_path
    except Exception as e:
        log.d(f"Could not find rs-fw-update in PATH: {e}")
    
    # Check build directory relative to test file
    test_dir = os.path.dirname(os.path.abspath(__file__))
    build_dir = os.path.join(test_dir, '../../../build/tools/fw-update')
    fw_update_path = os.path.join(build_dir, 'rs-fw-update')
    
    if os.path.exists(fw_update_path) and os.access(fw_update_path, os.X_OK):
        log.d(f"Found rs-fw-update in build directory: {fw_update_path}")
        return fw_update_path
    
    log.w("rs-fw-update tool not found in PATH or build directory")
    return None

def get_device_serial_number(device):
    """Get device serial number for tracking."""
    try:
        return device.get_info(rs.camera_info.serial_number)
    except Exception as e:
        log.w(f"Could not get device serial number: {e}")
        return None

def wait_for_device_disconnection(serial_number, timeout):
    """
    Wait for device to disconnect (disappear from device list).
    
    Args:
        serial_number: Serial number of device to monitor
        timeout: Maximum time to wait (seconds)
    
    Returns:
        True if device disconnected, False if timeout
    """
    log.d(f"Waiting for device {serial_number} to disconnect...")
    start_time = time.time()
    
    while (time.time() - start_time) < timeout:
        ctx = rs.context()
        devices = ctx.query_devices()
        
        device_found = False
        for dev in devices:
            try:
                sn = dev.get_info(rs.camera_info.serial_number)
                if sn == serial_number:
                    device_found = True
                    break
            except Exception:
                continue
        
        if not device_found:
            log.d(f"Device {serial_number} disconnected after {time.time() - start_time:.1f}s")
            return True
        
        time.sleep(1)
    
    log.w(f"Device {serial_number} did not disconnect within {timeout}s")
    return False

def wait_for_device_reconnection(serial_number, timeout):
    """
    Wait for device to reconnect (appear in device list).
    
    Args:
        serial_number: Serial number of device to monitor
        timeout: Maximum time to wait (seconds)
    
    Returns:
        Device object if reconnected, None if timeout
    """
    log.d(f"Waiting for device {serial_number} to reconnect...")
    start_time = time.time()
    
    while (time.time() - start_time) < timeout:
        ctx = rs.context()
        devices = ctx.query_devices()
        
        for dev in devices:
            try:
                sn = dev.get_info(rs.camera_info.serial_number)
                if sn == serial_number:
                    log.d(f"Device {serial_number} reconnected after {time.time() - start_time:.1f}s")
                    return dev
            except Exception:
                continue
        
        time.sleep(ENUMERATION_RETRY_INTERVAL)
    
    log.w(f"Device {serial_number} did not reconnect within {timeout}s")
    return None

def validate_device_enumeration(device):
    """
    Validate that device is properly enumerated and accessible.
    
    Args:
        device: Device to validate
    
    Returns:
        True if device is properly enumerated, False otherwise
    """
    try:
        # Check basic device info
        name = device.get_info(rs.camera_info.name)
        sn = device.get_info(rs.camera_info.serial_number)
        fw_version = device.get_info(rs.camera_info.firmware_version)
        
        log.d(f"Device enumeration: name={name}, serial={sn}, fw={fw_version}")
        
        # Check if device has sensors
        sensors = device.query_sensors()
        if len(sensors) == 0:
            log.w("Device has no sensors")
            return False
        
        log.d(f"Device has {len(sensors)} sensor(s)")
        
        # Verify depth sensor exists (for D400 devices)
        depth_sensor = None
        for sensor in sensors:
            if sensor.is_depth_sensor():
                depth_sensor = sensor
                break
        
        if not depth_sensor:
            log.w("Device has no depth sensor")
            return False
        
        log.d("Depth sensor found")
        
        # Check if depth sensor has profiles
        profiles = depth_sensor.get_stream_profiles()
        if len(profiles) == 0:
            log.w("Depth sensor has no stream profiles")
            return False
        
        log.d(f"Depth sensor has {len(profiles)} stream profile(s)")
        
        return True
        
    except Exception as e:
        log.w(f"Device enumeration validation failed: {e}")
        return False

################################################################################################

# Check if firmware binary path is provided
if not FW_BINARY_PATH:
    log.e("Firmware binary path not provided. Set FW_BINARY_PATH environment variable.")
    log.e("Example: FW_BINARY_PATH=/path/to/firmware.bin python test-fw-downgrade.py")
    test.print_results_and_exit()

if not os.path.exists(FW_BINARY_PATH):
    log.e(f"Firmware binary not found: {FW_BINARY_PATH}")
    test.print_results_and_exit()

log.i(f"Using firmware binary: {FW_BINARY_PATH}")

# Find rs-fw-update tool
rs_fw_update_tool = find_rs_fw_update_tool()
if not rs_fw_update_tool:
    log.e("rs-fw-update tool not found. Please ensure it is built and in PATH.")
    test.print_results_and_exit()

log.i(f"Using rs-fw-update tool: {rs_fw_update_tool}")

# Get initial device
device, _ = test.find_first_device_or_exit()
device_serial = get_device_serial_number(device)

if not device_serial:
    log.e("Could not get device serial number")
    test.print_results_and_exit()

log.i(f"Found device: serial={device_serial}")

# Get initial firmware version
initial_fw_version = None
try:
    initial_fw_version = device.get_info(rs.camera_info.firmware_version)
    log.i(f"Current firmware version: {initial_fw_version}")
except Exception as e:
    log.w(f"Could not get initial firmware version: {e}")

################################################################################################

test.start("Perform firmware downgrade using rs-fw-update")
# Test 1: Execute firmware downgrade using rs-fw-update tool
#
# This test validates that:
# - rs-fw-update tool can be executed successfully with -u flag for unsigned firmware
# - Firmware downgrade process completes without errors
# - Tool returns success exit code

log.i("Starting firmware downgrade...")
log.i(f"Command: {rs_fw_update_tool} -u -f {FW_BINARY_PATH}")

try:
    # Execute rs-fw-update command
    # Note: -u flag allows unsigned firmware
    result = subprocess.run(
        [rs_fw_update_tool, '-u', '-f', FW_BINARY_PATH],
        capture_output=True,
        text=True,
        timeout=FW_UPDATE_TIMEOUT
    )
    
    # Log output - use info level for visibility
    if result.stdout:
        log.i("rs-fw-update stdout:")
        for line in result.stdout.strip().split('\n'):
            if line.strip():  # Skip empty lines
                log.i(f"  {line}")
    
    if result.stderr:
        log.i("rs-fw-update stderr:")
        for line in result.stderr.strip().split('\n'):
            if line.strip():  # Skip empty lines
                log.i(f"  {line}")
    
    # Check exit code
    if result.returncode != 0:
        log.e(f"Firmware update failed with exit code {result.returncode}")
        log.e("Check the output above for error details")
    
    test.check(result.returncode == 0, 
              f"rs-fw-update should complete successfully (exit code 0), got {result.returncode}")
    
    if result.returncode == 0:
        log.i("Firmware update completed successfully")
        
except subprocess.TimeoutExpired:
    log.e(f"Firmware update timed out after {FW_UPDATE_TIMEOUT}s")
    test.check(False, "Firmware update should complete within timeout")
except Exception as e:
    log.e(f"Firmware update failed with exception: {e}")
    test.check(False, f"Firmware update should not raise exception: {e}")

test.finish()

################################################################################################

test.start("Verify firmware version after downgrade")
# Test 2: Verify the device has the expected firmware version after downgrade
#
# This test validates that:
# - Device can be queried after firmware downgrade
# - Firmware version matches the expected (older) version
# - Device properly reports its firmware information after downgrade

log.i("Waiting for device to be ready after firmware downgrade...")
time.sleep(5)  # Give device time to stabilize after firmware downgrade

# Query for devices
try:
    ctx = rs.context()
    devices = ctx.query_devices()
    
    if len(devices) == 0:
        log.e("No devices found after firmware downgrade")
        test.check(False, "Device should be available after firmware downgrade")
    else:
        device = devices[0]
        
        try:
            actual_fw_version = device.get_info(rs.camera_info.firmware_version)
            device_name = device.get_info(rs.camera_info.name)
            device_serial = device.get_info(rs.camera_info.serial_number)
            
            log.i(f"Device: {device_name}")
            log.i(f"Serial Number: {device_serial}")
            log.i(f"Firmware Version: {actual_fw_version}")
            log.i(f"Expected Version: {FW_VERSION}")
            
            # Check if firmware version matches expected
            version_matches = (actual_fw_version == FW_VERSION)
            test.check(version_matches, 
                      f"Firmware version should be {FW_VERSION}, got {actual_fw_version}")
            
            if version_matches:
                log.i("Firmware version verification passed")
            else:
                log.w(f"Firmware version mismatch: expected {FW_VERSION}, got {actual_fw_version}")
                
        except Exception as e:
            log.e(f"Could not query device firmware version: {e}")
            test.check(False, f"Should be able to query device firmware version: {e}")
except Exception as e:
    log.e(f"Could not query devices: {e}")
    test.check(False, f"Should be able to query devices after firmware downgrade: {e}")

test.finish()

################################################################################################
test.print_results_and_exit()
