# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import sys
import os
import subprocess
import tempfile
import shutil
import re
import pytest
from pytest_check import check
import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy import devices, repo, fw_compat, config_file, libci
from rspy.timer import Timer
import time
import logging

log = logging.getLogger(__name__)

# We want this test to run right after camera detection phase, so that all tests will run
# with updated FW versions, so we give it high priority
pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D555"),
    pytest.mark.device_each("D585"),
    pytest.mark.device_exclude("D585S"),
    pytest.mark.priority(1),
    pytest.mark.timeout(1600),
    pytest.mark.skipif(bool(os.environ.get('GITHUB_ACTIONS')), reason="not runnable on GHA"),
]


# A FW tool that never returns blocks the whole pytest session, and if pytest is then killed the
# tool survives as an orphan whose CWD pins the workspace directory (undeletable on Windows).
FW_TOOL_TIMEOUT = 600  # seconds; a USB flash takes ~50s, a GMSL/MIPI one ~385s


def abs_fw_path( path ):
    """
    Resolve a FW image path against the current working directory, so it stays valid for a tool
    that we run from elsewhere. Returns `path` unchanged when it is empty.
    """
    return os.path.abspath( path ) if path else path


def run_fw_tool( cmd, timeout = FW_TOOL_TIMEOUT ):
    """
    Run a FW tool (rs-fw-update / rs-dds-config) bounded by `timeout`, from a CWD outside the
    repo so a surviving process cannot pin the workspace. On timeout the child is killed and a
    non-zero result is returned, so callers keep their normal reboot-wait and failure flow.
    """
    log.debug( f'running: {cmd}' )
    sys.stdout.flush()
    tool_cwd = tempfile.mkdtemp()
    try:
        return subprocess.run( cmd, cwd=tool_cwd, timeout=timeout )
    except subprocess.TimeoutExpired:
        log.error( f'{cmd[0]} did not exit within {timeout} seconds; killed it' )
        return subprocess.CompletedProcess( cmd, returncode=-1 )
    finally:
        shutil.rmtree( tool_cwd, ignore_errors=True )


def wait_for_reboot( same_version ):
    """
    Wait for the camera to finish rebooting after a FW update.
    The test exit flow may cut USB power (via hub port disable), so we must ensure
    the device has had enough time to complete its reboot before we exit.
    When updating to a different version, FW may need time to flash a new ISP FW.
    """
    sleep_time = 60 if not same_version else 3
    log.debug( f"Waiting {sleep_time} seconds for device to finish rebooting after FW update..." )
    time.sleep( sleep_time )


def send_hardware_monitor_command(device, command):
    # byte_index = -1
    raw_result = rs.debug_protocol(device).send_and_receive_raw_data(command)

    return raw_result[4:]


def extract_version_from_filename(file_path):
    """
    Extracts the version string from a filename like:
    FlashGeneratedImage_Image5_16_7_0.bin -> 5.16.7
    FlashGeneratedImage_RELEASE_DS5_5_16_3_1.bin -> 5.16.3.1
    rvp-flash-dfu-release-7.56.37749.4831.img -> 7.56.37749.4831
    20260727_7.58.40846.12889.img (NIGHTLY build) -> 7.58.40846.12889

    Args:
        file_path (str): Full path to the file.

    Returns:
        str: Extracted version in format x.y.z or x.y.z.w, or None if not found or if path is invalid.
    """
    if not file_path or not os.path.exists(file_path):
        log.info(f"File not found: {file_path}")
        return None

    filename = os.path.basename(file_path)

    # Match *last* 4 numeric groups before .img/.bin
    # following matching patterns for cases:
    # FlashGeneratedImage_Image5_16_7_0.bin -> 5.16.7
    # FlashGeneratedImage_RELEASE_DS5_5_16_3_1.bin -> 5.16.3.1
    match = re.search(r'(\d+)_(\d+)_(\d+)_(\d+)\.(bin|img)$', filename)
    if not match:
        # Match a dot-separated x.y.z.w version immediately before the extension,
        # regardless of what precedes it (hyphen, underscore, a date prefix, etc.), e.g.:
        # rvp-flash-dfu-release-7.56.37749.4831.img -> 7.56.37749.4831
        # 20260727_7.58.40846.12889.img (NIGHTLY build) -> 7.58.40846.12889
        match = re.search(r'(?<!\d)(\d+)\.(\d+)\.(\d+)\.(\d+)\.(bin|img)$', filename)
        if not match:
            log.info(f"Version not found in filename: {filename}")
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
    pytest.fail( f"Incompatible product line: {product_line}" )


