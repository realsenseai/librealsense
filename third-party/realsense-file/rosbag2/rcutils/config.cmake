cmake_minimum_required(VERSION 3.10)

if(WIN32)
  set(time_impl_c time_win32.c)
else()
  set(time_impl_c time_unix.c)
endif()

set(HEADER_FILES_RCUTILS
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/allocator.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/cmdline_parser.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/env.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/error_handling.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/filesystem.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/find.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/format_string.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/get_env.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/isalnum_no_locale.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/logging.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/logging_macros.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/macros.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/process.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/qsort.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/repl_str.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/shared_library.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/snprintf.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/split.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/stdatomic_helper.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/strdup.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/strerror.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/time.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/types.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/visibility_control.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/visibility_control_macros.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/stdatomic_helper/gcc/stdatomic.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/stdatomic_helper/win32/stdatomic.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/testing/fault_injection.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/types/array_list.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/types/char_array.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/types/hash_map.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/types/rcutils_ret.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/types/string_array.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/types/string_map.h
    ${CMAKE_CURRENT_LIST_DIR}/include/rcutils/types/uint8_array.h
)

set(SOURCE_FILES_RCUTILS
    ${CMAKE_CURRENT_LIST_DIR}/src/allocator.c
    ${CMAKE_CURRENT_LIST_DIR}/src/array_list.c
    ${CMAKE_CURRENT_LIST_DIR}/src/char_array.c
    ${CMAKE_CURRENT_LIST_DIR}/src/cmdline_parser.c
    ${CMAKE_CURRENT_LIST_DIR}/src/env.c
    ${CMAKE_CURRENT_LIST_DIR}/src/error_handling.c
    ${CMAKE_CURRENT_LIST_DIR}/src/filesystem.c
    ${CMAKE_CURRENT_LIST_DIR}/src/find.c
    ${CMAKE_CURRENT_LIST_DIR}/src/format_string.c
    ${CMAKE_CURRENT_LIST_DIR}/src/get_env.c
    ${CMAKE_CURRENT_LIST_DIR}/src/hash_map.c
    ${CMAKE_CURRENT_LIST_DIR}/src/logging.c
    ${CMAKE_CURRENT_LIST_DIR}/src/process.c
    ${CMAKE_CURRENT_LIST_DIR}/src/qsort.c
    ${CMAKE_CURRENT_LIST_DIR}/src/repl_str.c
    ${CMAKE_CURRENT_LIST_DIR}/src/shared_library.c
    ${CMAKE_CURRENT_LIST_DIR}/src/snprintf.c
    ${CMAKE_CURRENT_LIST_DIR}/src/split.c
    ${CMAKE_CURRENT_LIST_DIR}/src/strdup.c
    ${CMAKE_CURRENT_LIST_DIR}/src/strerror.c
    ${CMAKE_CURRENT_LIST_DIR}/src/string_array.c
    ${CMAKE_CURRENT_LIST_DIR}/src/string_map.c
    ${CMAKE_CURRENT_LIST_DIR}/src/time.c
    ${CMAKE_CURRENT_LIST_DIR}/src/${time_impl_c}
    ${CMAKE_CURRENT_LIST_DIR}/src/uint8_array.c
    ${CMAKE_CURRENT_LIST_DIR}/src/testing/fault_injection.c
)

message(STATUS "ROSBAG2_COMPILE_FLAGS before adding rcutils: ${ROSBAG2_COMPILE_FLAGS}")
set(ROSBAG2_COMPILE_FLAGS "${ROSBAG2_COMPILE_FLAGS};RCUTILS_BUILDING_DLL")
message(STATUS "ROSBAG2_COMPILE_FLAGS after adding rcutils: ${ROSBAG2_COMPILE_FLAGS}")

