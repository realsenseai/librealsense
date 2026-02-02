# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

# test:device D400*

import pyrealsense2 as rs
from rspy import test, log
import json


# Sample HDR configurations (condensed)
MANUAL_HDR_CONFIG_1 = {"hdr-preset": {"id": "0", "iterations": "0", "items": [
    {"iterations": "2", "controls": {"depth-gain": "16", "depth-exposure": "1000"}},
    {"iterations": "3", "controls": {"depth-gain": "100", "depth-exposure": "5000"}}]}}

MANUAL_HDR_CONFIG_2 = {"hdr-preset": {"id": "0", "iterations": "0", "items": [
    {"iterations": "1", "controls": {"depth-gain": "20", "depth-exposure": "500"}},
    {"iterations": "2", "controls": {"depth-gain": "50", "depth-exposure": "2000"}},
    {"iterations": "1", "controls": {"depth-gain": "120", "depth-exposure": "8000"}}]}}

MANUAL_HDR_CONFIG_3 = {"hdr-preset": {"id": "0", "iterations": "0", "items": [
    {"iterations": "1", "controls": {"depth-gain": "16", "depth-exposure": "100"}},
    {"iterations": "2", "controls": {"depth-gain": "30", "depth-exposure": "1000"}},
    {"iterations": "1", "controls": {"depth-gain": "80", "depth-exposure": "3000"}},
    {"iterations": "2", "controls": {"depth-gain": "150", "depth-exposure": "10000"}}]}}

MANUAL_HDR_CONFIG_4 = {"hdr-preset": {"id": "0", "iterations": "0", "items": [
    {"iterations": "1", "controls": {"depth-gain": "16", "depth-exposure": "1"}},
    {"iterations": "1", "controls": {"depth-gain": "25", "depth-exposure": "10"}},
    {"iterations": "1", "controls": {"depth-gain": "50", "depth-exposure": "100"}},
    {"iterations": "1", "controls": {"depth-gain": "80", "depth-exposure": "500"}},
    {"iterations": "1", "controls": {"depth-gain": "120", "depth-exposure": "2000"}},
    {"iterations": "1", "controls": {"depth-gain": "200", "depth-exposure": "8000"}}]}}

AUTO_HDR_CONFIG_1 = {"hdr-preset": {"id": "0", "iterations": "0", "items": [
    {"iterations": "1", "controls": {"depth-ae": "1"}},
    {"iterations": "2", "controls": {"depth-ae-exp": "1000", "depth-ae-gain": "20"}},
    {"iterations": "1", "controls": {"depth-ae-exp": "-1000", "depth-ae-gain": "10"}},
    {"iterations": "2", "controls": {"depth-ae-exp": "2000", "depth-ae-gain": "30"}}]}}

HDR_CONFIGURATIONS = [MANUAL_HDR_CONFIG_1, MANUAL_HDR_CONFIG_2, MANUAL_HDR_CONFIG_3,
                      MANUAL_HDR_CONFIG_4, AUTO_HDR_CONFIG_1]


def to_signed(value):
    """Convert an unsigned integer to a signed integer."""
    value = int(value)
    if value & 0x80000000:
        value -= 0x100000000
    return str(value)


def test_json_load(config, test_title):
    global batch_size
    with test.closure(test_title):
        prior_config = json.loads(am.serialize_json())
        am.load_json(json.dumps(config))  # json dumps just stringify the object
        hdr_config_read = json.loads(am.serialize_json())  # jsonify, note this JSON contains also other keys (params)

        # go over the read data and convert all values to signed integers
        batch_size = 0
        for key in hdr_config_read["hdr-preset"]["items"]:
            batch_size += int(key["iterations"])
            for control_key in key["controls"].keys():
                key["controls"][control_key] = to_signed(key["controls"][control_key])
        test.check(hdr_config_read.get("hdr-preset") == config.get("hdr-preset"))
        test.check(sensor.get_option(rs.option.hdr_enabled) == 1)


def perform_manual_hdr_test(hdr_config, test_title, resolution=(640, 480)):
    with test.closure(test_title):
        expected_iterations = {}
        for idx, item in enumerate(hdr_config["hdr-preset"]["items"]):
            expected_iterations[idx] = int(item["iterations"])

        cfg = rs.config()
        cfg.enable_stream(rs.stream.depth, resolution[0], resolution[1], rs.format.z16, 30)
        seq_id_counts = {}
        pipe.start(cfg)

        log.d(f"Batch size: {batch_size}")
        for i in range(0, batch_size * 5):
            data = pipe.wait_for_frames()
            depth_frame = data.get_depth_frame()
            frame_number = depth_frame.get_frame_number()

            if i < batch_size:  # skip the first batch of frames
                continue

            seq_id = depth_frame.get_frame_metadata(rs.frame_metadata_value.sequence_id)
            frame_exposure = depth_frame.get_frame_metadata(rs.frame_metadata_value.actual_exposure)
            frame_gain = depth_frame.get_frame_metadata(rs.frame_metadata_value.gain_level)
            seq_size = depth_frame.get_frame_metadata(rs.frame_metadata_value.sequence_size)

            seq_id_counts[seq_id] = seq_id_counts.get(seq_id, 0) + 1
            log.d(f"Frame {frame_number} - Sequence ID: {seq_id}, Exposure: {frame_exposure}, Gain: {frame_gain}")

            current_controls = hdr_config["hdr-preset"]["items"][seq_id]["controls"]
            expected_exposure = int(current_controls["depth-exposure"])
            expected_gain = int(current_controls["depth-gain"])

            test.check(frame_exposure == expected_exposure,
                       f"Exposure - Expected: {expected_exposure}, Actual: {frame_exposure}")
            test.check(frame_gain == expected_gain, f"Gain - Expected: {expected_gain}, Actual: {frame_gain}")
            test.check(seq_size == len(hdr_config["hdr-preset"]["items"]))

            if i % batch_size == batch_size - 1:
                test.check(seq_id_counts == expected_iterations,
                           f"Sequence ID counts do not match expected: {seq_id_counts} != {expected_iterations}")
                seq_id_counts.clear()

                log.d("----")

        pipe.stop()


