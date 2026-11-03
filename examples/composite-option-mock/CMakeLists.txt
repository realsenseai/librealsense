# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.10)

project(RealsenseExamplesCompositeOptionMock)

# Test scaffolding proving the atomicity + round-trip behavior of the generic composite-option
# mechanism against a fake in-memory composite_option_interface standing in for real HKR/D500
# hardware, driven through the real public entry points - see rs-composite-option-mock.cpp. One
# parameterized binary covers all three DPP structs (Decimation/Temporal/ICR); not install()-ed,
# this is a unit test, not an SDK example end users need.
add_executable(rs-composite-option-mock
    rs-composite-option-mock.cpp
)
set_property(TARGET rs-composite-option-mock PROPERTY CXX_STANDARD 14)
target_link_libraries(rs-composite-option-mock PRIVATE ${LRS_TARGET})
set_target_properties (rs-composite-option-mock PROPERTIES FOLDER Examples)
