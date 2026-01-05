# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

import sys
import time
import copy
import pyrealsense2 as rs
from rspy import test, log
import struct
import zlib
import ctypes

# Constants for calibration
CALIBRATION_TIMEOUT_SECONDS = 90  # Increased from 30 to allow for longer calibration processes
OCC_TIMEOUT_MS = 30000  # Increased from 9000 to 30000ms to match working examples
TARE_TIMEOUT_MS = 10000
FRAME_PROCESSING_TIMEOUT_MS = 5000
HARDWARE_RESET_DELAY_SECONDS = 3

# Global variable to store original calibration table
_global_original_calib_table = None

def on_calib_cb(progress):
    """Callback function for calibration progress reporting."""
    pp = int(progress)
    log.d( f"Calibration at {progress}%" )

def get_calibration_device(image_width, image_height, fps):
    """
    Setup and configure the calibration device.
    
    Args:
        image_width (int): Image width
        image_height (int): Image height
        fps (int): Frames per second
        
    Returns:
        tuple: (pipeline, auto_calibrated_device)
    """
    config = rs.config()
    pipeline = rs.pipeline()
    pipeline_wrapper = rs.pipeline_wrapper(pipeline)
    config.enable_stream(rs.stream.depth, image_width, image_height, rs.format.z16, fps)            

    pipeline_profile = config.resolve(pipeline_wrapper)
    auto_calibrated_device = rs.auto_calibrated_device(pipeline_profile.get_device())
    if not auto_calibrated_device:
        raise RuntimeError("Failed to open auto_calibrated_device for calibration")
    
    return config, pipeline, auto_calibrated_device

def calibration_main(config, pipeline, calib_dev, occ_calib, json_config, ground_truth, return_table=False):
    """
    Main calibration function for both OCC and Tare calibrations.
    
    Args:
        host_assistance (bool): Whether to use host assistance mode
        occ_calib (bool): True for OCC calibration, False for Tare calibration
        json_config (str): JSON configuration string
        ground_truth (float): Ground truth value for Tare calibration (None for OCC)
    
    Returns:
        float: Health factor from calibration
    """

    conf = pipeline.start(config)
    pipeline.wait_for_frames()  # Verify streaming started before calling calibration methods
    camera_name = conf.get_device().get_info(rs.camera_info.name)
    emitter_required = True
    if camera_name == "Intel RealSense D415":
        emitter_required = False
    if emitter_required:
        depth_sensor = conf.get_device().first_depth_sensor()
        if depth_sensor.supports(rs.option.emitter_enabled):
            depth_sensor.set_option(rs.option.emitter_enabled, 1)    
    if depth_sensor.supports(rs.option.thermal_compensation):
        depth_sensor.set_option(rs.option.thermal_compensation, 0)

    new_calib_result = b''
    # Execute calibration based on type
    try:
        if occ_calib:    
            log.i("Starting on-chip calibration")
            new_calib, health = calib_dev.run_on_chip_calibration(json_config, on_calib_cb, OCC_TIMEOUT_MS)
        else:    
            log.i("Starting tare calibration")
            new_calib, health = calib_dev.run_tare_calibration(ground_truth, json_config, on_calib_cb, TARE_TIMEOUT_MS)

        calib_done = len(new_calib) > 0
        log.d(f"Initial calibration call completed. Calibration done: {calib_done}, table size: {len(new_calib)}")
        
        if not calib_done:
            log.d("Entering frame processing loop for host-assisted calibration")
            timeout_end = time.time() + CALIBRATION_TIMEOUT_SECONDS
            frame_count = 0
            
            while not calib_done:
                if time.time() > timeout_end:
                    raise RuntimeError("Calibration timed out after {} seconds".format(CALIBRATION_TIMEOUT_SECONDS))
                frame_set = pipeline.wait_for_frames()
                depth_frame = frame_set.get_depth_frame()
                
                # Only process valid depth frames
                if depth_frame:
                    frame_count += 1
                    new_calib, health = calib_dev.process_calibration_frame(depth_frame, on_calib_cb, FRAME_PROCESSING_TIMEOUT_MS)
                    calib_done = len(new_calib) > 0
                    if frame_count % 30 == 0:
                        log.d(f"Processed {frame_count} frames, calibration done: {calib_done}")
        else:
            log.d("Calibration completed in initial call (no frame processing needed)")
        # Preserve final table
        if calib_done:
            new_calib_result = bytes(new_calib)
            
        log.i("Calibration completed successfully")
        log.i("Health factor = ", health[0])
        
    except Exception as e:
        log.e("Calibration failed: ", str(e))
        raise
    finally:
        # Stop pipeline
        pipeline.stop()

    if return_table:
        return health[0], new_calib_result
    return health[0]


