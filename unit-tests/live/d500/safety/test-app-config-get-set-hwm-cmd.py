# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

# Not frequently changing, no need to test for each commit
# test:donotrun:!nightly
# test:device D585S

import time
import numpy as np
import zlib
import pyrealsense2 as rs
from rspy import test, log, devices
from rspy import tests_wrapper as tw


DEFAULT_PERCENTAGE_THRESHOLD = 80 #80%

def build_header(table_array, table_id, version_array):
    version_major = version_array[0]
    version_minor = version_array[1]
    table_size = len(table_array)
    print ("table size", table_size)
    counter = 0
    test_cal_version = 0
    crc_computed = zlib.crc32(table_array)

    table_id_ints = np.frombuffer(table_id.to_bytes(2, 'little'), dtype=np.uint8)
    table_size_ints = np.frombuffer(table_size.to_bytes(4, 'little'), dtype=np.uint8)
    counter_ints = np.frombuffer(counter.to_bytes(2, 'little'), dtype=np.uint8)
    test_cal_version_ints = np.frombuffer(test_cal_version.to_bytes(2, 'little'), dtype=np.uint8)
    crc_ints = np.frombuffer(crc_computed.to_bytes(4, 'little'), dtype=np.uint8)

    version_ints = np.hstack((version_major, version_minor))
    with_table_id = np.hstack((version_ints, table_id_ints))
    with_table_size = np.hstack((with_table_id, table_size_ints))
    with_counter = np.hstack((with_table_size, counter_ints))
    with_test_cal_version = np.hstack((with_counter, test_cal_version_ints))
    header_ready = np.hstack((with_test_cal_version, crc_ints))

    return header_ready


def to_np_array(val, num_of_bytes, np_type=np.uint8):
    return np.frombuffer(val.to_bytes(num_of_bytes, 'little'), np_type)


def generate_sip_part():
    sip_immediate_mode_safety_features_selection = 0
    sip_temporal_safety_features_selection = 0
    sip_mechanisms_thresholds = to_np_array(0, 16)
    sip_mechanisms_sampling_interval = to_np_array(0, 8)
    tc_consecutives_failures_threshold = to_np_array(3, 1)
    sip_reserved = to_np_array(0, 63)

    sip1 = np.asarray([sip_immediate_mode_safety_features_selection, sip_temporal_safety_features_selection], dtype=np.uint8)
    sip2 = np.hstack((sip1, sip_mechanisms_thresholds), dtype=np.uint8)
    sip3 = np.hstack((sip2, sip_mechanisms_sampling_interval), dtype=np.uint8)
    sip4 = np.hstack((sip3, tc_consecutives_failures_threshold), dtype=np.uint8)
    sip5 = np.hstack((sip4, sip_reserved), dtype=np.uint8)

    return sip5


def generate_dev_rules_selection_bitmap():
    # "Bit Mask values:
    # 0x1 <<0 - Dev Mode is active. Note that if this bit is inactive then all the other can be safely disregarded
    # 0x1 <<1 - Feat_#1 enabled
    # 0x1 <<2 - Feat_#2 enabled
    # 0x1 <<3 - ...
    # 0x1 <<63 - Feat_#63 enabled"

    bitmap = np.uint64(0)

    # Set bit number 0 (Dev Mode is active. Note that if this bit is inactive then all the other can be safely disregarded)
    bitmap |= np.uint64(1) << np.uint64(0)

    # Set bit number 6 (Feat_#6: SHT4x Humidity Threshold)
    bitmap |= np.uint64(1) << np.uint64(6)

    # bitmap: 0000000000000000000000000000000000000000000000000000000001000001
    # log.d(f"Bitmap: {bitmap:064b}") # uncomment this line for debug print of bitmap

    # convert 64-bit-map to 8 bytes np array
    bitmap_bytes = np.frombuffer(bitmap.tobytes(), dtype=np.uint8)
    return bitmap_bytes

def generate_features_1_4():

    depth_pipe_safety_checks_override = 0
    triggered_calib_safety_checks_override = 0
    smcu_bypass_directly_to_maintenance_mode = 0
    smcu_skip_spi_error = 0

    arr = np.asarray([depth_pipe_safety_checks_override, triggered_calib_safety_checks_override,
                       smcu_bypass_directly_to_maintenance_mode, smcu_skip_spi_error], dtype=np.uint8)
    return arr