def reset_downgrade_counter( device ):
    product_line = device.get_info( rs.camera_info.product_line )

    if product_line == "D400":
        opcode = 0x86  # DFU_RESET_CNT — resets the downgrade counter in flash payload header
        raw_cmd = rs.debug_protocol(device).build_command(opcode)
        send_hardware_monitor_command( device, raw_cmd )
        return
    if product_line == "D500":
        return  # D500 do not have downgrade counter
    pytest.fail( f"Incompatible product line: {product_line}" )


def find_device_or_fail( serial ):
    """
    Mirror of rspy.test.find_first_device_or_exit( serial ): find the device matching `serial`
    in a fresh rs.context(). The match is done against camera_info.serial_number when available,
    falling back to camera_info.firmware_update_id for devices in DFU/recovery mode (where
    serial_number isn't exposed) -- mirroring how rspy.devices registers devices on discovery.
    Fails the test if no such device is visible.
    """
    c = rs.context()
    if not c.devices.size():
        pytest.fail( "No device found" )
    for d in c.devices:
        if d.supports( rs.camera_info.serial_number ):
            d_sn = d.get_info( rs.camera_info.serial_number )
        elif d.supports( rs.camera_info.firmware_update_id ):
            d_sn = d.get_info( rs.camera_info.firmware_update_id )
        else:
            continue
        if d_sn == serial:
            log.debug( f'found {d}' )
            return d, c
    pytest.fail( f"No device with serial number / firmware-update ID '{serial}' is visible" )


def recover_dds_device_on_golden_domain( serial, context, fw_updater_exe ):
    """
    A D555 bricked in DFU reverts to its golden DDS domain (0), so it does NOT appear on the
    rig's configured domain -- our normal discovery below would miss it. The harness detects it
    via a domain-0 fallback and passes its serial here. If a recovery device with `serial` is
    present on domain 0: gold-flash it (on domain 0), then restore its configured DDS domain
    with rs-dds-config using --transient-sdk-domain-id (so nothing is written to realsense-config.json
    and an aborted run can't leave the config on domain 0). After this the device is back on the
    configured domain and the normal flow proceeds. Returns True if a recovery was performed.
    """
    if 'dds' not in context:
        return False
    # A DDS context also enumerates USB devices: never treat a USB device in recovery as a
    # bricked DDS camera (its gold image and flash path are different)
    rspy_dev = devices.get( serial )
    if rspy_dev and not rspy_dev.is_dds:
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
        log.debug( f"domain-0 DDS recovery probe failed ({e}); proceeding with normal discovery" )
        return False
    if not recovery_found:
        return False

    log.debug( f"found recovery device {serial} ({rec_name}) on golden DDS domain 0; recovering ..." )
    gold_fw = fw_compat.download_gold_fw( "D500", "D555" )  # D555 is the only DDS DFU SKU today; revisit for D585 etc.
    if not gold_fw:
        pytest.fail( f"Could not download gold recovery FW for {rec_name} ({serial}); cannot recover DFU device" )
    # 1) gold-flash on domain 0 (where a bricked DDS device lives)
    cmd = [fw_updater_exe, '-r', '-f', gold_fw, '-s', serial, '--domain-id', '0']
    result = run_fw_tool( cmd )
    if result.returncode != 0:
        pytest.fail( f"Gold-flash failed for {serial} (rc={result.returncode}); device may still be in DFU" )
    wait_for_reboot( same_version=False )
    # 2) the recovered camera comes back on golden domain 0; restore its configured domain
    #    WITHOUT persisting anything (so an aborted run can't leave the SDK config on domain 0).
    config_domain = config_file.get_domain_from_config_file_or_default()
    if config_domain and config_domain != 0:
        # rs-dds-config is only needed here, to restore a recovered camera's DDS domain
        dds_config_exe = repo.find_built_exe( 'tools/dds/dds-config', 'rs-dds-config' )
        if not dds_config_exe:
            pytest.fail( "Recovered the camera but rs-dds-config was not found to restore its DDS domain" )
        # Reach the camera on golden domain 0 (--transient-sdk-domain-id, not persisted) and set
        # its DDS domain to the rig's configured value.
        cmd = [dds_config_exe, '--serial-number', serial,
               '--transient-sdk-domain-id', '0', '--domain-id', str( config_domain )]
        result = run_fw_tool( cmd, timeout = 30 )  # a domain write plus reboot, not a flash
        if result.returncode != 0:
            log.warning( f"rs-dds-config returned rc={result.returncode}; camera may not be on DDS domain {config_domain}" )
        wait_for_reboot( same_version=False )
    return True


