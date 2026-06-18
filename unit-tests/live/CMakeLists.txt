
# recordings URL is same as in live-test.cmake
set(PP_Rosbag_Recordings_URL https://librealsense.realsenseai.com/rs-tests/Rosbag_unit_test_records)

# for rec-play/test-non-realtime.py
dl_file( ${PP_Rosbag_Recordings_URL} recordings recording_deadlock.db3 OFF )

# for rec-play/test-playback-stress.py and for post-processing/post-processing-from-bag.py
dl_file( ${PP_Rosbag_Recordings_URL} recordings all_combinations_depth_color.db3 OFF )

# for post-processing/post-processing-from-bag.py
dl_file( ${PP_Rosbag_Recordings_URL} recordings [aligned_2c]_all_combinations_depth_color.db3 OFF )

# for post-processing/post-processing-from-bag.py
dl_file( ${PP_Rosbag_Recordings_URL} recordings [aligned_2d]_all_combinations_depth_color.db3 OFF )

# for post-processing/post-processing-from-bag.py
dl_file( ${PP_Rosbag_Recordings_URL} recordings [pointcloud]_all_combinations_depth_color.db3 OFF )

# kept as .bag for backward-compat: convert test (live/tools/pytest-rs-convert-to-db3.py)
dl_file( ${PP_Rosbag_Recordings_URL} recordings single_depth_color_640x480.bag OFF )

# for rec-play/pytest-native-ros2-playback.py
dl_file( ${PP_Rosbag_Recordings_URL} recordings d555_all_streams_native_ros2.db3 OFF )
dl_file( ${PP_Rosbag_Recordings_URL} recordings d555_all_streams_native_ros2_zstd_compressed.db3 OFF )
dl_file( ${PP_Rosbag_Recordings_URL} recordings d455_all_streams_imu_native_ros2.db3 OFF )
dl_file( ${PP_Rosbag_Recordings_URL} recordings d455_all_streams_imu_native_ros2_zstd_compressed.db3 OFF )

# for rec-play/pytest-playback-step.py
dl_file( ${PP_Rosbag_Recordings_URL} recordings d455_depth_non_native.db3 OFF )

