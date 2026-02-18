# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

#We want this test to run right after camera detection phase, so that all tests will run with updated FW versions, so we give it high priority
#test:priority 1
#test:timeout 500
#test:donotrun:gha
#test:device each(D400*)
#test:device D555

import sys
import os
import subprocess
import re
import platform
import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy import log, test, file, repo
import time
import argparse

# Parse command-line arguments
parser = argparse.ArgumentParser(description="Test firmware update")
parser.add_argument('--custom-fw-d400', type=str, help='Path to custom D400 firmware file')
parser.add_argument('--custom-fw-d555', type=str, help='Path to custom D555 firmware file')
args = parser.parse_args()


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


def get_update_counter(device):
    product_line = device.get_info(rs.camera_info.product_line)
    opcode = 0x09
    start_index = 0x30
    size = None

    if product_line == "D400":
        size = 0x2
    elif product_line == "D500":
        return 0 # D500 do not have update counter
    else:
        log.f( "Incompatible product line:", product_line )

    raw_cmd = rs.debug_protocol(device).build_command(opcode, start_index, size)
    counter = send_hardware_monitor_command(device, raw_cmd)
    return counter[0]


def reset_update_counter( device ):
    product_line = device.get_info( rs.camera_info.product_line )

    if product_line == "D400":
        opcode = 0x86
        raw_cmd = rs.debug_protocol(device).build_command(opcode)
    else:
        log.f( "Incompatible product line:", product_line )

    send_hardware_monitor_command( device, raw_cmd )

def find_image_or_exit( product_name, fw_version_regex = r'(\d+\.){3}(\d+)' ):
    """
    Searches for a FW image file for the given camera name and optional version. If none are
    found, exits with an error!

    :param product_name: the name of the camera, from get_info(rs.camera_info.name)
    :param fw_version_regex: optional regular expression specifying which FW version image to find

    :return: the image file corresponding to product_name and fw_version if exist, otherwise exit
    """
    pattern = re.compile( r'^Intel RealSense (((\S+?)(\d+))(\S*))' )
    match = pattern.search( product_name )
    if not match:
        raise RuntimeError( "Failed to parse product name '" + product_name + "'" )

    # For a product 'PR567abc', we want to search, in order, these combinations:
    #     PR567abc
    #     PR306abX
    #     PR306aXX
    #     PR306
    #     PR30X
    #     PR3XX
    # Each of the above, combined with the FW version, should yield an image name like:
    #     PR567aXX_FW_Image-<fw-version>.bin
    suffix = 5             # the suffix
    for j in range(1, 3):  # with suffix, then without
        start_index, end_index = match.span(j)
        for i in range(0, len(match.group(suffix))):
            pn = product_name[start_index:end_index-i]
            image_name = '(^|/)' + pn + i*'X' + "_FW_Image-" + fw_version_regex + r'\.bin$'
            for image in file.find(repo.root, image_name):
                return os.path.join( repo.root, image )
        suffix -= 1
    #
    # If we get here, we didn't find any image...
    global product_line
    log.f( "Could not find image file for", product_line )

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

device, ctx = test.find_first_device_or_exit()
product_line = device.get_info( rs.camera_info.product_line )
product_name = device.get_info( rs.camera_info.name )
log.d( 'product line:', product_line )
###############################################################################
#

current_fw_version = rsutils.version( device.get_info( rs.camera_info.firmware_version ))
log.d( 'current FW version:', current_fw_version )

# Determine which firmware to use based on product
bundled_fw_version = rsutils.version("")
custom_fw_path = None
custom_fw_version = None
if product_line == "D400" and args.custom_fw_d400:
    custom_fw_path = args.custom_fw_d400
elif "D555" in product_name and args.custom_fw_d555:
    custom_fw_path = args.custom_fw_d555


test.start( "Update FW" )
# check if recovery. If so recover
recovered = False
if device.is_in_recovery_mode():
    log.d( "recovering device ..." )
    try:
        # always flash signed fw when device on recovery before flashing anything else
        # on D555 we currently do not have bundled FW
        image_file = find_image_or_exit(product_name) if "D555" not in product_name else custom_fw_path
        cmd = [fw_updater_exe, '-r', '-f', image_file]
        del device, ctx
        log.d( 'running:', cmd )
        subprocess.run( cmd )
        recovered = True

        if 'jetson' in test.context:
            # Reload d4xx mipi driver on Jetson
            log.d("Reloading d4xx driver on Jetson...")
            subprocess.run(['sudo', 'modprobe', '-r', 'd4xx'], check=True) # force remove
            subprocess.run(['sudo', 'modprobe', 'd4xx'], check=True) # load
    except Exception as e:
        test.unexpected_exception()
        log.f( "Unexpected error while trying to recover device:", e )
    else:
        device, ctx = test.find_first_device_or_exit()
        current_fw_version = rsutils.version(device.get_info(rs.camera_info.firmware_version))
        log.d("FW version after recovery:", current_fw_version)


if custom_fw_path:
    custom_fw_version = extract_version_from_filename(custom_fw_path)
    log.d('Using custom FW version: ', custom_fw_version)
elif device.supports(rs.camera_info.recommended_firmware_version):  # currently, D500 does not support recommended FW
    log.i(f"No Custom firmware path provided. using bundled firmware")
    bundled_fw_version = rsutils.version(device.get_info(rs.camera_info.recommended_firmware_version))
    log.d('bundled FW version:', bundled_fw_version)
else:
    log.w("No custom FW provided and no bundled FW version available; skipping FW update test")
    exit(0)

if (current_fw_version == bundled_fw_version and not custom_fw_path) or \
   (current_fw_version == custom_fw_version):
    if recovered or 'nightly' not in test.context:
        log.d('versions are same; skipping FW update')
        test.finish()
        test.print_results_and_exit()

update_counter = get_update_counter( device )
log.d( 'update counter:', update_counter )
if update_counter >= 19:
    log.d( 'resetting update counter' )
    reset_update_counter( device )
    update_counter = 0

fw_version_regex = bundled_fw_version.to_string()
if not bundled_fw_version.build():
    fw_version_regex += ".0"  # version drops the build if 0
fw_version_regex = re.escape( fw_version_regex )
image_file = find_image_or_exit(product_name, fw_version_regex) if not custom_fw_path else custom_fw_path
# finding file containing image for FW update

cmd = [fw_updater_exe, '-f', image_file]
if custom_fw_path:
    # Add '-u' only if the path doesn't include 'signed'
    if ('signed' not in custom_fw_path.lower()
            and "d555" not in product_name.lower()): # currently -u is not supported for D555
        cmd.insert(1, '-u')

# for DDS devices we need to close device and context to detect it back after FW update
del device, ctx
log.d( 'running:', cmd )
sys.stdout.flush()
subprocess.run( cmd )   # may throw

# make sure update worked
time.sleep(3) # MIPI devices do not re-enumerate so we need to give them some time to restart
device, ctx = test.find_first_device_or_exit()
current_fw_version = rsutils.version( device.get_info( rs.camera_info.firmware_version ))

expected_fw_version = custom_fw_version if custom_fw_path else bundled_fw_version
test.check_equal(current_fw_version, expected_fw_version)
new_update_counter = get_update_counter( device )
# According to FW: "update counter zeros if you load newer FW than (ever) before"
# TODO: check why update counter is 255 when installing cutom fw
if new_update_counter > 0 and not custom_fw_version:
    test.check_equal( new_update_counter, update_counter + 1 )

test.finish()
#
###############################################################################

test.print_results_and_exit()