def test_fw_update( request, module_device_setup, test_context_var ):
    serial = module_device_setup
    context = test_context_var

    # find the update tool exe
    fw_updater_exe = repo.find_built_exe( 'tools/fw-update', 'rs-fw-update' )
    if not fw_updater_exe:
        pytest.fail( "Could not find the update tool file (rs-fw-update.exe)" )

    # Absolutize here, in the parent: run_fw_tool runs the tool from its own directory, so a
    # relative --custom-fw-* path would no longer resolve for the child.
    custom_fw_d400 = abs_fw_path( request.config.getoption( '--custom-fw-d400' ) )
    custom_fw_d555 = abs_fw_path( request.config.getoption( '--custom-fw-d555' ) )
    custom_fw_d585 = abs_fw_path( request.config.getoption( '--custom-fw-d585' ) )

    # FW-compat pre-flash gate (was harness-side, in run-unit-tests.py): refuse to flash a
    # below-min image, or substitute the per-device fallback image registered in
    # rspy/fw_fallback.json. A refusal is a FAILURE (not a skip), matching the legacy harness.
    rspy_dev = devices.get( serial )
    gate_skip, fw_d400_override = fw_compat.resolve_fw_gate(
        rspy_dev, libci.home, 'pytest-fw-update', sn=serial,
        custom_fw_d400_path=custom_fw_d400,
        custom_fw_d555_path=custom_fw_d555 )
    if gate_skip:
        pytest.fail( f"candidate FW for {rspy_dev.name}_{serial} is below the device's minimum "
                     f"supported FW and no fallback is registered in rspy/fw_fallback.json" )
    if fw_d400_override:
        custom_fw_d400 = fw_d400_override

    # A bricked DDS camera reverts to golden domain 0 and won't show on the configured domain;
    # recover + restore it first so the normal discovery below succeeds.
    recovered = recover_dds_device_on_golden_domain( serial, context, fw_updater_exe )

    device, ctx = find_device_or_fail( serial )
    product_line = device.get_info( rs.camera_info.product_line )
    product_name = device.get_info( rs.camera_info.name )
    # A D500 device in recovery enumerates under the family-wide "D500 Recovery"; the harness
    # renames it from the device map (device_map.name_recovery_devices), so take the SKU from there
    if device.is_in_recovery_mode() and rspy_dev and rspy_dev.name:
        product_name = rspy_dev.name
    log.debug( f'product line: {product_line}, name: {product_name}' )
    ###############################################################################
    #
    if device.supports(rs.camera_info.firmware_version):
        current_fw_version = rsutils.version( device.get_info( rs.camera_info.firmware_version ))
        log.debug( f'current FW version: {current_fw_version}' )

    # Determine which firmware to use based on product.
    # The SDK no longer ships a bundled D400 FW, so a --custom-fw-<plat> path is required
    # for every product line; otherwise we cannot exercise the update flow.
    same_version = False
    custom_fw_path = None
    custom_fw_version = None
    if product_line == "D400" and custom_fw_d400:
        custom_fw_path = custom_fw_d400
    elif "D555" in product_name and custom_fw_d555:
        custom_fw_path = custom_fw_d555
    elif "D585" in product_name and custom_fw_d585:
        custom_fw_path = custom_fw_d585

    if not custom_fw_path:
        pytest.skip("No custom FW path provided (use --custom-fw-d400 / --custom-fw-d555 / --custom-fw-d585); skipping FW update test")

    # check if recovery on the configured domain (e.g. a D400 USB recovery device). If so recover.
    # (recovered may already be True from the domain-0 DDS recovery handled above.)
    if device.is_in_recovery_mode():
        if product_name.startswith( "D585S" ):
            pytest.fail( f"{serial} is a D585S in recovery; not flashing a safety camera from LibCI" )
        log.debug( "recovering device ..." )
        # rs-fw-update -r needs a known-good image, which isn't always the caller's
        # --custom-fw-<plat> path (e.g. D400 -r expects a *signed* FW, while the custom
        # image is typically unsigned). Fetch the per-product-line gold FW to recover with.
        gold_fw = fw_compat.download_gold_fw( product_line, product_name )
        if not gold_fw and product_line == "D500":
            # D500 FW is unsigned-only, so the custom image is as good as the gold one for -r
            log.debug( f"no gold FW for {product_name}; recovering with the custom image {custom_fw_path}" )
            gold_fw = custom_fw_path
        if not gold_fw:
            pytest.fail( f"Could not download gold recovery FW for {product_name}; cannot recover DFU device" )
        cmd = [fw_updater_exe, '-r', '-f', gold_fw, '-s', serial]
        del device, ctx
        result = run_fw_tool( cmd )
        if result.returncode != 0 and product_line == "D500":
            pytest.fail( f"Recovery flash of {product_name} ({serial}) with {gold_fw} failed (rc={result.returncode})" )
        recovered = True
        fw_compat.reload_d4xx_driver_on_jetson( context )
        # The device's identity changed: in DFU it exposed firmware_update_id only,
        # now in normal mode it exposes its real serial_number (optic_serial). The
        # firmware_update_id (asic_serial) is still exposed and matches what the
        # harness was tracking. Poll for the device to re-enumerate in normal mode
        # (a fresh rs.context() needs time after rs-fw-update exits) -- up to 60s.
        log.debug( "waiting for recovered device to re-enumerate in normal mode..." )
        recovered_device = None
        timer = Timer( 60 )
        timer.start()
        while not timer.has_expired():
            for d in rs.context().devices:
                if d.supports( rs.camera_info.firmware_update_id ) \
                   and d.get_info( rs.camera_info.firmware_update_id ) == serial \
                   and not d.is_in_recovery_mode():
                    recovered_device = d
                    break
            if recovered_device is not None:
                break
            time.sleep( 2 )
        if recovered_device is None:
            pytest.fail( f"Recovered device with firmware_update_id '{serial}' did not "
                         f"re-enumerate within {timer.get_timeout()}s after gold FW flash" )
        # Re-pin the serial to the device's normal-mode SN so downstream
        # rs-fw-update -s <sn> finds the device (rs-fw-update.cpp:480 uses SN when supported).
        if recovered_device.supports( rs.camera_info.serial_number ):
            new_sn = recovered_device.get_info( rs.camera_info.serial_number )
            if new_sn != serial:
                log.debug( f're-pinning serial: {serial} (FWID) -> {new_sn} (SN)' )
                serial = new_sn
        device, ctx = find_device_or_fail( serial )
        current_fw_version = rsutils.version(device.get_info(rs.camera_info.firmware_version))
        log.debug(f"FW version after recovery: {current_fw_version}")


    custom_fw_version = extract_version_from_filename(custom_fw_path)
    log.debug(f'Using custom FW version: {custom_fw_version}')

    if current_fw_version == custom_fw_version:
        same_version = True
        if recovered or 'nightly' not in context:
            log.debug('versions are same; skipping FW update')
            return

    downgrade_counter = get_downgrade_counter( device )
    log.debug( f'downgrade counter: {downgrade_counter}' )
    if downgrade_counter == 0xFFFF:
        log.debug( 'downgrade counter is uninitialized (0xFFFF), skipping reset' )
        downgrade_counter = 0
    elif downgrade_counter >= 19:
        log.debug( f'resetting downgrade counter (was {downgrade_counter})' )
        reset_downgrade_counter( device )
        log.debug( 'sleeping for 3 sec...' )
        time.sleep( 3 )
        downgrade_counter = get_downgrade_counter( device )
        log.debug( f'downgrade counter after reset is: {downgrade_counter}' )
        check.equal( downgrade_counter, 0 )
        downgrade_counter = 0

    image_file = custom_fw_path

    cmd = [fw_updater_exe, '-f', image_file]
    if serial:
        cmd += ['-s', serial]
    # Add '-u' only if the path doesn't include 'signed'
    if ('signed' not in custom_fw_path.lower()
            and "d555" not in product_name.lower()  # currently -u is not supported for D555
            and "d585" not in product_name.lower()): # nor for D585/D585S
        cmd.insert(1, '-u')

    # for DDS devices we need to close device and context to detect it back after FW update
    del device, ctx
    result = run_fw_tool( cmd )

    # Wait for the camera to finish rebooting before doing anything else, REGARDLESS of
    # rs-fw-update's exit code. A non-zero exit doesn't necessarily mean no flash started:
    # rs-fw-update may have begun a section flash before erroring out, leaving the device
    # mid-reboot. The test exit flow may cut USB power (hub port disable), so we must not
    # exit while the device is still rebooting.
    wait_for_reboot( same_version )

    if result.returncode != 0:
        pytest.fail( f'rs-fw-update should return exit code 0 (got {result.returncode})' )

    # make sure update worked and check FW version and update counter
    device, ctx = find_device_or_fail( serial )
    current_fw_version = rsutils.version( device.get_info( rs.camera_info.firmware_version ))

    # camera_locked returns "YES" (locked) or "NO" (unlocked)
    if device.supports( rs.camera_info.camera_locked ) and device.get_info( rs.camera_info.camera_locked ) == 'YES':
        log.warning( 'Device is flash-locked' )

    check.equal(current_fw_version, custom_fw_version)
    new_downgrade_counter = get_downgrade_counter( device )
    log.debug( f'downgrade counter after update: {new_downgrade_counter}' )
    #
    ###############################################################################
