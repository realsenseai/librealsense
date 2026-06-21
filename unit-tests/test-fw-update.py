# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

#We want this test to run right after camera detection phase, so that all tests will run with updated FW versions, so we give it high priority
#test:priority 1
#test:timeout 500
#test:donotrun:gha
#test:device each(D400*)
#test:device each(D555)

import sys
import os
import subprocess
import re
import platform
import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy import log, test, file, repo, fw_compat
from rspy.timer import Timer
import time
import argparse

# Parse command-line arguments
parser = argparse.ArgumentParser(description="Test firmware update")
parser.add_argument('--custom-fw-d400', type=str, help='Path to custom D400 firmware file')
parser.add_argument('--custom-fw-d555', type=str, help='Path to custom D555 firmware file')
parser.add_argument('--serial', type=str, default=None, help='Serial number of the device to update (for multi-device rigs)')
args = parser.parse_args()


def wait_for_reboot( serial, timeout = 30 ):
    """
    Wait for the camera to finish rebooting after a FW update and re-enumerate in normal
    (non-recovery) mode, returning as soon as it is back -- up to `timeout` seconds.

    The test exit flow may cut USB power (via hub port disable), so we must not return
    while the device is still rebooting, REGARDLESS of rs-fw-update's exit code.

    A same-version reflash comes back quickly, so we return early; a version change may
    reflash a new ISP FW and takes longer to enumerate -- we must give it the full window
    and not interfere with it. Polling handles both without guessing a fixed duration.

    If the flash failed and left the device stuck in recovery, it never re-enumerates in
    normal mode, so we wait the full timeout before letting the caller exit.

    After a flash the device may expose its firmware_update_id (asic serial) and/or its
    normal serial_number, so we match on either.
    """
    # Give the device a moment to drop off the bus and begin rebooting, so we don't match
    # the stale pre-reboot enumeration before it has even started to reset.
    time.sleep( 3 )
    log.d( "waiting up to", timeout, "seconds for device to re-enumerate after FW update..." )
    timer = Timer( timeout )
    timer.start()
    while not timer.has_expired():
        devices = rs.context().devices
        for d in devices:
            try:
                if d.is_in_recovery_mode():
                    continue
                sn = d.get_info( rs.camera_info.serial_number ) if d.supports( rs.camera_info.serial_number ) else None
                fwid = d.get_info( rs.camera_info.firmware_update_id ) if d.supports( rs.camera_info.firmware_update_id ) else None
            except Exception:
                # device dropped off the bus between enumeration and query (still rebooting) -- keep polling
                continue
            if serial is None or serial == sn or serial == fwid:
                if serial is None and len( devices ) > 1:
                    log.w( "no --serial given and multiple devices present; matched the first non-recovery",
                           "device, which may be wrong on a multi-device rig" )
                log.d( "device re-enumerated after", round( timer.get_elapsed(), 1 ), "seconds" )
                return
        time.sleep( 2 )
    log.e( "device did not re-enumerate in normal mode within", timeout, "seconds after FW update" )


def send_hardware_monitor_command(device, command):
    # byte_index = -1
    raw_result = rs.debug_protocol(device).send_and_receive_raw_data(command)

    return raw_result[4:]

import os
import re

def extract_version_from_filename(file_path):
    """
    Extracts the version string from a filename like:
    FlashGeneratedImage_Image5_16_7_0.bin -> 5.16.7
    FlashGeneratedImage_RELEASE_DS5_5_16_3_1.bin -> 5.16.3.1
    rvp-flash-dfu-release-7.56.37749.4831.img -> 7.56.37749.4831

    Args:
        file_path (str): Full path to the file.

    Returns:
        str: Extracted version in format x.y.z or x.y.z.w, or None if not found or if path is invalid.
    """
    if not file_path or not os.path.exists(file_path):
        log.i(f"File not found: {file_path}")
        return None

    filename = os.path.basename(file_path)

    # Match *last* 4 numeric groups before .img/.bin
    # following matching patterns for cases:
    # FlashGeneratedImage_Image5_16_7_0.bin -> 5.16.7
    # FlashGeneratedImage_RELEASE_DS5_5_16_3_1.bin -> 5.16.3.1
    match = re.search(r'(\d+)_(\d+)_(\d+)_(\d+)\.(bin|img)$', filename)
    if not match:
        # Match patterns like rvp-flash-dfu-release-7.56.37749.4831.img -> 7.56.37749.4831
        match = re.search(r'-(\d+)\.(\d+)\.(\d+)\.(\d+)\.(bin|img)$', filename)
        if not match:
            log.i(f"Version not found in filename: {filename}")
            return None

    a, b, c, d, _ = match.groups()

    # Drop the last part only if it equals "0"
    if d == "0":
        return rsutils.version(f"{a}.{b}.{c}")
    else:
        return rsutils.version(f"{a}.{b}.{c}.{d}")


