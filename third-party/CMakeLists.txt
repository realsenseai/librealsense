string(REPLACE ${PROJECT_SOURCE_DIR}/ "" _rel_path ${CMAKE_CURRENT_LIST_DIR})


add_subdirectory( "${CMAKE_CURRENT_LIST_DIR}/rsutils" )

pop_security_flags() # remove security flags for third party, as we cannot guarantee their security enforcment

include(CMake/external_json.cmake)
# Add additional include directories to allow file to include rosbag headers
include(${_rel_path}/realsense-file/CMakeLists.txt)

if(BUILD_EASYLOGGINGPP)
    include(${_rel_path}/easyloggingpp/CMakeLists.txt)
endif()

add_subdirectory( ${_rel_path}/tclap )

if( BUILD_WITH_DDS )
    add_subdirectory( "${CMAKE_CURRENT_LIST_DIR}/realdds" )
endif()

# restore security flags
push_security_flags()
