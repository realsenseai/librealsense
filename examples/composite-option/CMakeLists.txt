# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.10)

project(RealsenseExamplesCompositeOption)

# Reference sample: exercises the FULL composite-option surface (enumeration, get,
# read-modify-write set, get_range, metadata) against every composite option found on every
# embedded filter of every connected device - no fake/mock transport. See rs-composite-option.cpp.
add_executable(rs-composite-option
    rs-composite-option.cpp
)
set_property(TARGET rs-composite-option PROPERTY CXX_STANDARD 14)
target_link_libraries(rs-composite-option PRIVATE ${LRS_TARGET})
set_target_properties (rs-composite-option PROPERTIES FOLDER Examples)
install(TARGETS rs-composite-option RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Sweeps every connected device's depth sensor(s), enumerates whichever
# composite options each one supports, and prints the current value of each - against REAL
# connected devices (no fake transport). See rs-icr-control.cpp.
add_executable(rs-icr-control
    rs-icr-control.cpp
)
set_property(TARGET rs-icr-control PROPERTY CXX_STANDARD 14)
target_link_libraries(rs-icr-control PRIVATE ${LRS_TARGET})
set_target_properties (rs-icr-control PROPERTIES FOLDER Examples)
install(TARGETS rs-icr-control RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# The C99 raw-API counterpart of rs-composite-option.cpp lives under examples/C/composite-ctl
# (see rs-composite-ctl.c there) - Examples/C, not Examples, matching where every other pure-C
# sample (rs-infrared, rs-depth, ...) lives.
