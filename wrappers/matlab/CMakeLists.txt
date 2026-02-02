cmake_minimum_required(VERSION 3.10)

project(RealsenseMatlabWrappers)

find_package(Matlab COMPONENTS MX_LIBRARY MAT_LIBRARY REQUIRED)
list(APPEND DEPENDENCIES ${Matlab_MAT_LIBRARY})
# Workaround for https://github.com/realsenseai/librealsense/pull/3611/files CMAKE_DEBUG_SUFFIX
# With MSVC2017 the realsense2 dependency without the suffix is still added to the debug build
# via the /DEFAULTLIB:realsense2.lib entry
# A similar report exist dated in 2011 https://cmake.org/pipermail/cmake/2011-April/043808.html
SET( CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS} " /NODEFAULTLIB:realsense2" )

set(MATLAB_CPP librealsense_mex.cpp Factory.cpp)
set(MATLAB_H Factory.h MatlabParamParser.h rs2_type_traits.h types.h)
set(MATLAB_M align.m camera_info.m colorizer.m config.m context.m
             decimation_filter.m depth_frame.m depth_sensor.m
             depth_stereo_sensor.m device.m device_hub.m disparity_frame.m
             disparity_transform.m filter.m format.m frame.m
             frame_metadata_value.m frame_queue.m frameset.m hole_filling_filter.m
             motion_frame.m motion_stream_profile.m option.m options.m pipeline.m
             pipeline_profile.m playback.m playback_status.m pointcloud.m points.m
             pose_frame.m recorder.m roi_sensor.m save_single_frameset.m
             save_to_ply.m sensor.m spatial_filter.m stream.m stream_profile.m
             syncer.m temporal_filter.m timestamp_domain.m video_frame.m
             video_stream_profile.m
   )
set(MATLAB_EXAMPLES depth_example.m pointcloud_example.m rosbag_example.m depth_view_example.m capture_example.m)

matlab_add_mex(NAME librealsense_mex
               SRC ${MATLAB_CPP} ${MATLAB_H}
               LINK_TO ${DEPENDENCIES})
set_target_properties(librealsense_mex PROPERTIES FOLDER Wrappers)

#TODO: additionally, the compiled mex file and all the .m files need to be copied into a folder called `+realsense` at the install site