def perform_auto_hdr_test(hdr_config, test_title, resolution=(640, 480)):
    with test.closure(test_title):
        seq_id_0_exp = 0
        seq_id_0_gain = 0
        # after loading config for Auto mode, we should have AE enabled and be in RS2_DEPTH_AUTO_EXPOSURE_ACCELERATED mode
        test.check(sensor.get_option(rs.option.enable_auto_exposure) == 1)
        if sensor.supports(rs.option.auto_exposure_mode):
            test.check(sensor.get_option(rs.option.auto_exposure_mode) == 1)

        expected_iterations = {}
        for idx, item in enumerate(hdr_config["hdr-preset"]["items"]):
            expected_iterations[idx] = int(item["iterations"])

        seq_id_counts = {}
        cfg = rs.config()
        cfg.enable_stream(rs.stream.depth, resolution[0], resolution[1], rs.format.z16, 30)
        pipe.start(cfg)
        log.d(f"Batch size: {batch_size}")
        valid_batch = True
        had_any_valid_batch = False # initially, the AE calculated on seq id 0, can cause expected_exposure < 1, but we expect stabilization
        i = 0
        while i < batch_size * 5:
            data = pipe.wait_for_frames()
            depth_frame = data.get_depth_frame()
            frame_number = depth_frame.get_frame_number()

            seq_id = depth_frame.get_frame_metadata(rs.frame_metadata_value.sequence_id)
            frame_exposure = depth_frame.get_frame_metadata(rs.frame_metadata_value.actual_exposure)
            frame_gain = depth_frame.get_frame_metadata(rs.frame_metadata_value.gain_level)
            seq_size = depth_frame.get_frame_metadata(rs.frame_metadata_value.sequence_size)

            if i < batch_size:
                i += 1
                continue
            elif seq_id_0_exp == 0 and seq_id_0_gain == 0 and seq_id != 0:
                # skip until we see seq_id 0
                continue

            seq_id_counts[seq_id] = seq_id_counts.get(seq_id, 0) + 1
            log.d(f"Frame {frame_number} - Sequence ID: {seq_id}, Exposure: {frame_exposure}, Gain: {frame_gain}")
            if seq_id == 0:
                seq_id_0_exp = frame_exposure
                seq_id_0_gain = frame_gain
                i += 1
                continue

            current_controls = hdr_config["hdr-preset"]["items"][seq_id]["controls"]
            expected_exposure = seq_id_0_exp + int(current_controls["depth-ae-exp"])
            expected_gain = seq_id_0_gain + int(current_controls["depth-ae-gain"])

            if expected_exposure < 1:
                expected_exposure = 1 # if AE didn't stabilize, we can get exposure values where seq_id_0_exp + delta < 1
                valid_batch = False

            test.check(frame_exposure == expected_exposure,
                       f"Exposure - Expected: {expected_exposure}, Actual: {frame_exposure}")
            test.check(frame_gain == expected_gain, f"Gain - Expected: {expected_gain}, Actual: {frame_gain}")
            test.check(seq_size == len(hdr_config["hdr-preset"]["items"]))

            if i % batch_size == batch_size - 1:
                test.check(seq_id_counts == expected_iterations,
                           f"Sequence ID counts do not match expected: {seq_id_counts} != {expected_iterations}")
                seq_id_counts.clear()
                if valid_batch:
                    had_any_valid_batch = True
                valid_batch = True

                log.d("----")
            i += 1

        test.check(had_any_valid_batch, "No valid batch was captured where all expected exposures were >= 1")

        pipe.stop()


def load_and_perform_test(hdr_config, test_title, resolution=(640, 480)):
    """
    Load the HDR configuration and perform the HDR test.
    """
    test_json_load(hdr_config, test_title + " - Load JSON")
    is_auto = False
    for key in hdr_config["hdr-preset"]["items"][0]["controls"].keys():
        if key == "depth-ae":
            is_auto = True
            break

    if not is_auto:
        perform_manual_hdr_test(hdr_config, test_title, resolution)
    else:
        perform_auto_hdr_test(hdr_config, test_title, resolution)


device, ctx = test.find_first_device_or_exit()
am = rs.rs400_advanced_mode(device)
sensor = device.first_depth_sensor()
batch_size = 0  # updated on test_json_load
pipe = rs.pipeline(ctx)