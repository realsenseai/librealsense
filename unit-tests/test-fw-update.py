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
from rspy import log, test, file, repo, fw_compat, config_file
from rspy.timer import Timer
import time
import argparse

# Parse command-line arguments
parser = argparse.ArgumentParser(description="Test firmware update")
parser.add_argument('--custom-fw-d400', type=str, help='Path to custom D400 firmware file')
parser.add_argument('--custom-fw-d555', type=str, help='Path to custom D555 firmware file')
parser.add_argument('--serial', type=str, default=None, help='Serial number of the device to update (for multi-device rigs)')
args = parser.parse_args()


def wait_for_reboot( same_version ):
    """
    Wait for the camera to finish rebooting after a FW update.
    The test exit flow may cut USB power (via hub port disable), so we must ensure
    the device has had enough time to complete its reboot before we exit.
    When updating to a different version, FW may need time to flash a new ISP FW.
    """
    sleep_time = 60 if not same_version else 3
    log.d( "Waiting", sleep_time, "seconds for device to finish rebooting after FW update..." )
    time.sleep( sleep_time )


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


def recover_dds_device_on_golden_domain( serial ):
    """
    A D555 bricked in DFU reverts to its golden DDS domain (0), so it does NOT appear on the
    rig's configured domain -- our normal discovery below would miss it. The harness detects it
    via a domain-0 fallback and passes its serial here. If a recovery device with `serial` is
    present on domain 0: gold-flash it (on domain 0), then restore its configured DDS domain
    with rs-dds-config using --transient-sdk-domain-id (so nothing is written to realsense-config.json
    and an aborted run can't leave the config on domain 0). After this the device is back on the
    configured domain and the normal flow proceeds. Returns True if a recovery was performed.
    """
    if 'dds' not in test.context:
        return False
    # Look for the recovery device on the golden domain 0. Guard the probe so a DDS hiccup can
    # never disturb the normal discovery/recovery flow below (esp. the USB path on mixed rigs).
    recovery_found = False
    rec_name = None
    try:
        ctx0 = rs.context( { 'dds': { 'enabled': True, 'domain': 0 } } )
        for d in ctx0.query_devices():
            if not d.is_in_recovery_mode():
                continue
            d_id = d.get_info( rs.camera_info.firmware_update_id ) \
                if d.supports( rs.camera_info.firmware_update_id ) else None
            if d_id == serial:
                recovery_found = True
                rec_name = d.get_info( rs.camera_info.name ) if d.supports( rs.camera_info.name ) else None
                break
        del ctx0
    except Exception as e:
        log.d( f"domain-0 DDS recovery probe failed ({e}); proceeding with normal discovery" )
        return False
    if not recovery_found:
        return False

    log.d( f"found recovery device {serial} ({rec_name}) on golden DDS domain 0; recovering ..." )
    gold_fw = fw_compat.download_gold_fw( "D500", "D555" )  # D555 is the only DDS DFU SKU today; revisit for D585 etc.
    if not gold_fw:
        log.f( f"Could not download gold recovery FW for {rec_name} ({serial}); cannot recover DFU device" )
        return False  # defensive: log.f normally exits; never build a -f command with gold_fw=None
    # 1) gold-flash on domain 0 (where a bricked DDS device lives)
    cmd = [fw_updater_exe, '-r', '-f', gold_fw, '-s', serial, '--domain-id', '0']
    log.d( 'running:', cmd )
    result = subprocess.run( cmd )
    if result.returncode != 0:
        log.f( f"Gold-flash failed for {serial} (rc={result.returncode}); device may still be in DFU" )
        return False  # defensive: log.f normally exits; don't report success on a failed flash
    wait_for_reboot( same_version=False )
    # 2) the recovered camera comes back on golden domain 0; restore its configured domain
    #    WITHOUT persisting anything (so an aborted run can't leave the SDK config on domain 0).
    config_domain = config_file.get_domain_from_config_file_or_default()
    if config_domain and config_domain != 0:
        # rs-dds-config is only needed here, to restore a recovered camera's DDS domain
        dds_config_exe = repo.find_built_exe( 'tools/dds/dds-config', 'rs-dds-config' )
        if not dds_config_exe:
            log.f( "Recovered the camera but rs-dds-config was not found to restore its DDS domain" )
            return False  # defensive: log.f normally exits
        # Reach the camera on golden domain 0 (--transient-sdk-domain-id, not persisted) and set
        # its DDS domain to the rig's configured value.
        cmd = [dds_config_exe, '--serial-number', serial,
               '--transient-sdk-domain-id', '0', '--domain-id', str( config_domain )]
        log.d( 'running:', cmd )
        result = subprocess.run( cmd )
        if result.returncode != 0:
            log.w( f"rs-dds-config returned rc={result.returncode}; camera may not be on DDS domain {config_domain}" )
        wait_for_reboot( same_version=False )
    return True


# A bricked DDS camera reverts to golden domain 0 and won't show on the configured domain;
# recover + restore it first so the normal discovery below succeeds.
recovered = recover_dds_device_on_golden_domain( args.serial )

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
same_version = False
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
# check if recovery on the configured domain (e.g. a D400 USB recovery device). If so recover.
# (recovered may already be True from the domain-0 DDS recovery handled above.)
if device.is_in_recovery_mode():
    log.d( "recovering device ..." )
    try:
        # rs-fw-update -r needs a known-good image, which isn't always the caller's
        # --custom-fw-<plat> path (e.g. D400 -r expects a *signed* FW, while the custom
        # image is typically unsigned). Fetch the per-product-line gold FW to recover with.
        gold_fw = fw_compat.download_gold_fw( product_line, product_name )
        if not gold_fw:
            log.f( f"Could not download gold recovery FW for {product_name}; cannot recover DFU device" )
            sys.exit( 1 )  # defensive: log.f normally exits; never build a -f command with gold_fw=None
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
    same_version = True
    if recovered or 'nightly' not in test.context:
        log.d('versions are same; skipping FW update')
        test.finish()
        test.print_results_and_exit()

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
wait_for_reboot( same_version )

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
