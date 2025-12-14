# Copyright 2016-2018 Open Source Robotics Foundation, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

if(NOT rosidl_generator_cpp_FOUND)
  message(FATAL_ERROR
    "'rosidl_generator_cpp' not found when executing "
    "'rosidl_typesupport_cpp' extension.")
endif()

set(_output_path
  "${CMAKE_CURRENT_BINARY_DIR}/rosidl_typesupport_cpp/${PROJECT_NAME}")
set(_generated_sources "")
foreach(_abs_idl_file ${rosidl_generate_interfaces_ABS_IDL_FILES})
  get_filename_component(_parent_folder "${_abs_idl_file}" DIRECTORY)
  get_filename_component(_parent_folder "${_parent_folder}" NAME)
  get_filename_component(_idl_name "${_abs_idl_file}" NAME_WE)
  string_camel_case_to_lower_case_underscore("${_idl_name}" _header_name)
  list(APPEND _generated_sources
    "${_output_path}/${_parent_folder}/${_header_name}__type_support.cpp"
  )
endforeach()

set(_dependency_files "")
set(_dependencies "")
foreach(_pkg_name ${rosidl_generate_interfaces_DEPENDENCY_PACKAGE_NAMES})
  foreach(_idl_file ${${_pkg_name}_IDL_FILES})
    set(_abs_idl_file "${${_pkg_name}_DIR}/../${_idl_file}")
    normalize_path(_abs_idl_file "${_abs_idl_file}")
    list(APPEND _dependency_files "${_abs_idl_file}")
    list(APPEND _dependencies "${_pkg_name}:${_abs_idl_file}")
  endforeach()
endforeach()

set(target_dependencies
  "${rosidl_typesupport_cpp_BIN}"
  ${rosidl_typesupport_cpp_GENERATOR_FILES}
  "${rosidl_typesupport_cpp_TEMPLATE_DIR}/action__type_support.cpp.em"
  "${rosidl_typesupport_cpp_TEMPLATE_DIR}/idl__type_support.cpp.em"
  "${rosidl_typesupport_cpp_TEMPLATE_DIR}/msg__type_support.cpp.em"
  "${rosidl_typesupport_cpp_TEMPLATE_DIR}/srv__type_support.cpp.em"
  ${rosidl_generate_interfaces_ABS_IDL_FILES}
  ${_dependency_files})
foreach(dep ${target_dependencies})
  if(NOT EXISTS "${dep}")
    message(FATAL_ERROR "Target dependency '${dep}' does not exist")
  endif()
endforeach()

set(generator_arguments_file "${CMAKE_CURRENT_BINARY_DIR}/rosidl_typesupport_cpp__arguments.json")
rosidl_write_generator_arguments(
  "${generator_arguments_file}"
  PACKAGE_NAME "${PROJECT_NAME}"
  IDL_TUPLES "${rosidl_generate_interfaces_IDL_TUPLES}"
  ROS_INTERFACE_DEPENDENCIES "${_dependencies}"
  OUTPUT_DIR "${_output_path}"
  TEMPLATE_DIR "${rosidl_typesupport_cpp_TEMPLATE_DIR}"
  TARGET_DEPENDENCIES ${target_dependencies}
)

get_used_typesupports(typesupports "rosidl_typesupport_cpp")
add_custom_command(
  OUTPUT ${_generated_sources}
  COMMAND ${PYTHON_EXECUTABLE} ${rosidl_typesupport_cpp_BIN}
  --generator-arguments-file "${generator_arguments_file}"
  --typesupports ${typesupports}
  DEPENDS ${target_dependencies}
  COMMENT "Generating C++ type support dispatch for ROS interfaces"
  VERBATIM
)

set(_target_suffix "__rosidl_typesupport_cpp")

add_library(${rosidl_generate_interfaces_TARGET}${_target_suffix} ${rosidl_typesupport_cpp_LIBRARY_TYPE} ${_generated_sources})
if(rosidl_generate_interfaces_LIBRARY_NAME)
  set_target_properties(${rosidl_generate_interfaces_TARGET}${_target_suffix}
    PROPERTIES OUTPUT_NAME "${rosidl_generate_interfaces_LIBRARY_NAME}${_target_suffix}")