def default_temp_threshold():
    minus_40_in_uint8 = 216
    minus_50_in_uint8 = 206
    return np.asarray([100, 120, minus_40_in_uint8, minus_50_in_uint8], dtype=np.uint8)


def generate_temp_thresholds():
    temp_thresholds_ir_right = default_temp_threshold()
    temp_thresholds_ir_left = default_temp_threshold()
    temp_thresholds_apm_left = default_temp_threshold()
    temp_thresholds_apm_right = default_temp_threshold()
    temp_thresholds_reserved = default_temp_threshold()
    temp_thresholds_hkr_core = default_temp_threshold()
    temp_thresholds_smcu = default_temp_threshold()
    temp_thresholds_reserved2 = default_temp_threshold()
    temp_thresholds_sht4x = default_temp_threshold()
    temp_thresholds_reserved3 = default_temp_threshold()
    temp_thresholds_imu = default_temp_threshold()

    temp_1 = np.hstack((temp_thresholds_ir_right, temp_thresholds_ir_left))
    temp_2 = np.hstack((temp_1, temp_thresholds_apm_left))
    temp_3 = np.hstack((temp_2, temp_thresholds_apm_right))
    temp_4 = np.hstack((temp_3, temp_thresholds_reserved))
    temp_5 = np.hstack((temp_4, temp_thresholds_hkr_core))
    temp_6 = np.hstack((temp_5, temp_thresholds_smcu))
    temp_7 = np.hstack((temp_6, temp_thresholds_reserved2))
    temp_8 = np.hstack((temp_7, temp_thresholds_sht4x))
    temp_9 = np.hstack((temp_8, temp_thresholds_reserved3))
    temp_10 = np.hstack((temp_9, temp_thresholds_imu))

    return temp_10



def generate_humid_volt_thresholds():
    humidity_threshold_sht4x = DEFAULT_PERCENTAGE_THRESHOLD
    voltage_thresholds_vdd3v3 = DEFAULT_PERCENTAGE_THRESHOLD
    voltage_thresholds_vdd1v8 = DEFAULT_PERCENTAGE_THRESHOLD
    voltage_thresholds_vdd1v2 = DEFAULT_PERCENTAGE_THRESHOLD
    voltage_thresholds_vdd1v1 = DEFAULT_PERCENTAGE_THRESHOLD
    voltage_thresholds_vdd0v8 = DEFAULT_PERCENTAGE_THRESHOLD
    voltage_thresholds_vdd0v6 = DEFAULT_PERCENTAGE_THRESHOLD
    voltage_thresholds_vdd5vo_u = DEFAULT_PERCENTAGE_THRESHOLD
    voltage_thresholds_vdd5vo_l = DEFAULT_PERCENTAGE_THRESHOLD
    voltage_thresholds_vdd0v8_ddr = DEFAULT_PERCENTAGE_THRESHOLD
    reserved1 = 0
    reserved2 = 0

    arr = np.asarray([humidity_threshold_sht4x, voltage_thresholds_vdd3v3, voltage_thresholds_vdd1v8,
                     voltage_thresholds_vdd1v2, voltage_thresholds_vdd1v1, voltage_thresholds_vdd0v8,
                     voltage_thresholds_vdd0v6, voltage_thresholds_vdd5vo_u, voltage_thresholds_vdd5vo_l,
                     voltage_thresholds_vdd0v8_ddr, reserved1, reserved2], dtype=np.uint8)

    return arr


def generate_dev_mode_features():
    hkr_developer_mode = 0
    smcu_developer_mode = 0
    hkr_developer_mode_simulated_lock_state = 0
    sc_developer_mode = 0

    arr = np.asarray([hkr_developer_mode, smcu_developer_mode,
                      hkr_developer_mode_simulated_lock_state, sc_developer_mode], dtype=np.uint8)

    return arr


