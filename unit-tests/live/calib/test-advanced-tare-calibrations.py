# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.
                                                          ##
import sys
import time
import pyrealsense2 as rs
from rspy import test, log
from test_calibrations_common import (
    calibration_main,
    is_mipi_device,
    get_calibration_device,
    get_current_rect_params,
    modify_extrinsic_calibration,
    save_calibration_table,
    restore_calibration_table,
    write_calibration_table_with_crc,
    measure_average_depth,
    is_d555
)

#disabled until we stabilize lab
#test:donotrun

def tare_calibration_json(tare_json_file, host_assistance):
    tare_json = None
    if tare_json_file is not None:
        try:
            tare_json = open(tare_json_file).read()
        except:
            tare_json = None
            log.e('Error reading tare_json_file: ', tare_json_file)
    if tare_json is None:
        log.i('Using default parameters for Tare calibration.')
        tare_json = '{\n  '+\
                    '"host assistance": ' + str(int(host_assistance)) + ',\n'+\
                    '"speed": 3,\n'+\
                    '"scan parameter": 0,\n'+\
                    '"step count": 20,\n'+\
                    '"apply preset": 1,\n'+\
                    '"accuracy": 2,\n'+\
                    '"depth": 0,\n'+\
                    '"resize factor": 1\n'+\
                    '}'
    return tare_json


def calculate_target_z():
    number_of_images = 50  # The required number of frames is 10+
    timeout_s = 30
    target_size = [175, 100]

    cfg = rs.config()
    cfg.enable_stream(rs.stream.infrared, 1, 1280, 720, rs.format.y8, 30)

    q = rs.frame_queue(capacity=number_of_images, keep_frames=True)
    q2 = rs.frame_queue(capacity=number_of_images, keep_frames=True)
    q3 = rs.frame_queue(capacity=number_of_images, keep_frames=True)

    counter = 0

    def cb(frame):
        nonlocal counter
        if counter > number_of_images:
            return
        for f in frame.as_frameset():
            q.enqueue(f)
            counter += 1

    ctx = rs.context()
    pipe = rs.pipeline(ctx)
    pp = pipe.start(cfg, cb)
    dev = pp.get_device()

    try:
        stime = time.time()
        while counter < number_of_images:
            time.sleep(0.5)
            if timeout_s < (time.time() - stime):
                raise RuntimeError(f"Failed to capture {number_of_images} frames in {timeout_s} seconds, got only {counter} frames")

        adev = dev.as_auto_calibrated_device()
        log.i('Calculating distance to target...')
        log.i(f'\tTarget Size:\t{target_size}')
        target_z = adev.calculate_target_z(q, q2, q3, target_size[0], target_size[1])
        log.i(f'Calculated distance to target is {target_z}')
    finally:
        pipe.stop()

    return target_z


# Constants for validation
HEALTH_FACTOR_THRESHOLD = 0.25
TARGET_Z_MIN = 600
TARGET_Z_MAX = 1500
_target_z = None

# Additional constants & thresholds for advanced calibration modification test
PIXEL_CORRECTION = -0.8  # pixel shift to apply to principal point (right IR)
SHORT_DISTANCE_PIXEL_CORRECTION = -3.0  # increased correction for short distances
EPSILON = 0.5         # half of PIXEL_CORRECTION tolerance
DIFF_THRESHOLD = 0.001  # minimum change expected after TARE calibration
HEALTH_FACTOR_THRESHOLD_AFTER_MODIFICATION = 2.0  # TARE health factor acceptance for modified run