def get_downgrade_counter(device):
    product_line = device.get_info(rs.camera_info.product_line)

    if product_line == "D400":
        opcode = 0x93  # DFU_READ_CNT — reads the actual downgrade counter from flash payload header
        raw_cmd = rs.debug_protocol(device).build_command(opcode)
        counter = send_hardware_monitor_command(device, raw_cmd)
        return counter[0] | (counter[1] << 8)  # uint16_t little-endian
    if product_line == "D500":
        return 0  # D500 do not have downgrade counter
    log.f( "Incompatible product line:", product_line )  # calls sys.exit(1)


def reset_downgrade_counter( device ):
    product_line = device.get_info( rs.camera_info.product_line )

    if product_line == "D400":
        opcode = 0x86  # DFU_RESET_CNT — resets the downgrade counter in flash payload header
        raw_cmd = rs.debug_protocol(device).build_command(opcode)
        send_hardware_monitor_command( device, raw_cmd )
        return
    if product_line == "D500":
        return  # D500 do not have downgrade counter
    log.f( "Incompatible product line:", product_line )  # calls sys.exit(1)

# find the update tool exe
fw_updater_exe = None
fw_updater_exe_regex = r'(^|/)rs-fw-update'
if platform.system() == 'Windows':
    fw_updater_exe_regex += r'\.exe'
fw_updater_exe_regex += '$'
for tool in file.find( repo.build, fw_updater_exe_regex ):
    fw_updater_exe = os.path.join( repo.build, tool )
if not fw_updater_exe:
    log.f( "Could not find the update tool file (rs-fw-update.exe)" )

device, ctx = test.find_first_device_or_exit( args.serial )
product_line = device.get_info( rs.camera_info.product_line )
product_name = device.get_info( rs.camera_info.name )
log.d( 'product line:', product_line )
###############################################################################
#
if device.supports(rs.camera_info.firmware_version):
    current_fw_version = rsutils.version( device.get_info( rs.camera_info.firmware_version ))
    log.d( 'current FW version:', current_fw_version )

# Determine which firmware to use based on product.
# The SDK no longer ships a bundled D400 FW, so a --custom-fw-<plat> path is required
# for every product line; otherwise we cannot exercise the update flow.
custom_fw_path = None
custom_fw_version = None
if product_line == "D400" and args.custom_fw_d400:
    custom_fw_path = args.custom_fw_d400
elif "D555" in product_name and args.custom_fw_d555:
    custom_fw_path = args.custom_fw_d555

if not custom_fw_path:
    log.w("No custom FW path provided (use --custom-fw-d400 / --custom-fw-d555); skipping FW update test")
    exit(0)


