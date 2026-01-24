# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

import time
import struct
import pyrealsense2 as rs
from rspy import test, log
from test_calibrations_common import (
    calibration_main,
    get_calibration_device,
    get_current_rect_params,
    is_mipi_device,
    modify_extrinsic_calibration,
    save_calibration_table,
    restore_calibration_table,
    write_calibration_table_with_crc,
    measure_average_depth,
    is_d555
)

#disabled until we stabilize lab
#test:device D400*

# Constants & thresholds (reintroduce after import fix)
PIXEL_CORRECTION = -1.0  # pixel shift to apply to principal point
EPSILON = 0.1         # distance comparison tolerance
DIFF_THREDSHOLD = 0.001  # minimum change expected after OCC calibration
HEALTH_FACTOR_THRESHOLD_AFTER_MODIFICATION = 2
DEPTH_CONVERGENCE_TOLERANCE_MM = 50.0  # 5 cm tolerance for depth convergence toward ground truth
def on_chip_calibration_json(occ_json_file, host_assistance):
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
                   '"resize factor": 1\n' + \
                   '}'
    return occ_json

#disabled until we stabilize lab

def run_advanced_occ_calibration_test(host_assistance, config, pipeline, calib_dev, image_width, image_height, fps, modify_ppy=True, ground_truth_mm=None):
    """Run advanced OCC calibration test with calibration table modifications.

        Flow:
        1. Read and log base principal points (reference).
        2. Measure baseline average depth; establish ground truth if not provided.
        3. Apply manual principal-point perturbation (ppx/ppy shift) to the calibration table.
        4. Re-read and verify the modification was applied (delta vs base within tolerance).
        5. Measure average depth after modification (pre-OCC).
        6. Run OCC calibration (host assistance optional); obtain new table & health factor; validate threshold.
        7. Write returned table; read final principal points; compute and log distances to base and modified.
        8. Measure post-OCC average depth; assert convergence toward ground truth and principal point reversion (failure handling if not satisfied).
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

        base_axis_val = base_right_pp[1]

        # 2. Baseline average depth (before perturbation)
        baseline_avg_depth_m = measure_average_depth(config, pipeline, width=image_width, height=image_height, fps=fps)
        if baseline_avg_depth_m is not None:
            baseline_mm = baseline_avg_depth_m * 1000.0
            log.i(f"Baseline average depth (pre-modification): {baseline_mm:.1f} mm")
            # If caller did not supply ground_truth_mm, derive it from baseline
            if ground_truth_mm is None:
                ground_truth_mm = baseline_mm
                log.i(f"Ground truth depth set from baseline: {ground_truth_mm:.1f} mm")
            else:
                log.i(f"Ground truth depth provided: {ground_truth_mm:.1f} mm")
        else:
            log.w("Baseline average depth unavailable; depth convergence assertion will be skipped")
        
        # 3. Apply perturbation
        log.i(f"Applying manual raw intrinsic correction: delta={PIXEL_CORRECTION:+.3f} px")
        modification_success, _modified_table_bytes, modified_ppx, modified_ppy = modify_extrinsic_calibration(
            calib_dev, PIXEL_CORRECTION, modify_ppy=modify_ppy)
        if not modification_success:
            log.e("Failed to modify calibration table")
            test.fail()

        # 4. Verify modification for ppy/ppx was applied
        modified_principal_points_result = get_current_rect_params(calib_dev)
        if modified_principal_points_result is None:
            log.e("Could not read principal points after modification")
            test.fail()
        mod_left_pp, mod_right_pp, mod_offsets = modified_principal_points_result
        modified_axis_val = mod_right_pp[1]
        returned_modified_axis_val = modified_ppy if modify_ppy else modified_ppx
        if abs(modified_axis_val - returned_modified_axis_val) > EPSILON:
            log.e(f"Modification mismatch for ppy. Expected {returned_modified_axis_val:.6f} got {modified_axis_val:.6f}")
            test.fail()

        # 5. Measure average depth after modification, before OCC correction (modified baseline)
        modified_avg_depth_m = measure_average_depth(config, pipeline, width=image_width, height=image_height, fps=fps)
        if modified_avg_depth_m is not None:
            log.i(f"Average depth after modification (pre-OCC): {modified_avg_depth_m*1000:.1f} mm")
        else:
            log.e("Average depth after modification unavailable")
            test.fail()

        # 6. Run OCC
        iterations = 1
        depth_check_failed = True
        while (depth_check_failed and iterations > 0):
            iterations -= 1
            occ_json = on_chip_calibration_json(None, host_assistance)
            new_calib_bytes = None
            try:
                health_factor, new_calib_bytes = calibration_main(config, pipeline, calib_dev, True, occ_json, None, return_table=True)
            except Exception as e:
                log.e(f"Calibration_main failed: {e}")
                health_factor = None

            if not (new_calib_bytes and health_factor is not None and abs(health_factor) < HEALTH_FACTOR_THRESHOLD_AFTER_MODIFICATION):
                log.e(f"OCC calibration failed or health factor out of threshold (hf={health_factor})")
                test.fail()
            log.i(f"OCC calibration completed (health factor={health_factor:+.4f})")

            # 7 Write updated table & evaluate
            write_ok, _ = write_calibration_table_with_crc(calib_dev, new_calib_bytes)
            if not write_ok:
                log.e("Failed to write OCC calibration table to device")
                test.fail()

            final_principal_points_result = get_current_rect_params(calib_dev)
            if final_principal_points_result is None:
                log.e("Could not read final principal points")
                test.fail()

            fin_left_pp, fin_right_pp, fin_offsets = final_principal_points_result
            final_axis_val = fin_right_pp[1]
            log.i(f"  Final principal points (Right) ppx={fin_right_pp[0]:.6f} ppy={fin_right_pp[1]:.6f}")

            # 8. Reversion checks:
                # a. Final must differ from modified (change happened)
                # b. Final must be closer to base than to modified (strict revert expectation)
            dist_from_original = abs(final_axis_val - base_axis_val)
            dist_from_modified = abs(final_axis_val - modified_axis_val)
            log.i(f"  ppy distances: from_base={dist_from_original:.6f} from_modified={dist_from_modified:.6f}")

            # Measure average depth after OCC correction
            post_avg_depth_m = measure_average_depth(config, pipeline, width=image_width, height=image_height, fps=fps)
            if post_avg_depth_m is not None:
                log.i(f"Average depth after OCC: {post_avg_depth_m*1000:.1f} mm")
            else:
                log.e("Average depth after OCC unavailable")
                test.fail()

            # Depth convergence assertion relative to ground truth: ensure post depth is closer to ground truth than modified depth
            if (ground_truth_mm is not None and
                modified_avg_depth_m is not None and
                post_avg_depth_m is not None):
                dist_modified_gt_mm = abs(modified_avg_depth_m * 1000.0 - ground_truth_mm)
                dist_post_gt_mm = abs(post_avg_depth_m * 1000.0 - ground_truth_mm)
                log.i(f"Depth to ground truth (mm): modified={dist_modified_gt_mm:.1f} post={dist_post_gt_mm:.1f} (ground truth={ground_truth_mm:.1f} mm)")
                # verify convergence
                if dist_post_gt_mm > dist_modified_gt_mm + DEPTH_CONVERGENCE_TOLERANCE_MM:
                    log.e("Post-calibration average depth did not converge toward ground truth")
                    depth_check_failed = True
                #    restore_calibration_table(calib_dev, None)
                #    if iterations == 0:
                #        test.fail()
                else:
                    improvement = dist_modified_gt_mm - dist_post_gt_mm
                    log.i(f"Post-calibration average depth converged toward ground truth (improvement={improvement:.1f} mm)")
                    depth_check_failed = False

        if abs(final_axis_val - modified_axis_val) <= DIFF_THREDSHOLD:
            log.e(f"OCC left ppy unchanged (within EPSILON={DIFF_THREDSHOLD}); failing")            
            test.fail()
        elif dist_from_modified + EPSILON <= dist_from_original:
            log.e("OCC did not revert toward base (still closer to modified)")
            test.fail()
        else:
            log.i("OCC reverted ppy toward base successfully")
    except Exception as e:
        log.e("OCC calibration failed: ", str(e))
        test.fail()

    return calib_dev, saved_table

if not is_mipi_device() and not is_d555():
    # mipi devices do not support OCC calibration without host assistance; D555 excluded separately
    # D555 needs different parsing of calibration tables , SRC and more
    with test.closure("Advanced OCC calibration test with calibration table modifications"):
        calib_dev = None
        config = None
        pipeline = None
        try:
            host_assistance = False
            image_width, image_height, fps = (256, 144, 90)
            ground_truth_mm = None  # Example ground-truth depth (mm); adjust if known
            config, pipeline, calib_dev = get_calibration_device(image_width, image_height, fps)
            restore_calibration_table(calib_dev, None)
            calib_dev, saved_table = run_advanced_occ_calibration_test(host_assistance, config, pipeline, calib_dev, image_width, image_height, fps, modify_ppy=True, ground_truth_mm=ground_truth_mm)
        except Exception as e:
            log.e("OCC calibration with principal point modification failed: ", str(e))
            log.i("Restoring calibration table after test failure")
            restore_calibration_table(calib_dev, None)
            test.fail()
        finally:
            restore_calibration_table(calib_dev, None)
"""
temprorary disabled on mipi devices to stabilize the lab
if is_mipi_device() and not is_d555():
    with test.closure("Advanced OCC calibration test with host assistance"):
        calib_dev = None
        config = None
        pipeline = None
        try:
            host_assistance = True
            image_width, image_height, fps = (1280, 720, 30)
            ground_truth_mm = None  # Example ground-truth depth (mm); adjust if known
            config, pipeline, calib_dev = get_calibration_device(image_width, image_height, fps)
            restore_calibration_table(calib_dev, None)
            calib_dev, saved_table = run_advanced_occ_calibration_test(host_assistance, config, pipeline, calib_dev, image_width, image_height, fps, modify_ppy=True, ground_truth_mm=ground_truth_mm)
        except Exception as e:
            log.e("OCC calibration with principal point modification failed: ", str(e))
            log.i("Restoring calibration table after test failure")
            restore_calibration_table(calib_dev, None)
            test.fail()
        finally:
            restore_calibration_table(calib_dev, None)
"""
test.print_results_and_exit()

"""
OCC in Host Assistance mode is allowing to run on any resolution selected by the user.

For example see the attached video - running in 848x100 res.

manual exposure
"""