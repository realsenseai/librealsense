
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

# Post-processing filters reference dataset (post-processing/pytest-filters.py).
# Shipped as a single zip (308 small files) so a build downloads it once instead of the
# test doing hundreds of per-file requests at runtime.
set( PP_Filters_Dataset_ZIP post_processing_tests_2018_ww18.zip )
dl_file( https://librealsense.realsenseai.com/rs-tests recordings ${PP_Filters_Dataset_ZIP} OFF )
get_filename_component( PP_Recordings_Dir recordings ABSOLUTE BASE_DIR ${CMAKE_CURRENT_BINARY_DIR} )
# Extract once; sentinel is one of the dataset files (first sequence's metadata).
if( EXISTS "${PP_Recordings_Dir}/${PP_Filters_Dataset_ZIP}" AND NOT EXISTS "${PP_Recordings_Dir}/1551257764229.0.Output.csv" )
    message( STATUS "Extracting ${PP_Filters_Dataset_ZIP}" )
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xf "${PP_Recordings_Dir}/${PP_Filters_Dataset_ZIP}"
        WORKING_DIRECTORY "${PP_Recordings_Dir}"
        RESULT_VARIABLE PP_Filters_Extract_Result )
    if( PP_Filters_Extract_Result )
        message( SEND_ERROR "Failed to extract ${PP_Filters_Dataset_ZIP}: ${PP_Filters_Extract_Result}" )
    endif()
endif()

