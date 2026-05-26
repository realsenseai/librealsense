# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Diagnostic for intermittent enumeration where the device comes up without
# the safety sensor (observed after hub-driven resets, e.g. via Acroname).
# Single-shot test -- invoke with --repeat 5 to exercise the failure path
# 5 times, each preceded by the framework's hub recycle.

#test:device D585S

import pyrealsense2 as rs
from rspy import test, log


with test.closure("Safety sensor is present after device reset"):
    device, _ = test.find_first_device_or_exit()

    sensor_names = [s.get_info(rs.camera_info.name) for s in device.sensors
                    if s.supports(rs.camera_info.name)]
    log.d("Enumerated sensors:", sensor_names)

    # first_safety_sensor() throws "Could not find requested sensor type!"
    # when the device enumerated without it -- the exact symptom we are hunting.
    safety_sensor = device.first_safety_sensor()
    test.check(safety_sensor is not None, "first_safety_sensor() returned None")

test.print_results_and_exit()
