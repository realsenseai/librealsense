# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense Inc. All Rights Reserved.

# Currently only D555 supports DDS configuration natively
# test:device D555

import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy import test


get_eth_config_opcode = 0xBB
set_eth_config_opcode = 0xBA

default_values_param = 0
current_values_param = 1


dev, ctx = test.find_first_device_or_exit()

def get_eth_config( get_default_config = False ):
    raw_command = rs.debug_protocol( dev ).build_command( get_eth_config_opcode, default_values_param if get_default_config else current_values_param )
    raw_result = rs.debug_protocol( dev ).send_and_receive_raw_data( raw_command )
    test.check( raw_result[0] == get_eth_config_opcode )
    return rsutils.eth_config( raw_result[4:] )
 
def set_eth_config( config ):
    raw_command = rs.debug_protocol( dev ).build_command( set_eth_config_opcode, 0, 0, 0, 0, config.build_command() )
    raw_result = rs.debug_protocol( dev ).send_and_receive_raw_data( raw_command )
    test.check( raw_result[0] == set_eth_config_opcode )


with test.closure("Test DDS support"):
    orig_config = get_eth_config()
    new_config = get_eth_config() # Get a new config object to keep orig_config intact

with test.closure("Test link timeout configuration"):
    new_config.link.timeout *= 2
    set_eth_config( new_config )
    updated_config = get_eth_config()
    test.check( updated_config.link.timeout == orig_config.link.timeout * 2 )

    if new_config.header.version >= 5:
        new_config.link.timeout = 1000
        try:
            set_eth_config( new_config )
        except ValueError as e:
            test.check_exception( e, ValueError, "Link timeout should be 2000-30000. Current 1000" )
        else:
            test.unreachable()

        new_config.link.timeout = 31000
        try:
            set_eth_config( new_config )
        except ValueError as e:
            test.check_exception( e, ValueError, "Link timeout should be 2000-30000. Current 31000" )
        else:
            test.unreachable()

        new_config.link.timeout = 2345
        try:
            set_eth_config( new_config )
        except ValueError as e:
            test.check_exception( e, ValueError, "Link timeout must be divisible by 100. Current 2345" )
        else:
            test.unreachable()

with test.closure("Test MTU configuration"):
    new_config.link.mtu = 4000
    if new_config.header.version == 3:
        try:
            set_eth_config( new_config )
        except ValueError as e:
            test.check_exception( e, ValueError, "Camera FW supports only MTU 9000." )
        else:
            test.unreachable()
    else:
        set_eth_config( new_config )
        updated_config = get_eth_config()
        test.check( updated_config.link.mtu == 4000 )
        
        new_config.link.mtu = 0
        try:
            set_eth_config( new_config )
        except ValueError as e:
            test.check_exception( e, ValueError, "MTU size should be 500-9000. Current 0" )
        else:
            test.unreachable()

        new_config.link.mtu = 1234
        try:
            set_eth_config( new_config )
        except ValueError as e:
            test.check_exception( e, ValueError, "MTU size must be divisible by 500. Current 1234" )
        else:
            test.unreachable()
    new_config.link.mtu = orig_config.link.mtu # Restore field that might fail other tests, depending header version.


with test.closure("Test transmission delay configuration"):
    new_config.transmission_delay = 21
    if new_config.header.version == 3:
        try:
            set_eth_config( new_config )
        except ValueError as e:
            test.check_exception( e, ValueError, "Camera FW does not support transmission delay." )
        else:
            test.unreachable()
    else:
        set_eth_config( new_config )
        updated_config = get_eth_config()
        test.check( updated_config.transmission_delay == 21 )

        new_config.transmission_delay = 222
        try:
            set_eth_config( new_config )
        except ValueError as e:
            test.check_exception( e, ValueError, "Transmission delay should be 0-144. Current 222" )
        else:
            test.unreachable()
        
        new_config.transmission_delay = 100
        try:
            set_eth_config( new_config )
        except ValueError as e:
            test.check_exception( e, ValueError, "Transmission delay must be divisible by 3. Current 100" )
        else:
            test.unreachable()
    new_config.transmission_delay = orig_config.transmission_delay # Restore field that might fail other tests, depending header version.