test.start( "Update FW" )
# check if recovery. If so recover
recovered = False
if device.is_in_recovery_mode():
    log.d( "recovering device ..." )
    try:
        # rs-fw-update -r requires a *signed* FW image. The caller's --custom-fw-d400
        # is typically unsigned, so we fetch a gold signed FW from S3 to recover with.
        gold_fw = fw_compat.download_gold_d400_fw()
        if not gold_fw:
            log.f( "Could not download gold signed FW; cannot recover DFU device" )
        cmd = [fw_updater_exe, '-r', '-f', gold_fw, '-s', args.serial]
        del device, ctx
        log.d( 'running:', cmd )
        subprocess.run( cmd )
        recovered = True
        fw_compat.reload_d4xx_driver_on_jetson( test.context )
    except Exception as e:
        test.unexpected_exception()
        log.f( "Unexpected error while trying to recover device:", e )
    else:
        # The device's identity changed: in DFU it exposed firmware_update_id only,
        # now in normal mode it exposes its real serial_number (optic_serial). The
        # firmware_update_id (asic_serial) is still exposed and matches what the
        # harness was tracking. Poll for the device to re-enumerate in normal mode
        # (a fresh rs.context() needs time after rs-fw-update exits) -- up to 60s.
        log.d( "waiting for recovered device to re-enumerate in normal mode..." )
        recovered_device = None
        timer = Timer( 60 )
        timer.start()
        while not timer.has_expired():
            for d in rs.context().devices:
                if d.supports( rs.camera_info.firmware_update_id ) \
                   and d.get_info( rs.camera_info.firmware_update_id ) == args.serial \
                   and not d.is_in_recovery_mode():
                    recovered_device = d
                    break
            if recovered_device is not None:
                break
            time.sleep( 2 )
        if recovered_device is None:
            log.f( f"Recovered device with firmware_update_id '{args.serial}' did not "
                   f"re-enumerate within {timer.get_timeout()}s after gold FW flash" )
        # Re-pin args.serial to the device's normal-mode SN so downstream
        # rs-fw-update -s <sn> finds the device (rs-fw-update.cpp:480 uses SN when supported).
        if recovered_device.supports( rs.camera_info.serial_number ):
            new_sn = recovered_device.get_info( rs.camera_info.serial_number )
            if new_sn != args.serial:
                log.d( f're-pinning args.serial: {args.serial} (FWID) -> {new_sn} (SN)' )
                args.serial = new_sn
        device, ctx = test.find_first_device_or_exit( args.serial )
        current_fw_version = rsutils.version(device.get_info(rs.camera_info.firmware_version))
        log.d("FW version after recovery:", current_fw_version)


custom_fw_version = extract_version_from_filename(custom_fw_path)
log.d('Using custom FW version: ', custom_fw_version)

if current_fw_version == custom_fw_version:
    if recovered or 'nightly' not in test.context:
        log.d('versions are same; skipping FW update')
        test.finish()
        test.print_results_and_exit()
    # else: nightly re-flashes the same version on purpose, to exercise the update flow

downgrade_counter = get_downgrade_counter( device )
log.d( 'downgrade counter:', downgrade_counter )
if downgrade_counter == 0xFFFF:
    log.d( 'downgrade counter is uninitialized (0xFFFF), skipping reset' )
    downgrade_counter = 0
elif downgrade_counter >= 19:
    log.d( 'resetting downgrade counter (was', str(downgrade_counter) + ')' )
    reset_downgrade_counter( device )
    log.d( 'sleeping for 3 sec...' )
    time.sleep( 3 )
    downgrade_counter = get_downgrade_counter( device )
    log.d( 'downgrade counter after reset is:', str(downgrade_counter))
    test.check_equal( downgrade_counter, 0 )
    downgrade_counter = 0

image_file = custom_fw_path

cmd = [fw_updater_exe, '-f', image_file]
if args.serial:
    cmd += ['-s', args.serial]
# Add '-u' only if the path doesn't include 'signed'
if ('signed' not in custom_fw_path.lower()
        and "d555" not in product_name.lower()): # currently -u is not supported for D555
    cmd.insert(1, '-u')

# for DDS devices we need to close device and context to detect it back after FW update
del device, ctx
log.d( 'running:', cmd )
sys.stdout.flush()
result = subprocess.run( cmd )   # may throw

# Wait for the camera to finish rebooting before doing anything else, REGARDLESS of
# rs-fw-update's exit code. A non-zero exit doesn't necessarily mean no flash started:
# rs-fw-update may have begun a section flash before erroring out, leaving the device
# mid-reboot. The test exit flow may cut USB power (hub port disable), so we must not
# exit while the device is still rebooting.
wait_for_reboot( args.serial )

if result.returncode != 0:
    log.e( 'rs-fw-update returned exit code', result.returncode )
    test.check( False, description='rs-fw-update should return exit code 0' )
    test.finish()
    test.print_results_and_exit()

# make sure update worked and check FW version and update counter
device, ctx = test.find_first_device_or_exit( args.serial )
current_fw_version = rsutils.version( device.get_info( rs.camera_info.firmware_version ))

# camera_locked returns "YES" (locked) or "NO" (unlocked)
if device.supports( rs.camera_info.camera_locked ) and device.get_info( rs.camera_info.camera_locked ) == 'YES':
    log.w( 'Device is flash-locked' )

test.check_equal(current_fw_version, custom_fw_version)
new_downgrade_counter = get_downgrade_counter( device )
log.d( 'downgrade counter after update:', new_downgrade_counter )

test.finish()
#
###############################################################################

test.print_results_and_exit()
