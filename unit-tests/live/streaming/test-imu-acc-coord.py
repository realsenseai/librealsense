# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# This test verifies IMU accelerometer functionality by streaming acceleration data
# and checking that the Y-axis acceleration matches Earth's gravitational acceleration (~-9.8 m/s^2).
# The camera must be placed horizontally (flat) for accurate gravity measurement.
#
# The test checks for outliers - samples that fall outside the configured threshold
# of the expected gravity acceleration, which could indicate sensor noise or improper
# camera placement. The test fails if any outliers are detected.
#
# Thresholds:
# - GRAVITY_THRESHOLD_FACTOR: Tolerance for average gravity validation (default: ±25%)
# - OUTLIER_THRESHOLD_FACTOR: Per-sample outlier detection (default: ±50%)
#
# This test supports both separate accel/gyro streams and combined motion format.

# test:device each(D400*) 
# test:device each(D500*)

import pyrealsense2 as rs
from rspy import test, log
import math

device, ctx = test.find_first_device_or_exit()
device_name = device.get_info( rs.camera_info.name )

GRAVITY_ACCELERATION = -9.8  # m/s^2
GRAVITY_THRESHOLD_FACTOR = 0.25  # ±25% tolerance for average gravity validation
OUTLIER_THRESHOLD_FACTOR = 0.50  # ±50% tolerance for per-sample outlier detection

with test.closure("Stream acceleration and verify gravity"):
    sensors = {sensor.get_info( rs.camera_info.name ) : sensor for sensor in device.query_sensors()}

    if 'Motion Module' in sensors:  # Filter out models without IMU
        # Get motion module sensor
        sensor = sensors['Motion Module']
        
        # Check if device supports combined motion or separate accel stream
        has_combined_motion = rs.stream.motion in [p.stream_type() for p in sensor.get_stream_profiles()]
        has_accel = rs.stream.accel in [p.stream_type() for p in sensor.get_stream_profiles()]
        
        if not has_combined_motion and not has_accel:
            log.d("Device does not have accelerometer or motion stream")
            test.print_results_and_exit()
        
        try:
            # Create pipeline and config
            pipeline = rs.pipeline()
            config = rs.config()
            
            # Enable appropriate stream based on device capabilities
            if has_combined_motion:
                log.d("Using combined motion format")
                motion_profile = next(p for p in sensor.get_stream_profiles() if p.stream_type() == rs.stream.motion)
                motion_profile_video = motion_profile.as_motion_stream_profile()
                config.enable_stream(rs.stream.motion, motion_profile_video.stream_index(), rs.format.combined_motion, motion_profile_video.fps())
                using_combined = True
            else:
                log.d("Using separate accelerometer stream")
                accel_profile = next(p for p in sensor.get_stream_profiles() if p.stream_type() == rs.stream.accel)
                accel_profile_video = accel_profile.as_motion_stream_profile()
                config.enable_stream(rs.stream.accel, accel_profile_video.stream_index(), rs.format.motion_xyz32f, accel_profile_video.fps())
                using_combined = False
            
            pipeline.start(config)
            
            # Collect acceleration samples
            accel_samples_y = []
            num_samples = 30
            
            log.d("Streaming acceleration data (camera must be placed horizontally)...")
            max_attempts = num_samples * 2
            attempts = 0
            while len(accel_samples_y) < num_samples and attempts < max_attempts:
                frames = pipeline.wait_for_frames()
                
                if using_combined:
                    # Extract accel data from combined motion frame
                    motion_frame = frames.first(rs.stream.motion)
                    if motion_frame:
                        combined_data = motion_frame.as_motion_frame().get_combined_motion_data()
                        sample_index = len(accel_samples_y) + 1
                        log.d(f"Sample {sample_index}: accel = [{combined_data.linear_acceleration.x:.2f}, {combined_data.linear_acceleration.y:.2f}, {combined_data.linear_acceleration.z:.2f}] m/s^2")
                        accel_samples_y.append(combined_data.linear_acceleration.y)
                    else:
                        log.d("No motion frame in current frameset; retrying...")
                else:
                    # Extract accel data from separate accel stream
                    accel_frame = frames.first(rs.stream.accel)
                    if accel_frame:
                        accel_data = accel_frame.as_motion_frame().get_motion_data()
                        sample_index = len(accel_samples_y) + 1
                        log.d(f"Sample {sample_index}: accel = [{accel_data.x:.2f}, {accel_data.y:.2f}, {accel_data.z:.2f}] m/s^2")
                        accel_samples_y.append(accel_data.y)
                    else:
                        log.d("No accelerometer frame in current frameset; retrying...")
                
                attempts += 1
            
            if len(accel_samples_y) < num_samples:
                log.d(f"Collected {len(accel_samples_y)} valid accelerometer samples out of requested {num_samples}")            
            
            pipeline.stop()
            
            # Calculate average Y-axis acceleration (should be near -9.8 m/s^2 when horizontal)
            if accel_samples_y:
                # Check for outliers using per-sample threshold
                outlier_threshold = abs(GRAVITY_ACCELERATION) * OUTLIER_THRESHOLD_FACTOR
                lower_bound = GRAVITY_ACCELERATION - outlier_threshold
                upper_bound = GRAVITY_ACCELERATION + outlier_threshold
                outliers = [sample for sample in accel_samples_y if sample < lower_bound or sample > upper_bound]
                
                if outliers:
                    log.e(f"Found {len(outliers)} outlier(s) outside ±{int(OUTLIER_THRESHOLD_FACTOR*100)}% of gravity acceleration ({lower_bound:.2f} to {upper_bound:.2f} m/s^2): {[f'{o:.2f}' for o in outliers]}")
                    test.fail()
                else:
                    log.d(f"No outliers detected (all samples within ±{int(OUTLIER_THRESHOLD_FACTOR*100)}% of gravity acceleration)")
                
                avg_accel_y = sum(accel_samples_y) / len(accel_samples_y)
                log.d(f"Average Y-axis acceleration: {avg_accel_y:.2f} m/s^2")
                
                # Validate average acceleration against gravity threshold
                accel_error = abs(avg_accel_y - GRAVITY_ACCELERATION)
                log.d(f"Gravity acceleration error: {accel_error:.2f} m/s^2")
                
                gravity_threshold = abs(GRAVITY_ACCELERATION) * GRAVITY_THRESHOLD_FACTOR
                if accel_error <= gravity_threshold:
                    log.d(f"[PASS] Acceleration matches expected gravity acceleration (-9.8 m/s^2 ± {gravity_threshold:.2f} m/s^2)")
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