def measure_average_depth(config, pipe, width=640, height=480, fps=30, frames=10, timeout_s=12, enable_emitter=True):
    """Measure the average depth distance (in meters) across a series of frames.

    Valid depth pixels are > 0 (raw units). Invalid (<=0) are ignored.

    Args:
        ctx (rs.context|None): Optional existing context; created if None.
        width, height, fps (int): Stream configuration for depth.
        frames (int): Maximum number of frames to sample.
        timeout_s (float): Overall timeout for the sampling loop.
        enable_emitter (bool): Attempt to enable emitter for more reliable data.

    Returns:
        float | None: Mean depth in meters over all valid pixels from collected frames,
                      or None if no valid data was obtained.
    """
    try:
        import numpy as np

        conf = pipe.start(config)
        # Configure sensor options (best-effort)
        try:
            depth_sensor = conf.get_device().first_depth_sensor()
            if enable_emitter and depth_sensor.supports(rs.option.emitter_enabled):
                depth_sensor.set_option(rs.option.emitter_enabled, 1)
            if depth_sensor.supports(rs.option.thermal_compensation):
                depth_sensor.set_option(rs.option.thermal_compensation, 0)
            depth_scale = depth_sensor.get_depth_scale()
        except Exception:
            # Fallback default RealSense depth scale (commonly 0.001) if retrieval fails
            depth_scale = 0.001

        start = time.time()
        collected = 0
        valid_sum_m = 0.0
        valid_count = 0

        while collected < frames and (time.time() - start) < timeout_s:
            try:
                fs = pipe.wait_for_frames(3000)
            except Exception:
                continue
            depth = fs.get_depth_frame()
            if not depth:
                continue
            data = np.asanyarray(depth.get_data())  # uint16
            if data.size == 0:
                continue
            valid_mask = data > 0  # ignore zero / invalid
            count = valid_mask.sum()
            if count == 0:
                # No valid points in this frame; keep trying
                collected += 1
                continue
            # Accumulate sums in meters (raw * scale)
            valid_sum_m += float(data[valid_mask].sum()) * depth_scale
            valid_count += int(count)
            collected += 1
            # Early break heuristic: if we already have plenty of samples and elapsed > 1.5s
            if collected >= 5 and (time.time() - start) > 1.5:
                break

        pipe.stop()
        if valid_count == 0:
            return None
        return valid_sum_m / valid_count
    except Exception as e:
        log.w(f"measure_average_depth failed: {e}")
        try:
            if pipe:
                pipe.stop()
        except Exception:
            pass
        return None


def is_mipi_device():
    ctx = rs.context()
    device = ctx.query_devices()[0]
    return device.supports(rs.camera_info.connection_type) and device.get_info(rs.camera_info.connection_type) == "GMSL"

def is_d555():
    try:
        ctx = rs.context()
        dev = ctx.query_devices()[0]
        name = dev.get_info(rs.camera_info.name) if dev.supports(rs.camera_info.name) else ""
        return ("D555" in name)
    except Exception:
        return False

# for step 2 -  not in use for now

def get_current_rect_params(auto_calib_device):
    """
    Return principal points (ppx, ppy) for left and right sensors derived from the device's calibration table

    ""Return per-eye principal points (ppx, ppy) using intrinsic matrices; minimal logging."""
    calib_table = auto_calib_device.get_calibration_table()
    if calib_table is not None:
        calib_table = bytes(calib_table)
    if len(calib_table) < 280:
        log.e("Calibration table is too small")
        return None
    try:
        #  calibration table is 4 3x3 float matrices (each 9 * 4 bytes) — intrinsics/extrinsics left/right pairs
        header_size = 16
        intrinsic_left_offset = header_size
        intrinsic_right_offset = header_size + 36
        left_intrinsics_raw = struct.unpack('<9f', calib_table[intrinsic_left_offset:intrinsic_left_offset + 36])
        right_intrinsics_raw = struct.unpack('<9f', calib_table[intrinsic_right_offset:intrinsic_right_offset + 36])
        width = 1280.0
        height = 800.0
        left_ppx = left_intrinsics_raw[2] * width
        right_ppx = right_intrinsics_raw[2] * width
        left_ppy = left_intrinsics_raw[3] * height
        right_ppy = right_intrinsics_raw[3] * height
        offsets_dict = {
            'intrinsic_left_offset': intrinsic_left_offset,
            'intrinsic_right_offset': intrinsic_right_offset
        }
        return (left_ppx, left_ppy), (right_ppx, right_ppy), offsets_dict
    except Exception as e:
        log.e(f"Error reading principal points: {e}")
        return None
        
def restore_calibration_table(device):
    global _global_original_calib_table

    # Get the auto calibrated device interface
    auto_calib_device = rs.auto_calibrated_device(device)
    if not auto_calib_device:
        log.e("Device does not support auto calibration")
        return False

    auto_calib_device.reset_to_factory_calibration()
    return True

def get_calibration_table(device):
    """Get current calibration table from device"""
    try:
        # device is already an auto_calibrated_device
        calib_table = device.get_calibration_table()
        # Convert to bytes if it's a list
        if calib_table is not None:
            calib_table = bytes(calib_table)
        return calib_table
    except Exception as e:
        log.e(f"-E- Failed to get calibration table: {e}")
        return None
    