with test.closure("Test UDP TTL configuration"):
    new_config.udp_ttl = 128
    if new_config.header.version < 5:
        try:
            set_eth_config( new_config )
        except ValueError as e:
            test.check_exception( e, ValueError, "Camera FW does not support changing UDP TTL value." )
        else:
            test.unreachable()
    else:
        set_eth_config( new_config )
        updated_config = get_eth_config()
        test.check( updated_config.udp_ttl == 128 )

        new_config.udp_ttl = 300
        try:
            set_eth_config( new_config )
        except ValueError as e:
            test.check_exception( e, ValueError, "UDP TTL should be 1-255 (or 0 for system default). Current 300" )
        else:
            test.unreachable()
    new_config.udp_ttl = orig_config.udp_ttl # Restore field that might fail other tests, depending header version.

with test.closure("Test configuration failures"): # Failures depending on version tested separately
    new_config.header.version = 2
    try:
        set_eth_config( new_config )
    except ValueError as e:
        test.check_exception( e, ValueError, "Unrecognized Eth config table version 2" )
    else:
        test.unreachable()
    new_config.header.version = orig_config.header.version
    
    new_config.configured.ip = rsutils.ip_address( 0, 0 ,0 ,0 )
    try:
        set_eth_config( new_config )
    except ValueError as e:
        test.check_exception( e, ValueError, "Invalid configured IP address 0.0.0.0" )
    else:
        test.unreachable()
    new_config.configured.ip = orig_config.configured.ip

    new_config.configured.netmask = rsutils.ip_address( 0, 0 ,0 ,0 )
    try:
        set_eth_config( new_config )
    except ValueError as e:
        test.check_exception( e, ValueError, "Invalid configured network mask 0.0.0.0" )
    else:
        test.unreachable()
    new_config.configured.netmask = orig_config.configured.netmask
    
    # Don't set valid domain_id, it might cause DDS devices to loose connection (in case of reset/power loss).
    new_config.dds.domain_id = -1
    try:
        set_eth_config( new_config )
    except ValueError as e:
        test.check_exception( e, ValueError, "Domain ID should be in 0-232 range. Current -1" )
    else:
        test.unreachable()
    new_config.dds.domain_id = 233
    try:
        set_eth_config( new_config )
    except ValueError as e:
        test.check_exception( e, ValueError, "Domain ID should be in 0-232 range. Current 233" )
    else:
        test.unreachable()
    new_config.dds.domain_id = orig_config.dds.domain_id

with test.closure("Test python wrapper functionality"):
    eth_device = rs.eth_config_device( dev )
    orig_link_timeout = eth_device.get_link_timeout()
    eth_device.set_link_timeout( orig_link_timeout * 2 )
    updated_link_timeout = eth_device.get_link_timeout()
    test.check( updated_link_timeout == orig_link_timeout * 2 )
    
    orig_ip, orig_actual_ip = eth_device.get_ip_address()
    eth_device.set_ip_address( rs.ip_address( 127, 0, 0, 1 ) )
    new_ip, new_actual_ip = eth_device.get_ip_address()
    test.check( new_ip == rs.ip_address( 127, 0, 0, 1 ) )
    
    orig_link_priority = eth_device.get_link_priority()
    priority_to_set = rs.link_priority.usb_first if orig_link_priority != rs.link_priority.usb_first else rs.link_priority.eth_first
    eth_device.set_link_priority( priority_to_set )
    new_link_priority = eth_device.get_link_priority()
    test.check( new_link_priority == priority_to_set )
    
with test.closure("Restore configuration"):
    set_eth_config( orig_config )        
    
test.print_results_and_exit()
