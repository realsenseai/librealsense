cmake_minimum_required(VERSION 3.10)

if(UNIX AND NOT APPLE)
  include(${CMAKE_CURRENT_LIST_DIR}/cmake/check_c_compiler_uses_glibc.cmake)
  check_c_compiler_uses_glibc(USES_GLIBC)
  if(USES_GLIBC)
    # Ensure GNU extended libc API is used, as C++ test code will.
    # See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=2082.
    set(ROSBAG2_COMPILE_FLAGS "${ROSBAG2_COMPILE_FLAGS};_GNU_SOURCE")
  endif()
endif()

if(WIN32)
  set(time_impl_c time_win32.c)
else()
  set(time_impl_c time_unix.c)
endif()

file(GLOB_RECURSE HEADER_FILES_RCUTILS
    "${CMAKE_CURRENT_LIST_DIR}/include/rcutils/*.h"
)

file(GLOB SOURCE_FILES_RCUTILS
    "${CMAKE_CURRENT_LIST_DIR}/src/*.c"
)

# Remove platform-specific time implementation files from the list
list(FILTER SOURCE_FILES_RCUTILS EXCLUDE REGEX ".*time_win32\\.c$")
list(FILTER SOURCE_FILES_RCUTILS EXCLUDE REGEX ".*time_unix\\.c$")

# Add the correct platform-specific time implementation
list(APPEND SOURCE_FILES_RCUTILS "${CMAKE_CURRENT_LIST_DIR}/src/${time_impl_c}")

message(STATUS "ROSBAG2_COMPILE_FLAGS before adding rcutils: ${ROSBAG2_COMPILE_FLAGS}")
set(ROSBAG2_COMPILE_FLAGS "${ROSBAG2_COMPILE_FLAGS};RCUTILS_BUILDING_DLL")
message(STATUS "ROSBAG2_COMPILE_FLAGS after adding rcutils: ${ROSBAG2_COMPILE_FLAGS}")

