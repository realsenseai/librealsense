# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

# This test verifies IMU accelerometer functionality by streaming acceleration data
# and checking that the Y-axis acceleration matches Earth's gravitational acceleration (~-9.8 m/s^2).
# The camera must be placed horizontally (flat) for accurate gravity measurement.

# test:device each(D400*)

import pyrealsense2 as rs
from rspy import test, log
import math

device, ctx = test.find_first_device_or_exit()
device_name = device.get_info( rs.camera_info.name )

GRAVITY_ACCELERATION = -9.8  # m/s^2
GRAVITY_THRESHOLD = 0.5  # m/s^2 tolerance

with test.closure("Stream acceleration and verify gravity"):
    sensors = {sensor.get_info( rs.camera_info.name ) : sensor for sensor in device.query_sensors()}

    if 'Motion Module' in sensors:  # Filter out models without IMU
        # Get motion module sensor
        sensor = sensors['Motion Module']
        
        # Find accel profile
        accel_profile = None
        for profile in sensor.get_stream_profiles():
            if profile.stream_type() == rs.stream.accel:
                accel_profile = profile
                break
        
        if not accel_profile:
            log.d("Device does not have accelerometer stream")
            test.print_results_and_exit()
        
        try:
            # Create pipeline and config
            pipeline = rs.pipeline()
            config = rs.config()
            
            # Enable accel stream
            accel_profile_video = accel_profile.as_motion_stream_profile()
            config.enable_stream(rs.stream.accel, accel_profile_video.stream_index(), rs.format.motion_xyz32f, accel_profile_video.fps())
            
            pipeline.start(config)
            
            # Collect acceleration samples
            accel_samples_y = []
            num_samples = 30
            
            log.d("Streaming acceleration data (camera must be placed horizontally)...")
            for i in range(num_samples):
                frames = pipeline.wait_for_frames()
                accel_frame = frames.first(rs.stream.accel)
                
                if accel_frame:
                    # Get acceleration data (x, y, z)
                    accel_data = accel_frame.as_motion_frame().get_motion_data()
                    log.d(f"Sample {i+1}: accel = [{accel_data.x:.2f}, {accel_data.y:.2f}, {accel_data.z:.2f}] m/s^2")
                    accel_samples_y.append(accel_data.y)
            
            pipeline.stop()
            
            # Calculate average Y-axis acceleration (should be near -9.8 m/s^2 when horizontal)
            if accel_samples_y:
                avg_accel_y = sum(accel_samples_y) / len(accel_samples_y)
                log.d(f"Average Y-axis acceleration: {avg_accel_y:.2f} m/s^2")
                
                # Check if acceleration is close to gravity
                accel_error = abs(avg_accel_y - GRAVITY_ACCELERATION)
                log.d(f"Gravity acceleration error: {accel_error:.2f} m/s^2")
                
                if accel_error <= GRAVITY_THRESHOLD:
                    log.d(f"✓ Acceleration matches expected gravity acceleration (-9.8 m/s^2 ± {GRAVITY_THRESHOLD} m/s^2)")
                else:
                    log.e(f"Acceleration {avg_accel_y:.2f} m/s^2 is too far from expected gravity acceleration {GRAVITY_ACCELERATION} m/s^2 (error: {accel_error:.2f} m/s^2)")
                    test.fail()
            else:
                log.e("No acceleration samples collected")
                test.fail()
                
        except Exception as e:
            test.unexpected_exception()
    else:
        log.d("Device does not have Motion Module, skipping test")

test.print_results_and_exit()
