# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# test:device D555
# test:donotrun:!nightly

import pyrealsense2 as rs
from rspy import log, repo, test
import subprocess

with test.closure("Check rs-dds-configs exists and runs"):
    exe_path = repo.find_built_exe('tools/dds/dds-config', 'rs-dds-config')
    test.check(exe_path, on_fail=test.ABORT)
    
    p = subprocess.run([exe_path, ""], # Enables DDS by default, checks if can connect to a supporting device
                        stdout=None,
                        stderr=subprocess.STDOUT,
                        universal_newlines=True,
                        timeout=10,
                        check=False)  # don't fail on errors
    test.check(p.returncode == 0) # verify success return code    

test.print_results_and_exit()

# The following test was implemented to check tool effect on SDK and device.
# It was decided not to use this at CI runs as it is risky - if fails or aborted in mid test all subsequent tests might fail on incompatible configuration.

# import pyrealsense2 as rs
# import pyrsutils as rsutils
# from rspy import log, repo, test
# import os
# import json

# get_eth_config_opcode = 0xbb
# set_eth_config_opcode = 0xba
# current_eth_config_values = 1

# dev, ctx = test.find_first_device_or_exit()

# def get_curr_device_config(get_default_config = false):
    # raw_command = rs.debug_protocol(dev).build_command(get_eth_config_opcode, current_eth_config_values)
    # raw_result = rs.debug_protocol(dev).send_and_receive_raw_data(raw_command)
    # test.check(raw_result[0] == get_eth_config_opcode)
    # return rsutils.eth_config(raw_result[4:])
 
# def set_device_config(config):
    # raw_command = rs.debug_protocol(dev).build_command(set_eth_config_opcode, 0, 0, 0, 0, config.build_command())
    # raw_result = rs.debug_protocol(dev).send_and_receive_raw_data(raw_command)
    # test.check(raw_result[0] == set_eth_config_opcode)

# def get_config_path():
    # file_name = "realsense-config.json"
    # if os.name == "nt":  # windows
        # base_dir = os.environ.get("appdata")
    # else:  # linux / macos / other unix-like
        # file_name = "." + file_name # Hidden on unix like
        # base_dir = os.environ.get("home")
    # test.check(base_dir)

    # config_path = os.path.join(base_dir, file_name)
    # return config_path

# def get_curr_config_file():
    # if os.name == "nt":  # windows
        # base_dir = os.environ.get("appdata")
    # else:  # linux / macos / other unix-like
        # base_dir = os.environ.get("home")
    # test.check(base_dir)

    # config_path = get_config_path()
    # test.check(config_path)

    # try:
        # with open(config_path, "r", encoding="utf-8") as f:
            # config = json.load(f)
    # except filenotfounderror:
        # raise filenotfounderror(f"config file not found: {config_path}")
    # except json.jsondecodeerror as e:
        # raise valueerror(f"invalid json in {config_path}: {e}")
    
    # return config

# with test.closure("find dds-config tool", on_fail=test.abort):
    # exe_path = repo.find_built_exe('tools/dds/dds-config', 'rs-dds-config')
    # test.check(exe_path)
    
# with test.closure("save configuration file", on_fail=test.abort):    
    # orig_config_file = get_curr_config_file()
    
# with test.closure("save device configuration", on_fail=test.abort):
    # orig_device_config = get_curr_device_config()

# with test.closure("check dds-config affects the sdk"):
    # p = subprocess.run([exe_path, "--disable", "--sdk-domain-id 123"], # won't affect current context run
                        # stdout=none,
                        # stderr=subprocess.stdout,
                        # universal_newlines=true,
                        # timeout=10,
                        # check=false)  # don't fail on errors
    # test.check(p.returncode == 0) # verify success return code    
    # config_file = get_curr_config_file()
    
    # try:
        # test.check_throws( lambda: test.check(config_file["context"]["dds"]["enabled"] ), keyerror )
    # except:
        # test.check(config_file["context"]["dds"]["enabled"], false) # the tool deletes "enabled" key if exists and true, but might have been false before.
    # test.check(config_file["context"]["dds"]["domain"], 123)

    # command = "--sdk-domain-id " + str(orig_config_file["context"]["dds"]["domain"]) # restore sdk domain to find device
    # p = subprocess.run([exe_path, command ], # should enable dds by default
                        # stdout=none,
                        # stderr=subprocess.stdout,
                        # universal_newlines=true,
                        # timeout=10,
                        # check=false)  # don't fail on errors
    # test.check(p.returncode == 0) # verify success return code
    # config_file = get_curr_config_file()
    # test.check(config_file["context"]["dds"]["enabled"], true)

# with test.closure("check dds-config affects the device"):
    # p = subprocess.run([exe_path, "--mtu 7000", "--no-reset"], # 7000 is not commonly used, we can detect the tool change
                        # stdout=none,
                        # stderr=subprocess.stdout,
                        # universal_newlines=true,
                        # timeout=10,
                        # check=false)  # don't fail on errors
    # test.check(p.returncode == 0) # verify success return code    
    # device_config = get_curr_device_config()
    # test.check( device_config.link.mtu == 7000 )
    
# with test.closure( "restore configurations" ):
    # test.check( orig_config_file )
    # test.check( orig_device_config )
    # try:
        # with open(get_config_path(), "w", encoding="utf-8") as f:
            # json.dump(orig_config_file, f, indent=4)
    # except oserror as e:
        # raise oserror(f"failed to write to {config_path}: {e}")
        
    # set_device_config(orig_device_config)

# test.print_results_and_exit()
