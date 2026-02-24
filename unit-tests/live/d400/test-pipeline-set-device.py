# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

# LibCI doesn't have D435i so //test:device D435I// is disabled for now
#test:device: D455

import pyrealsense2 as rs
from rspy import test

gyro_sensitivity_value = 4.0

with test.closure("pipeline - set device"):
    cfg = rs.config()

    dev, ctx = test.find_first_device_or_exit()
    motion_sensor = dev.first_motion_sensor()
    pipe = rs.pipeline(ctx)
    pipe.set_device(dev)

    motion_sensor.set_option(rs.option.gyro_sensitivity, gyro_sensitivity_value)

    cfg.enable_stream(rs.stream.accel)
    cfg.enable_stream(rs.stream.gyro)

    profile = pipe.start(cfg)
    device_from_profile = profile.get_device()
    sensor = device_from_profile.first_motion_sensor()
    sensor_gyro_sensitivity_value = sensor.get_option(rs.option.gyro_sensitivity)
    test.check_equal(gyro_sensitivity_value, sensor_gyro_sensitivity_value)
    pipe.stop()

test.print_results_and_exit()