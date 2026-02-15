cmake_minimum_required(VERSION 3.10)

set( LRS_TARGET realsense2 )
project( ${LRS_TARGET} LANGUAGES CXX C )

# Allow librealsense2 and all of the nested project to include the main repo folder
set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${REPO_ROOT})

include(CMake/lrs_options.cmake)
include(CMake/connectivity_check.cmake)
#Deprecation message, should be removed in future releases
if(${FORCE_LIBUVC} OR ${FORCE_WINUSB_UVC} OR ${ANDROID_USB_HOST_UVC})
    MESSAGE(DEPRECATION "FORCE_LIBUVC, FORCE_WINUSB_UVC and ANDROID_USB_HOST_UVC are deprecated, use FORCE_RSUSB_BACKEND instead")
    set(FORCE_RSUSB_BACKEND ON)
endif()

# Checking Internet connection, as DEPTH CAM needs to download the FW from amazon cloud
if(IMPORT_DEPTH_CAM_FW AND NOT INTERNET_CONNECTION)
    message(WARNING "No internet connection, disabling IMPORT_DEPTH_CAM_FW")
    set(IMPORT_DEPTH_CAM_FW OFF)
endif()

if (BUILD_PC_STITCHING AND NOT BUILD_GLSL_EXTENSIONS)
    MESSAGE(STATUS "BUILD_PC_STITCHING explicitely depends on BUILD_GLSL_EXTENSIONS, set it ON")
    SET(BUILD_GLSL_EXTENSIONS ON)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/CMake)

# include security flags helper functions
include(CMake/security_flags_helper_functions.cmake)

# include librealsense general configuration
include(CMake/global_config.cmake)

# include os specific macros
# macro definition located at "CMake/global_config.cmake"
include(CMake/include_os.cmake)

# set os specific configuration flags
# macro definition located at "CMake/<OS>_config.cmake"
os_set_flags()

# set global configuration flags
# macro definition located at "CMake/global_config.cmake"
global_set_flags()

# If no type is given explicitly the type is STATIC or SHARED based on whether the current value of the variable [BUILD_SHARED_LIBS](https://cmake.org/cmake/help/v3.10/variable/BUILD_SHARED_LIBS.html#variable:BUILD_SHARED_LIBS) is ON
add_library(${LRS_TARGET})
# Apply ASan flags if enabled
if(BUILD_ASAN)
  message(STATUS "AddressSanitizer is enabled")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -g")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -g")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
  target_compile_options(${LRS_TARGET} PRIVATE -fsanitize=address -g)
endif()

config_cxx_flags()

# set library version
set_target_properties(${LRS_TARGET} PROPERTIES VERSION ${REALSENSE_VERSION_STRING} SOVERSION "${REALSENSE_VERSION_MAJOR}.${REALSENSE_VERSION_MINOR}")

include(include/CMakeLists.txt)
include(src/CMakeLists.txt)
include(third-party/CMakeLists.txt)

target_link_libraries( ${LRS_TARGET} PUBLIC rsutils )

if(BUILD_WITH_DDS)
    message(STATUS "Building with FastDDS")
	include(CMake/external_foonathan_memory.cmake)
	include(CMake/external_fastdds.cmake)

	target_link_libraries( ${LRS_TARGET} PRIVATE realdds )
else()
    # Even without full DDS support, we need fastcdr for ROS2 bag file serialization
    message(STATUS "Building with FastCDR for ROS2 bag support")
    include(CMake/external_fastcdr.cmake)
    target_link_libraries( ${LRS_TARGET} PRIVATE fastcdr )
endif()

# configure the project according to OS specific requirments
# macro definition located at "CMake/<OS>_config.cmake"
os_target_config()

# global project configuration
# macro definition located at "CMake/global_config.cmake"
global_target_config()

include(CMake/install_config.cmake)

add_subdirectory(wrappers)
if ( ( BUILD_EXAMPLES OR BUILD_PC_STITCHING ) AND BUILD_GLSL_EXTENSIONS )
    find_package(glfw3 3.3 QUIET)
    if(NOT TARGET glfw)
        message(STATUS "GLFW 3.3 not found; using internal version")
        set(GLFW_INSTALL ON CACHE BOOL "" FORCE)
        add_subdirectory(third-party/glfw)
    endif()
    add_subdirectory(src/gl)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
    add_subdirectory(tools)
elseif(BUILD_TOOLS)
    add_subdirectory(tools)
endif()

if(BUILD_UNIT_TESTS)
    include( CMake/external_catch2.cmake )
    add_subdirectory(unit-tests)
endif()

if(IMPORT_DEPTH_CAM_FW)
    add_subdirectory(common/fw)
endif()

include(CMake/embedd_udev_rules.cmake)

if( BUILD_RS2_ALL )
    include( src/realsense2-all.cmake )
endif()