endif()
if(WIN32)
  target_compile_definitions(${rosidl_generate_interfaces_TARGET}${_target_suffix}
    PRIVATE "ROSIDL_TYPESUPPORT_CPP_BUILDING_DLL")
endif()
set_target_properties(${rosidl_generate_interfaces_TARGET}${_target_suffix}
  PROPERTIES CXX_STANDARD 14)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set_target_properties(${rosidl_generate_interfaces_TARGET}${_target_suffix}
    PROPERTIES COMPILE_OPTIONS -Wall -Wextra -Wpedantic)
endif()
target_include_directories(${rosidl_generate_interfaces_TARGET}${_target_suffix}
  PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/rosidl_generator_cpp>"
  "$<INSTALL_INTERFACE:include>"
)

# if only a single typesupport is used this package will directly reference it
# therefore it needs to link against the selected typesupport
if(NOT typesupports MATCHES ";")
  target_include_directories(${rosidl_generate_interfaces_TARGET}${_target_suffix}
    PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/${typesupports}>")
  target_link_libraries(${rosidl_generate_interfaces_TARGET}${_target_suffix}
    ${rosidl_generate_interfaces_TARGET}__${typesupports})
else()
  if("${rosidl_typesupport_cpp_LIBRARY_TYPE}" STREQUAL "STATIC")
    message(FATAL_ERROR "Multiple typesupports [${typesupports}] but static "
      "linking was requested")
  endif()
endif()

ament_target_dependencies(${rosidl_generate_interfaces_TARGET}${_target_suffix}
  "rosidl_runtime_c"
  "rosidl_runtime_cpp"
  "rosidl_typesupport_cpp"
  "rosidl_typesupport_interface")
foreach(_pkg_name ${rosidl_generate_interfaces_DEPENDENCY_PACKAGE_NAMES})
  ament_target_dependencies(
    ${rosidl_generate_interfaces_TARGET}${_target_suffix}
    ${_pkg_name})
  if(NOT rosidl_generate_interfaces_SKIP_INSTALL)
    ament_export_dependencies(${_pkg_name})
  endif()
endforeach()

add_dependencies(
  ${rosidl_generate_interfaces_TARGET}
  ${rosidl_generate_interfaces_TARGET}${_target_suffix}
)
add_dependencies(
  ${rosidl_generate_interfaces_TARGET}${_target_suffix}
  ${rosidl_generate_interfaces_TARGET}__cpp
)

if(NOT rosidl_generate_interfaces_SKIP_INSTALL)
  install(
    TARGETS ${rosidl_generate_interfaces_TARGET}${_target_suffix}
    EXPORT ${rosidl_generate_interfaces_TARGET}${_target_suffix}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
  )
  ament_export_libraries(${rosidl_generate_interfaces_TARGET}${_target_suffix})
  ament_export_targets(${rosidl_generate_interfaces_TARGET}${_target_suffix})
endif()

if(BUILD_TESTING AND rosidl_generate_interfaces_ADD_LINTER_TESTS)
  if(NOT _generated_sources STREQUAL "")
    find_package(ament_cmake_cppcheck REQUIRED)
    ament_cppcheck(
      TESTNAME "cppcheck_rosidl_typesupport_cpp"
      "${_output_path}")

    find_package(ament_cmake_cpplint REQUIRED)
    get_filename_component(_cpplint_root "${_output_path}" DIRECTORY)
    ament_cpplint(
      TESTNAME "cpplint_rosidl_typesupport_cpp"
      # the generated code might contain longer lines for templated types
      MAX_LINE_LENGTH 999
      ROOT "${_cpplint_root}"
      "${_output_path}")

    find_package(ament_cmake_uncrustify REQUIRED)
    ament_uncrustify(
      TESTNAME "uncrustify_rosidl_typesupport_cpp"
      # the generated code might contain longer lines for templated types
      # a value of zero tells uncrustify to ignore line length
      MAX_LINE_LENGTH 0
      "${_output_path}")
  endif()
endif()
