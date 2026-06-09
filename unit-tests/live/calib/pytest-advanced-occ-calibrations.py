# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import time
import struct
import pytest
import pyrealsense2 as rs
import logging
from calibrations_common import (
    calibration_main,
    get_calibration_device,
    get_current_rect_params,
    is_mipi_device,
    modify_intrinsic_calibration,
    save_calibration_table,
    restore_calibration_table,
    write_calibration_table_with_crc,
    measure_depth_fill_rate,
    is_d555
)

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.context("nightly"),
    pytest.mark.device("D400*"),
    pytest.mark.device_exclude("D401"),
    pytest.mark.device_exclude("D555"),
]

# Constants & thresholds (reintroduce after import fix)
PIXEL_CORRECTION = -1.0  # pixel shift to apply to principal point
SHORT_DISTANCE_PIXEL_CORRECTION = -3.0
EPSILON = 0.5         # half of PIXEL_CORRECTION tolerance
DIFF_THRESHOLD = 0.001  # minimum change expected after OCC calibration
HEALTH_FACTOR_THRESHOLD_AFTER_MODIFICATION = 3.0

def on_chip_calibration_json(occ_json_file, host_assistance):
    occ_json = None
    if occ_json_file is not None:
        try:
            occ_json = open(occ_json_file).read()
        except Exception:
            occ_json = None
            log.error(f'Error reading occ_json_file: {occ_json_file}')
    if occ_json is None:
        log.info('Using default parameters for on-chip calibration.')
        occ_json = '{\n  ' + \
                   '"calib type": 0,\n' + \
                   '"host assistance": ' + str(int(host_assistance)) + ',\n' + \
                   '"speed": 2,\n' + \
                   '"average step count": 20,\n' + \
                   '"scan parameter": 0,\n' + \
                   '"step count": 20,\n' + \
                   '"apply preset": 1,\n' + \
                   '"accuracy": 2,\n' + \
                   '"scan only": ' + str(int(host_assistance)) + ',\n' + \
                   '"interactive scan": 0,\n' + \
                   '"resize factor": 1\n' + \
                   '}'
    return occ_json

def run_advanced_occ_calibration_test(host_assistance, config, pipeline, calib_dev, image_width, image_height, fps, modify_ppy=True):
    """Run advanced OCC calibration test with calibration table modifications.

        Flow:
        1. Read and log base principal points (reference).
        2. Apply manual principal-point perturbation (ppx/ppy shift) to the calibration table.
        3. Re-read and verify the modification was applied (delta vs base within tolerance).
        4. Measure fill rate after modification (pre-OCC).
        5. Run OCC calibration (host assistance optional); obtain new table & health factor; validate threshold.
        6. Write returned table; read final principal points; compute and log distances to base and modified.
        7. Measure post-OCC fill rate; assert it is higher than the modified fill rate and principal point reversion (failure handling if not satisfied).
    """
    try:

        # 0. Save original calibration table
        saved_table = save_calibration_table(calib_dev)
        if saved_table is None:
            log.error("Failed to save original calibration table")
            pytest.fail()
            
        # 1. Read base (reference) principal points
        principal_points_result = get_current_rect_params(calib_dev)
        if principal_points_result is None:
            log.error("Could not read current principal points")
            pytest.fail()
        base_left_pp, base_right_pp, base_offsets = principal_points_result
        log.info(f"  Base principal points (Right) ppx={base_right_pp[0]:.6f} ppy={base_right_pp[1]:.6f}")

        base_axis_val = base_right_pp[1]

        # 2. Apply perturbation
        pixel_correction = PIXEL_CORRECTION
        log.info(f"Applying manual raw intrinsic correction: delta={pixel_correction:+.3f} px")
        modification_success, _modified_table_bytes, modified_ppx, modified_ppy = modify_intrinsic_calibration(
            calib_dev, pixel_correction, modify_ppy=modify_ppy)
        if not modification_success:
            log.error("Failed to modify calibration table")
            pytest.fail()

        # 4. Verify modification for ppy/ppx was applied
        modified_principal_points_result = get_current_rect_params(calib_dev)
        if modified_principal_points_result is None:
            log.error("Could not read principal points after modification")
            pytest.fail()
        mod_left_pp, mod_right_pp, mod_offsets = modified_principal_points_result
        modified_axis_val = mod_right_pp[1]
        returned_modified_axis_val = modified_ppy if modify_ppy else modified_ppx
        if abs(modified_axis_val - returned_modified_axis_val) > DIFF_THRESHOLD:
            log.error(f"Modification mismatch for ppy. Expected {returned_modified_axis_val:.6f} got {modified_axis_val:.6f}")
            pytest.fail()

        # 5. Measure fill rate after modification, before OCC correction (modified baseline)
        modified_fill_rate = measure_depth_fill_rate(config, pipeline, width=image_width, height=image_height, fps=fps)
        if modified_fill_rate is not None:
            log.info(f"Fill rate after modification (pre-OCC): {modified_fill_rate*100:.1f}%")
        else:
            log.error("Fill rate after modification unavailable")
            pytest.fail()
        
        # 6. Run OCC
        occ_json = on_chip_calibration_json(None, host_assistance)
        new_calib_bytes = None
        try:
            health_factor, new_calib_bytes = calibration_main(config, pipeline, calib_dev, True, occ_json, None, host_assistance, return_table=True)
        except Exception as e:
            log.error(f"Calibration_main failed: {e}")
            health_factor = None

        if not (new_calib_bytes and health_factor is not None and abs(health_factor) < HEALTH_FACTOR_THRESHOLD_AFTER_MODIFICATION):
            log.error(f"OCC calibration failed or health factor out of threshold (hf={health_factor})")
            pytest.fail()
        log.info(f"OCC calibration completed (health factor={health_factor:+.4f})")

        # 7 Write updated table & evaluate
        write_ok, _ = write_calibration_table_with_crc(calib_dev, new_calib_bytes)
        if not write_ok:
            log.error("Failed to write OCC calibration table to device")
            pytest.fail()

        # Allow time for device to apply the new calibration table
        time.sleep(1.0)

        final_principal_points_result = get_current_rect_params(calib_dev)
        if final_principal_points_result is None:
            log.error("Could not read final principal points")
            pytest.fail()

        fin_left_pp, fin_right_pp, fin_offsets = final_principal_points_result
        final_axis_val = fin_right_pp[1]
        log.info(f"  Final principal points (Right) ppx={fin_right_pp[0]:.6f} ppy={fin_right_pp[1]:.6f}")

        # 8. Reversion checks:
            # a. Final must differ from modified (change happened)
            # b. Final must be closer to base than to modified (strict revert expectation)
        dist_from_original = abs(final_axis_val - base_axis_val)
        dist_from_modified = abs(final_axis_val - modified_axis_val)
        log.info(f"  ppy distances: from_base={dist_from_original:.6f} from_modified={dist_from_modified:.6f}")

        # Measure depth fill rate after OCC correction
        post_fill_rate = measure_depth_fill_rate(config, pipeline, width=image_width, height=image_height, fps=fps)
        if post_fill_rate is not None:
            log.info(f"Fill rate after OCC: {post_fill_rate*100:.1f}%")
        else:
            log.error("Fill rate after OCC unavailable")
            pytest.fail()

        # Fill rate assertion: post-OCC fill rate must be higher than the perturbed fill rate
        if modified_fill_rate is not None and post_fill_rate is not None:
            log.info(f"Fill rates: modified={modified_fill_rate*100:.1f}% post={post_fill_rate*100:.1f}%")
            if post_fill_rate <= modified_fill_rate:
                log.error("Post-OCC fill rate did not improve over perturbed fill rate")
                pytest.fail()
            else:
                log.info(f"Fill rate improved after OCC (improvement={( post_fill_rate - modified_fill_rate)*100:.1f}%)")

        if abs(final_axis_val - modified_axis_val) <= DIFF_THRESHOLD:
            log.error(f"OCC left ppy unchanged (within DIFF_THRESHOLD={DIFF_THRESHOLD}); failing")            
            pytest.fail()
        elif dist_from_modified + EPSILON <= dist_from_original:
            log.error("OCC did not revert toward base (still closer to modified)")
            pytest.fail()
        else:
            log.info("OCC reverted ppy toward base successfully")
    except Exception as e:
        log.error(f"OCC calibration failed: {e}")
        pytest.fail()

    return calib_dev, saved_table

