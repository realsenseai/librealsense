# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

import sys
import time
import pyrealsense2 as rs
from rspy import test, log
from test_calibrations_common import calibration_main, get_calibration_device, is_mipi_device

#test:device D400*

def on_chip_calibration_json(occ_json_file, host_assistance):
    occ_json = None
    if occ_json_file is not None:
        try:
            occ_json = open(occ_json_file).read()
        except:
            occ_json = None
            log.e('Error reading occ_json_file: ', occ_json_file)
        
    if occ_json is None:
        log.i('Using default parameters for on-chip calibration.')
        occ_json = '{\n  '+\
                    '"calib type": 0,\n'+\
                    '"host assistance": ' + str(int(host_assistance)) + ',\n'+\
                    '"speed": 2,\n'+\
                    '"average step count": 20,\n'+\
                    '"scan parameter": 0,\n'+\
                    '"step count": 20,\n'+\
                    '"apply preset": 1,\n'+\
                    '"accuracy": 2,\n'+\
                    '"scan only": ' + str(int(host_assistance)) + ',\n'+\
                    '"interactive scan": 0,\n'+\
                    '"resize factor": 1\n'+\
                    '}'
    # TODO - host assistance actual value may be different when reading from json
    return occ_json

# Health factor threshold for calibration success
# 1.5 is temporarily W/A for our cameras places in very low position in the lab. the proper value for good calibration is 0.25
HEALTH_FACTOR_THRESHOLD = 1.5
NUM_ITERATIONS = 1
 
if not is_mipi_device():
# mipi devices do not support OCC calibration without host assistance    
    with test.closure(f"OCC calibration test - {NUM_ITERATIONS} iterations"):
        for iteration in range(1, NUM_ITERATIONS + 1):
            try:        
                log.i(f"Starting OCC calibration iteration {iteration}/{NUM_ITERATIONS}")
                host_assistance = False        
                occ_json = on_chip_calibration_json(None, host_assistance)
                image_width, image_height, fps = 256, 144, 90
                config, pipeline, calib_dev = get_calibration_device(image_width, image_height, fps)
                health_factor, new_calib_bytes = calibration_main(config, pipeline, calib_dev, True, occ_json, None, return_table=True)
                test.check(abs(health_factor) < HEALTH_FACTOR_THRESHOLD or new_calib_bytes is None)
                log.i(f"Completed OCC calibration iteration {iteration}/{NUM_ITERATIONS} - Health factor: {health_factor}")
            except Exception as e:
                log.e(f"OCC calibration test iteration {iteration} failed: ", str(e))
                test.fail()

if is_mipi_device():
    with test.closure(f"OCC calibration test with host assistance - {NUM_ITERATIONS} iterations"):
        for iteration in range(1, NUM_ITERATIONS + 1):
            try:
                log.i(f"Starting OCC calibration with host assistance iteration {iteration}/{NUM_ITERATIONS}")
                host_assistance = True
                image_width, image_height, fps = 1280, 720, 30
                occ_json = on_chip_calibration_json(None, host_assistance)
                config, pipeline, calib_dev = get_calibration_device(image_width, image_height, fps)
                health_factor, new_calib_bytes = calibration_main(config, pipeline, calib_dev, True, occ_json, None, host_assistance=host_assistance, return_table=True)
                test.check(abs(health_factor) < HEALTH_FACTOR_THRESHOLD or new_calib_bytes is None)
                log.i(f"Completed OCC calibration iteration {iteration}/{NUM_ITERATIONS} - Health factor: {health_factor}")   
            except Exception as e:
                log.e(f"OCC calibration test with host assistance iteration {iteration} failed: ", str(e))
                test.fail()
test.print_results_and_exit()


