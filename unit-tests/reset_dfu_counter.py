# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Simple script to reset the DFU (Device Firmware Update) counter on D400 devices.
"""

import sys
import pyrealsense2 as rs
from rspy import log, test


def send_hardware_monitor_command(device, command):
    """Send a hardware monitor command and return the response."""
    raw_result = rs.debug_protocol(device).send_and_receive_raw_data(command)
    return raw_result[4:]


def get_update_counter(device):
    """Get the current DFU update counter value."""
    product_line = device.get_info(rs.camera_info.product_line)
    
    if product_line != "D400":
        log.f("Error: This script only supports D400 devices. Found:", product_line)
    
    opcode = 0x09
    start_index = 0x30
    size = 0x2
    
    raw_cmd = rs.debug_protocol(device).build_command(opcode, start_index, size)
    counter = send_hardware_monitor_command(device, raw_cmd)
    return counter[0]


def reset_update_counter(device):
    """Reset the DFU update counter to zero."""
    product_line = device.get_info(rs.camera_info.product_line)
    
    if product_line != "D400":
        log.f("Error: This script only supports D400 devices. Found:", product_line)
    
    opcode = 0x86
    raw_cmd = rs.debug_protocol(device).build_command(opcode)
    send_hardware_monitor_command(device, raw_cmd)


def main():
    """Main function to reset DFU counter on a D400 device."""
    # Find the first available device
    device, ctx = test.find_first_device_or_exit()
    
    product_line = device.get_info(rs.camera_info.product_line)
    product_name = device.get_info(rs.camera_info.name)
    
    log.d('Product name:', product_name)
    log.d('Product line:', product_line)
    
    if product_line != "D400":
        log.f("Error: This script only supports D400 devices. Found:", product_line)
    
    # Get current counter value
    current_counter = get_update_counter(device)
    log.d('Current DFU update counter:', current_counter)
    
    if current_counter == 0:
        log.i('DFU counter is already at 0. No reset needed.')
        return
    
    # Reset the counter
    log.i('Resetting DFU update counter...')
    reset_update_counter(device)
    
    # Verify the reset
    new_counter = get_update_counter(device)
    log.d('New DFU update counter:', new_counter)
    
    if new_counter == 0:
        log.i('Successfully reset DFU counter to 0')
    else:
        log.e('Failed to reset DFU counter. Current value:', new_counter)
        sys.exit(1)


if __name__ == "__main__":
    main()
