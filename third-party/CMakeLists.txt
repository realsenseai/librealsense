string(REPLACE ${PROJECT_SOURCE_DIR}/ "" _rel_path ${CMAKE_CURRENT_LIST_DIR})

pop_security_flags() # remove security flags for third party, as we cannot guarantee their security enforcment

if(USE_EXTERNAL_NLOHMANN_JSON)
    find_package(nlohmann_json 3.11.3 REQUIRED)
else()
    include(CMake/external_json.cmake)
endif()

add_subdirectory( "${CMAKE_CURRENT_LIST_DIR}/rsutils" )

# Add additional include directories to allow file to include rosbag headers
include(${_rel_path}/realsense-file/config.cmake)

if(BUILD_EASYLOGGINGPP)
    include(${_rel_path}/easyloggingpp/CMakeLists.txt)
endif()

add_subdirectory(${_rel_path}/realsense-file)
add_subdirectory( ${_rel_path}/tclap )

if( BUILD_WITH_DDS )
    add_subdirectory( "${CMAKE_CURRENT_LIST_DIR}/realdds" )
endif()

# restore security flags
push_security_flags()