def generate_depth_sign_features():
    depth_pipeline_config = 0
    depth_roi = 0
    ir_for_sip = 0
    peripherals_sensors_disable_mask = to_np_array(0, 2)
    reserved3 = to_np_array(0, 265)
    digital_signature = to_np_array(0, 32)

    ds_1 = np.asarray([depth_pipeline_config, depth_roi, ir_for_sip], dtype=np.uint8)
    ds_2 = np.hstack((ds_1, peripherals_sensors_disable_mask))
    ds_3 = np.hstack((ds_2, reserved3))
    ds_4 = np.hstack((ds_3, digital_signature))

    return ds_4


def generate_app_config_table():
    sip_np_arr = generate_sip_part()
    dev_rules_selection_bitmap = generate_dev_rules_selection_bitmap()
    features_1_4_np_arr = generate_features_1_4()
    features_temp_thresholds_np_arr = generate_temp_thresholds()
    features_humid_volt_thresholds_np_arr = generate_humid_volt_thresholds()
    features_dev_mode_np_arr = generate_dev_mode_features()
    depth_sign_features_np_arr = generate_depth_sign_features()

    app_cfg_1 = np.hstack((sip_np_arr, dev_rules_selection_bitmap))
    app_cfg_2 = np.hstack((app_cfg_1, features_1_4_np_arr))
    app_cfg_3 = np.hstack((app_cfg_2, features_temp_thresholds_np_arr))
    app_cfg_4 = np.hstack((app_cfg_3, features_humid_volt_thresholds_np_arr))
    app_cfg_5 = np.hstack((app_cfg_4, features_dev_mode_np_arr))
    app_cfg_6 = np.hstack((app_cfg_5, depth_sign_features_np_arr))

    return app_cfg_6


def set_app_config_table(app_config_table):
    set_table_opcode = 0xa6
    flash_mem_enum = 1
    app_config_id = 0xc0de
    d500_dynamic = 0
    version_array = np.array([1, 0])
    header = build_header(app_config_table, app_config_id, version_array)
    table_with_header = np.hstack((header, app_config_table))
    cmd = hwm_dev.build_command(opcode=set_table_opcode,
                                param1=flash_mem_enum,
                                param2=app_config_id,
                                param3=d500_dynamic,
                                data=table_with_header)
    ans = hwm_dev.send_and_receive_raw_data(cmd)
    opcode = ans[0]
    test.check_equal(opcode, set_table_opcode)
    return ans


def get_app_config_table():
    get_table_opcode = 0xa7
    flash_mem_enum = 1
    app_config_id = 0xc0de
    d500_dynamic = 0
    cmd = hwm_dev.build_command(opcode=get_table_opcode,
                                param1=flash_mem_enum,
                                param2=app_config_id,
                                param3=d500_dynamic)
    ans = hwm_dev.send_and_receive_raw_data(cmd)
    opcode = ans[0]
    test.check_equal(opcode, get_table_opcode)
    # slicing:
    # the 4 first bytes - opcode
    # the 16 following bytes = header
    ans_data = ans[20:]
    return ans_data


#############################################################################################
# Tests
#############################################################################################

dev, _ = test.find_first_device_or_exit()
hwm_dev = dev.as_debug_protocol()
tw.start_wrapper(dev)

#############################################################################################
test.start("get app config table")
orig_app_config_table = get_app_config_table()
log.d("app config successfully downloaded")
test.finish()

#############################################################################################
test.start("set app config table, and check its writing")
generated_app_config_table_array = generate_app_config_table()
ans = set_app_config_table(generated_app_config_table_array)
log.d("app config successfully uploaded")
# checking the app config table now in device is equal to the one uploaded
curr_config_table = get_app_config_table()
test.check_equal(curr_config_table, generated_app_config_table_array.tolist())
test.finish()

#############################################################################################
test.start("restoring config table")
app_config_table = set_app_config_table(np.asarray(orig_app_config_table, dtype=np.uint8))
log.d("app config successfully uploaded")
# checking the app config table now in device is equal to the one uploaded
curr_config_table = get_app_config_table()
test.check_equal(curr_config_table, orig_app_config_table)
test.finish()

#############################################################################################
tw.stop_wrapper(dev)
test.print_results_and_exit()