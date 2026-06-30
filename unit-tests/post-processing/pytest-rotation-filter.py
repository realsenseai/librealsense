# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pyrealsense2 as rs
import numpy as np

# Parameters for frame creation and depth intrinsics
input_res_x = 640
input_res_y = 480
focal_length = 600
depth_units = 0.001
stereo_baseline_mm = 50
frames = 5  # Number of frames to process


# Function to create depth intrinsics directly
def create_depth_intrinsics():
    depth_intrinsics = rs.intrinsics()
    depth_intrinsics.width = input_res_x
    depth_intrinsics.height = input_res_y
    depth_intrinsics.ppx = input_res_x / 2.0
    depth_intrinsics.ppy = input_res_y / 2.0
    depth_intrinsics.fx = focal_length
    depth_intrinsics.fy = focal_length
    depth_intrinsics.model = rs.distortion.brown_conrady
    depth_intrinsics.coeffs = [0, 0, 0, 0, 0]
    return depth_intrinsics


# Function to create a video stream with specified parameters
def create_video_stream(depth_intrinsics):
    vs = rs.video_stream()
    vs.type = rs.stream.depth
    vs.index = 0
    vs.uid = 0
    vs.width = input_res_x
    vs.height = input_res_y
    vs.fps = 30
    vs.bpp = 2
    vs.fmt = rs.format.z16
    vs.intrinsics = depth_intrinsics
    return vs


# Function to create a synthetic frame
def create_frame(depth_stream_profile, index):
    frame = rs.software_video_frame()
    data = np.arange(input_res_x * input_res_y, dtype=np.uint16).reshape((input_res_y, input_res_x))
    frame.pixels = data.tobytes()
    frame.bpp = 2
    frame.stride = input_res_x * 2
    frame.timestamp = index * 33
    frame.domain = rs.timestamp_domain.system_time
    frame.frame_number = index
    frame.profile = depth_stream_profile.as_video_stream_profile()
    frame.depth_units = depth_units
    return frame


# Function to validate rotated results based on the angle
def validate_rotation_results(filtered_frame, angle):
    rotated_height = filtered_frame.profile.as_video_stream_profile().height()
    rotated_width = filtered_frame.profile.as_video_stream_profile().width()

    # Reshape the rotated data according to its actual dimensions
    rotated_data = np.frombuffer(filtered_frame.get_data(), dtype=np.uint16).reshape((rotated_height, rotated_width))

    # Original data for comparison
    original_data = np.arange(input_res_x * input_res_y, dtype=np.uint16).reshape((input_res_y, input_res_x))

    # Determine the expected rotated result based on the angle
    if angle == 90:
        expected_data = np.rot90(original_data, k=-1)  # Clockwise
    elif angle == 180:
        expected_data = np.rot90(original_data, k=2)  # 180 degrees
    elif angle == -90:
        expected_data = np.rot90(original_data, k=1)  # Counterclockwise

    # Convert numpy arrays to lists before comparison
    rotated_list = rotated_data.flatten().tolist()
    expected_list = expected_data.flatten().tolist()

    # Compare the flattened lists
    assert rotated_list == expected_list


################################################################################################
def test_rotation_filter():
    # Set up software device and depth sensor
    sw_dev = rs.software_device()
    depth_sensor = sw_dev.add_sensor("Depth")

    # Initialize intrinsics and video stream profile
    depth_intrinsics = create_depth_intrinsics()
    vs = create_video_stream(depth_intrinsics)
    depth_stream_profile = depth_sensor.add_video_stream(vs)

    frame_queue = rs.frame_queue(15)

    # Define rotation angles to test
    rotation_angles = [90, 180, -90]
    for angle in rotation_angles:
        rotation_filter = rs.rotation_filter([rs.stream.depth])
        rotation_filter.set_option(rs.option.rotation, angle)

        # Start depth sensor
        depth_sensor.open(depth_stream_profile)
        depth_sensor.start(frame_queue)

        for i in range(frames):
            # Create and process each frame
            frame = create_frame(depth_stream_profile, i)
            depth_sensor.on_video_frame(frame)

            # Wait for frames and apply rotation filter
            depth_frame = frame_queue.wait_for_frame()
            filtered_depth = rotation_filter.process(depth_frame)

            # Validate rotated frame results
            validate_rotation_results(filtered_depth, angle)

        # Stop and close the sensor after each angle test
        depth_sensor.stop()
        depth_sensor.close()
