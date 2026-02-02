# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#test:device each(D400*)
#test:device each(D500*) !D555
# We can enable D555 when RSDEV-3159 is resolved


import pyrealsense2 as rs
from rspy import test, log

device, ctx = test.find_first_device_or_exit()
device_name = device.get_info( rs.camera_info.name )

with test.closure("Check depth-IMU extrinsics"):
    sensors = {sensor.get_info( rs.camera_info.name ) : sensor for sensor in device.query_sensors()}

    if 'Motion Module' in sensors: # Filter out models without IMU
        depth_profile = rs.stream_profile()
        imu_profile = rs.stream_profile()
        
        sensor = sensors['Stereo Module']
        for profile in sensor.get_stream_profiles() :
            if profile.stream_type() == rs.stream.depth:
                depth_profile = profile
                break
                
        sensor = sensors['Motion Module']
        log.d("Found Sensor: " +  sensor.get_info(rs.camera_info.name) + " of type: " + device.get_info(rs.camera_info.imu_type))
        for profile in sensor.get_stream_profiles() :
            if profile.stream_type() == rs.stream.motion: # For combined motion profiles.
                imu_profile = profile
                break
            if profile.stream_type() == rs.stream.gyro:
                imu_profile = profile
                break

        try:
            actual_extrinsics = depth_profile.get_extrinsics_to( imu_profile )
        except:
            test.unexpected_exception()
        # Using default (CAD) values. Currently no specific IMU extrinsic calibration
        expected_rotation = [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]
        expected_translation = [0, 0, 0]
        
        compatible_devices = ["D435I", "D436"]
        if any(device in device_name for device in compatible_devices):
            expected_translation = [-0.005520000122487545, 0.005100000184029341, 0.011739999987185001]
        compatible_devices = ["D455"]
        if any(device in device_name for device in compatible_devices):
            expected_translation = [-0.030220000073313713, 0.007400000002235174, 0.016019999980926514]
        compatible_devices = ["D457"]
        if any(device in device_name for device in compatible_devices):
            expected_translation = [-0.09529999643564224, -0.000560000014957041, 0.017400000244379044]
        compatible_devices = ["D555"]
        if any(device in device_name for device in compatible_devices):
            expected_translation = [-0.03739999979734421, 0.007189999800175428, 0.022299999371170998]
        compatible_devices = ["D585"]
        if any(device in device_name for device in compatible_devices):
            expected_translation = [-0.01950000040233135, 0, -0.011400000192224979]
        test.check_float_lists( actual_extrinsics.rotation, expected_rotation )
        test.check_float_lists( actual_extrinsics.translation, expected_translation )


test.print_results_and_exit()
