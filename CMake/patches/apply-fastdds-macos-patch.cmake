if(NOT EXISTS "${FASTDDS_PATCH}")
    message(FATAL_ERROR "FastDDS macOS SO_REUSEPORT patch is missing: ${FASTDDS_PATCH}")
endif()

set(_fastdds_udp4 "${FASTDDS_SOURCE_DIR}/src/cpp/rtps/transport/UDPv4Transport.cpp")
set(_fastdds_udp6 "${FASTDDS_SOURCE_DIR}/src/cpp/rtps/transport/UDPv6Transport.cpp")
if(NOT EXISTS "${_fastdds_udp4}" OR NOT EXISTS "${_fastdds_udp6}")
    message(FATAL_ERROR "FastDDS source tree is incomplete: ${FASTDDS_SOURCE_DIR}")
endif()

file(READ "${_fastdds_udp4}" _fastdds_udp4_contents)
file(READ "${_fastdds_udp6}" _fastdds_udp6_contents)
string(FIND "${_fastdds_udp4_contents}" "defined(__QNX__) || defined(__APPLE__)" _fastdds_udp4_marker)
string(FIND "${_fastdds_udp6_contents}" "defined(__QNX__) || defined(__APPLE__)" _fastdds_udp6_marker)
if(NOT _fastdds_udp4_marker EQUAL -1 AND NOT _fastdds_udp6_marker EQUAL -1)
    message(STATUS "FastDDS macOS SO_REUSEPORT patch already applied")
    return()
endif()

execute_process(
    COMMAND patch -p1 --forward --dry-run -i "${FASTDDS_PATCH}"
    WORKING_DIRECTORY "${FASTDDS_SOURCE_DIR}"
    RESULT_VARIABLE _fastdds_patch_dry_run_rc
    OUTPUT_VARIABLE _fastdds_patch_dry_run_out
    ERROR_VARIABLE _fastdds_patch_dry_run_err)
if(NOT _fastdds_patch_dry_run_rc EQUAL 0)
    message(FATAL_ERROR
        "FastDDS macOS SO_REUSEPORT patch dry-run failed (rc=${_fastdds_patch_dry_run_rc}): "
        "${_fastdds_patch_dry_run_err}${_fastdds_patch_dry_run_out}")
endif()

execute_process(
    COMMAND patch -p1 --forward -i "${FASTDDS_PATCH}"
    WORKING_DIRECTORY "${FASTDDS_SOURCE_DIR}"
    RESULT_VARIABLE _fastdds_patch_rc
    OUTPUT_VARIABLE _fastdds_patch_out
    ERROR_VARIABLE _fastdds_patch_err)
if(NOT _fastdds_patch_rc EQUAL 0)
    message(FATAL_ERROR
        "FastDDS macOS SO_REUSEPORT patch failed (rc=${_fastdds_patch_rc}): "
        "${_fastdds_patch_err}${_fastdds_patch_out}")
endif()
message(STATUS "FastDDS macOS SO_REUSEPORT patch applied")