def test_advanced_occ_calibration(test_device):
    dev, _ = test_device
    # mipi devices do not support OCC calibration without host assistance; D555 excluded separately
    # (D555 needs different parsing of calibration tables, SRC and more).
    if is_mipi_device(dev) or is_d555(dev):
        pytest.skip("Non-mipi non-D555 only — see test_advanced_occ_calibration_with_host_assistance for mipi")

    calib_dev = None
    config = None
    pipeline = None
    try:
        host_assistance = False
        image_width, image_height, fps = (256, 144, 90)
        config, pipeline, calib_dev = get_calibration_device(image_width, image_height, fps, dev=dev)
        restore_calibration_table(calib_dev, None)
        calib_dev, saved_table = run_advanced_occ_calibration_test(host_assistance, config, pipeline, calib_dev, image_width, image_height, fps, modify_ppy=True)
    except Exception as e:
        log.error(f"OCC calibration with principal point modification failed: {e}")
        raise
    finally:
        if calib_dev is not None:
            log.info("Restoring calibration table")
            restore_calibration_table(calib_dev, None)


def test_advanced_occ_calibration_with_host_assistance(test_device):
    dev, _ = test_device
    if not is_mipi_device(dev) or is_d555(dev):
        pytest.skip("Host-assistance OCC calibration only on mipi/GMSL non-D555 devices")

    calib_dev = None
    config = None
    pipeline = None
    try:
        host_assistance = True
        image_width, image_height, fps = (1280, 720, 30)
        config, pipeline, calib_dev = get_calibration_device(image_width, image_height, fps, dev=dev)
        restore_calibration_table(calib_dev, None)
        calib_dev, saved_table = run_advanced_occ_calibration_test(host_assistance, config, pipeline, calib_dev, image_width, image_height, fps, modify_ppy=True)
    except Exception as e:
        log.error(f"OCC calibration with principal point modification failed: {e}")
        raise
    finally:
        if calib_dev is not None:
            log.info("Restoring calibration table")
            restore_calibration_table(calib_dev, None)

"""
OCC in Host Assistance mode is allowing to run on any resolution selected by the user.

For example see the attached video - running in 848x100 res.

manual exposure
"""