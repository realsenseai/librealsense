
set(PP_Rosbag_Recordings_URL ${TEST_DATA_BASE_URL}/Rosbag_unit_test_records)

# for rec-play/test-non-realtime.py
dl_file( ${PP_Rosbag_Recordings_URL} recordings recording_deadlock.db3 e1a4265bb11ec06343f826282cf827b6f9c0810c )

# for rec-play/test-playback-stress.py and for post-processing/post-processing-from-bag.py
dl_file( ${PP_Rosbag_Recordings_URL} recordings all_combinations_depth_color.db3 5b02f695419a46a894d76959511c26e1df24e4d1 )

# for post-processing/post-processing-from-bag.py
dl_file( ${PP_Rosbag_Recordings_URL} recordings [aligned_2c]_all_combinations_depth_color.db3 052570e4451bd5ce4d5d10b66d7b334175697413 )

# for post-processing/post-processing-from-bag.py
dl_file( ${PP_Rosbag_Recordings_URL} recordings [aligned_2d]_all_combinations_depth_color.db3 ee78c90c65f8c9b89292494cce1c588728f0923d )

# for post-processing/post-processing-from-bag.py
dl_file( ${PP_Rosbag_Recordings_URL} recordings [pointcloud]_all_combinations_depth_color.db3 d1e5321402129504a5c09884b5cb2fb1afdb9270 )

# kept as .bag for backward-compat: convert test (live/tools/pytest-rs-convert-to-db3.py)
dl_file( ${PP_Rosbag_Recordings_URL} recordings single_depth_color_640x480.bag c180fd8b4d87d1f9d86f2361c5225d724ca977c7 )

# for rec-play/pytest-native-ros2-playback.py
dl_file( ${PP_Rosbag_Recordings_URL} recordings d555_all_streams_native_ros2.db3 2877180b5bdca9c9a8cb5f9c213d1b6f5f059a78 )
dl_file( ${PP_Rosbag_Recordings_URL} recordings d555_all_streams_native_ros2_zstd_compressed.db3 51890b56376be6ae7efb3f5ae212c1256324477d )
dl_file( ${PP_Rosbag_Recordings_URL} recordings d455_all_streams_imu_native_ros2.db3 e6ef22d0c403d2970941ac41b4484a62b06f974b )
dl_file( ${PP_Rosbag_Recordings_URL} recordings d455_all_streams_imu_native_ros2_zstd_compressed.db3 54120dc1630315e8b7bd131d2477c9c156721e75 )

# for rec-play/pytest-playback-step.py
dl_file( ${PP_Rosbag_Recordings_URL} recordings d455_depth_non_native.db3 fd2678f0559689a9bb89a31af68bc652e169c9ad )

# Post-processing filters reference dataset (post-processing/pytest-filters.py).
# Shipped as a single zip (308 small files) so a build downloads it once instead of the
# test doing hundreds of per-file requests at runtime.
set( PP_Filters_Dataset_ZIP post_processing_tests_2018_ww18.zip )
dl_file( ${TEST_DATA_BASE_URL} recordings ${PP_Filters_Dataset_ZIP} dbb7475422187e5390df00385cf870cf7e563ec2 )
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