def run_advanced_tare_calibration_test(host_assistance, config, pipeline, calib_dev, image_width, image_height, fps, target_z=None):
    """Run advanced tare calibration test with calibration table modifications.

    Flow:
        1. Read and log base principal points (reference).
        2. Measure baseline average depth; establish ground truth if not provided.
        3. Apply manual principal-point perturbation (ppx/ppy shift) to the calibration table.
        4. Re-read and verify the modification was applied (delta vs base within tolerance).
        5. Measure average depth after modification (pre-Tare).
        6. Run Tare calibration (host assistance optional); obtain new table & health factor; validate threshold.
        7. Write returned table; read final principal points; compute and log distances to base and modified.
        8. Measure post-Tare average depth; assert convergence toward ground truth and principal point reversion (failure handling if not satisfied).
    """
    try:
        # 0. Save original calibration table
        saved_table = save_calibration_table(calib_dev)
        if saved_table is None:
            log.e("Failed to save original calibration table")
            test.fail()

        # 1. Read base (reference) principal points
        principal_points_result = get_current_rect_params(calib_dev)
        if principal_points_result is None:
            log.e("Could not read current principal points")
            test.fail()
        base_left_pp, base_right_pp, base_offsets = principal_points_result
        log.i(f"  Base principal points (Right) ppx={base_right_pp[0]:.6f} ppy={base_right_pp[1]:.6f}")

        base_axis_val = base_right_pp[0]        

        # 3. Apply perturbation
        pixel_correction = PIXEL_CORRECTION
        if target_z < 1300.0:
            pixel_correction = SHORT_DISTANCE_PIXEL_CORRECTION
        log.i(f"Applying manual raw intrinsic correction: delta={pixel_correction:+.3f} px")
        modification_success, _modified_table_bytes, modified_ppx, modified_ppy = modify_extrinsic_calibration(
            calib_dev, pixel_correction, False)
        if not modification_success:
            log.e("Failed to modify calibration table")
            test.fail()

        # 4. Verify modification
        modified_principal_points_result = get_current_rect_params(calib_dev)
        if modified_principal_points_result is None:
            log.e("Could not read principal points after modification")
            test.fail()
        mod_left_pp, mod_right_pp, mod_offsets = modified_principal_points_result
        if abs(modified_ppx - mod_right_pp[0]) > DIFF_THRESHOLD:
            log.e(f"Modification mismatch for ppx. Expected {modified_ppx:.6f} got {mod_right_pp[0]:.6f}")
            test.fail()

       # Measure average depth after modification, before tare correction (modified baseline)
        modified_avg_depth_m = measure_average_depth(config, pipeline, width=image_width, height=image_height, fps=fps)
        if modified_avg_depth_m is not None:
            log.i(f"Average depth after modification (pre-tare): {modified_avg_depth_m*1000:.1f} mm")
        else:
            log.e("Average depth after modification unavailable")
            test.fail()

        # 5. Run tare again
        tare_json = tare_calibration_json(None, host_assistance)
        new_calib_bytes = None
        try:
            health_factor, new_calib_bytes = calibration_main(config, pipeline, calib_dev, False, tare_json, target_z, host_assistance, return_table=True)
        except Exception as e:
            log.e(f"Calibration_main failed: {e}")
            health_factor = None

        if not (new_calib_bytes and health_factor is not None and abs(health_factor) < HEALTH_FACTOR_THRESHOLD_AFTER_MODIFICATION):
            log.e(f"tare calibration failed or health factor out of threshold (hf={health_factor})")
            test.fail()
        log.i(f"tare calibration completed (health factor={health_factor:+.4f})")

        # 6. Write updated table & evaluate
        write_ok, _ = write_calibration_table_with_crc(calib_dev, new_calib_bytes)
        if not write_ok:
            log.e("Failed to write tare calibration table to device")
            test.fail()

        final_principal_points_result = get_current_rect_params(calib_dev)
        if final_principal_points_result is None:
            log.e("Could not read final principal points")
            test.fail()
        fin_left_pp, fin_right_pp, fin_offsets = final_principal_points_result
        final_axis_val = fin_right_pp[0]
        log.i(f"  Final principal points (Right) ppx={fin_right_pp[0]:.6f} ppy={fin_right_pp[1]:.6f}")

        # Measure average depth after tare correction
        post_avg_depth_m = measure_average_depth(config, pipeline, width=image_width, height=image_height, fps=fps)
        if post_avg_depth_m is not None:
            log.i(f"Average depth after tare: {post_avg_depth_m*1000:.1f} mm")
        else:
            log.e("Average depth after tare unavailable")
            test.fail()

        # Reversion checks:
        # 1. Final must differ from modified (change happened)
        # 2. Final must be closer to base than to modified (strict revert expectation)
        dist_from_original = abs(final_axis_val - base_axis_val)
        dist_from_modified = abs(final_axis_val - modified_ppx)
        log.i(f"  ppx distances: from_base={dist_from_original:.6f} from_modified={dist_from_modified:.6f}")

        if abs(final_axis_val - modified_ppx) <= DIFF_THRESHOLD:
            log.e(f"tare left ppy unchanged (within DIFF_THRESHOLD={DIFF_THRESHOLD}); failing")
            test.fail()
        elif dist_from_modified + EPSILON <= dist_from_original:
            log.e("tare did not revert toward base (still closer to modified)")
            test.fail()
        else:
            log.i("tare reverted ppy toward base successfully")

        # Measure average depth after tare correction
        # Compare average depths to target_z (in mm). Expect post calibration to be closer.
        if target_z is not None and modified_avg_depth_m is not None and post_avg_depth_m is not None:
            modified_diff_mm = abs(modified_avg_depth_m * 1000.0 - target_z)
            post_diff_mm = abs(post_avg_depth_m * 1000.0 - target_z)
            log.i(f"  Depth distance to target: pre-tare={modified_diff_mm:.2f} mm post-tare={post_diff_mm:.2f} mm (target_z={target_z} mm)")
            if post_diff_mm > modified_diff_mm:
                log.e("Average depth after OCC not closer to target distance")
                test.fail()
            else:
                log.i("Average depth after OCC moved closer to target distance")
    finally:
        # Always stop pipeline before returning device so subsequent tests can reset factory calibration
        try:
            pipeline.stop()
        except Exception:
            pass
    return calib_dev, saved_table