def write_calibration_table_with_crc(device, modified_data):
    """Write modified calibration table with updated CRC"""
    try:
        # Calculate CRC32 for the data after the header (skip first 16 bytes)
        actual_data = modified_data[16:]
        new_crc32 = zlib.crc32(actual_data) & 0xffffffff
        
        # Get old CRC for comparison
        old_crc32 = struct.unpack('<I', modified_data[12:16])[0]
        
        # Update CRC in the header
        final_data = bytearray(modified_data)
        final_data[12:16] = struct.pack('<I', new_crc32)
                
        # Convert to list of ints for the API
        calib_list = list(final_data)
        
        # Write to device - device is already auto_calibrated_device
        device.set_calibration_table(calib_list)
        device.write_calibration()
        
        return True, bytes(final_data)
        
    except Exception as e:
        log.e(f"-E- Error writing calibration table: {e}")
        return False, str(e)

def modify_extrinsic_calibration(device, pixel_correction, modify_ppy=True):
    """Modify either raw right intrinsic ppx or ppy by pixel_correction (in pixels).

    Args:
        device: auto_calibrated_device (rs.auto_calibrated_device)
        pixel_correction (float): delta in pixels to add
        modify_ppy (bool): if True adjust ppy else adjust ppx
    Returns:
        tuple: (success(bool), modified_table_bytes(or error str), new_ppx, new_ppy)
    """
    try:
        calib_table = get_calibration_table(device)
        if not calib_table:
            return False, "Failed to get calibration table", None, None
        modified_data = bytearray(calib_table)
        header_size = 16
        right_intrinsics_offset = header_size + 36  # skip left 9 floats (left eye)
        right_intrinsics = list(struct.unpack('<9f', modified_data[right_intrinsics_offset:right_intrinsics_offset+36]))
        width = 1280.0
        height = 800.0
        original_raw_ppx = right_intrinsics[2] * width
        original_raw_ppy = right_intrinsics[3] * height
        if modify_ppy:
            corrected_raw_ppy = original_raw_ppy + pixel_correction
            right_intrinsics[3] = corrected_raw_ppy / height
            log.i(f"  Raw Right ppy original={original_raw_ppy:.6f} modified={corrected_raw_ppy:.6f}")
        else:
            corrected_raw_ppx = original_raw_ppx + pixel_correction
            right_intrinsics[2] = corrected_raw_ppx / width
            log.i(f"  Raw Right ppx original={original_raw_ppx:.6f} modified={corrected_raw_ppx:.6f}")
        modified_data[right_intrinsics_offset:right_intrinsics_offset+36] = struct.pack('<9f', *right_intrinsics)
        write_ok, modified_table_or_err = write_calibration_table_with_crc(device, bytes(modified_data))
        if not write_ok:
            return False, modified_table_or_err, None, None
        new_ppx = right_intrinsics[2] * width
        new_ppy = right_intrinsics[3] * height
        return True, modified_table_or_err, new_ppx, new_ppy
    except Exception as e:
        log.e(f"Error modifying calibration: {e}")
        return False, str(e), None, None

# ---------------------------------------------------------------------------
# Advanced OCC calibration test helper (moved from test_occ_calibrations.py)
# ---------------------------------------------------------------------------

# Pixel correction constant for DSCalibrationModifier-style corrections
PIXEL_CORRECTION = -1
EPSILON = 0.001  # Small tolerance for floating point precision

# Health factor thresholds
HEALTH_FACTOR_THRESHOLD = 0.3
HEALTH_FACTOR_THRESHOLD_AFTER_MODIFICATION = 0.75

def on_chip_calibration_json(occ_json_file, host_assistance):
    """Return OCC JSON string (default if file not provided)."""
    occ_json = None
    if occ_json_file is not None:
        try:
            occ_json = open(occ_json_file).read()
        except Exception:
            occ_json = None
            log.e('Error reading occ_json_file: ', occ_json_file)
    if occ_json is None:
        log.i('Using default parameters for on-chip calibration.')
        occ_json = '{\n  ' + \
                   '"calib type": 0,\n' + \
                   '"host assistance": ' + str(int(host_assistance)) + ',\n' + \
                   '"keep new value after successful scan": 1,\n' + \
                   '"fl data sampling": 0,\n' + \
                   '"adjust both sides": 0,\n' + \
                   '"fl scan location": 0,\n' + \
                   '"fy scan direction": 0,\n' + \
                   '"white wall mode": 0,\n' + \
                   '"speed": 2,\n' + \
                   '"scan parameter": 0,\n' + \
                   '"apply preset": 0,\n' + \
                   '"scan only": ' + str(int(host_assistance)) + ',\n' + \
                   '"interactive scan": 0,\n' + \
                   '"resize factor": 1\n' + \
                   '}'
    return occ_json