if not is_mipi_device() and not is_d555():
# mipi devices do not support OCC calibration without host assistance
# D555 needs different parsing of calibration tables , SRC and more
    with test.closure("Advanced tare calibration test with calibration table modifications"):
        calib_dev = None
        try:
            host_assistance = False
            if (_target_z is None):
                _target_z = calculate_target_z()
                test.check(_target_z > TARGET_Z_MIN and _target_z < TARGET_Z_MAX)
            image_width, image_height, fps = (256, 144, 90)
            config, pipeline, calib_dev = get_calibration_device(image_width, image_height, fps)
            restore_calibration_table(calib_dev, None)
            calib_dev, saved_table = run_advanced_tare_calibration_test(host_assistance, config, pipeline, calib_dev, image_width, image_height, fps, _target_z)
        except Exception as e:
            log.e("Tare calibration with principal point modification failed: ", str(e))
            restore_calibration_table(calib_dev, None)
            test.fail()
        finally:
            if calib_dev is not None:
                log.i("Restoring calibration table after test failure")
                restore_calibration_table(calib_dev, None)

"""
temporarily disabled on mipi devices to stabilize the lab

if is_mipi_device() and not is_d555():
# mipi devices do not support OCC calibration without host assistance
# D555 needs different parsing of calibration tables , SRC and more
    with test.closure("Advanced tare calibration test with host assistance"):
        calib_dev = None
        try:
            host_assistance = True
            if (_target_z is None):
                _target_z = calculate_target_z()
                test.check(_target_z > TARGET_Z_MIN and _target_z < TARGET_Z_MAX)
            image_width, image_height, fps = (1280, 720, 30)
            config, pipeline, calib_dev = get_calibration_device(image_width, image_height, fps)
            calib_dev, saved_table = run_advanced_tare_calibration_test(host_assistance, config, pipeline, calib_dev, image_width, image_height, fps, _target_z)
        except Exception as e:
            log.e("Tare calibration with principal point modification failed: ", str(e))
            test.fail()
        finally:
            if calib_dev is not None and getattr(test, 'test_failed', True):
                log.i("Restoring calibration table after test failure")
                restore_calibration_table(calib_dev, None)
"""
test.print_results_and_exit()

# for step 2 -  not in use for now
"""
test.print_results_and_exit()
change exposuere 8500 (tried with various exposure values)/ host assisatnece true
"""