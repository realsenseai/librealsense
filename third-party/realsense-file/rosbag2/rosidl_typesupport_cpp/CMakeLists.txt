cmake_minimum_required(VERSION 3.5)

project(rosidl_typesupport_cpp)

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 14)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake_ros REQUIRED)
find_package(rcpputils REQUIRED)
find_package(rosidl_runtime_c REQUIRED)
find_package(rosidl_typesupport_c REQUIRED)

ament_export_dependencies(rcpputils)
ament_export_dependencies(rosidl_runtime_c)
ament_export_dependencies(rosidl_runtime_cpp)
ament_export_dependencies(rosidl_typesupport_interface)
ament_export_dependencies(rosidl_typesupport_c)

ament_export_include_directories(include)

ament_python_install_package(${PROJECT_NAME})

add_library(${PROJECT_NAME}
  src/identifier.cpp
  src/message_type_support_dispatch.cpp
  src/service_type_support_dispatch.cpp
  src/type_support_dispatch.cpp)
if(WIN32)
  target_compile_definitions(${PROJECT_NAME}
    PRIVATE "ROSIDL_TYPESUPPORT_CPP_BUILDING_DLL")
endif()
target_include_directories(${PROJECT_NAME} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include>")

ament_target_dependencies(${PROJECT_NAME}
  "rcpputils"
  "rosidl_runtime_c"
  "rosidl_typesupport_c"
)
ament_export_libraries(${PROJECT_NAME})
ament_export_targets(${PROJECT_NAME})

ament_index_register_resource("rosidl_runtime_packages")

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()

  find_package(performance_test_fixture REQUIRED)
  # Give cppcheck hints about macro definitions coming from outside this package
  get_target_property(ament_cmake_cppcheck_ADDITIONAL_INCLUDE_DIRS
    performance_test_fixture::performance_test_fixture INTERFACE_INCLUDE_DIRECTORIES)

  set(TEST_LIB_DIR "${CMAKE_CURRENT_BINARY_DIR}/test_libs")

  add_library(rosidl_typesupport_cpp__test_type_support1
    test/test_type_support.cpp)
  target_include_directories(rosidl_typesupport_cpp__test_type_support1 PRIVATE include)
  ament_target_dependencies(rosidl_typesupport_cpp__test_type_support1
    "rosidl_runtime_c"
  )
  add_custom_command(TARGET rosidl_typesupport_cpp__test_type_support1 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    $<TARGET_FILE:rosidl_typesupport_cpp__test_type_support1>
    ${TEST_LIB_DIR})

  # This same library is added a second type with a new name for additional tests
  add_library(rosidl_typesupport_cpp__test_type_support2
    test/test_type_support.cpp)
  target_include_directories(rosidl_typesupport_cpp__test_type_support2 PRIVATE include)
  ament_target_dependencies(rosidl_typesupport_cpp__test_type_support2
    "rosidl_runtime_c"
  )
  add_custom_command(TARGET rosidl_typesupport_cpp__test_type_support2 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    $<TARGET_FILE:rosidl_typesupport_cpp__test_type_support2>
    ${TEST_LIB_DIR})

  ament_add_gtest(test_message_type_support test/test_message_type_support_dispatch.cpp
    APPEND_ENV
    "DYLD_LIBRARY_PATH=${TEST_LIB_DIR}"
    "LD_LIBRARY_PATH=${TEST_LIB_DIR}"
    "PATH=${TEST_LIB_DIR}")
  if(TARGET test_message_type_support)
    target_link_libraries(test_message_type_support
      ${PROJECT_NAME}
    )
    target_compile_definitions(test_message_type_support PUBLIC RCUTILS_ENABLE_FAULT_INJECTION)
  endif()

  ament_add_gtest(test_service_type_support test/test_service_type_support_dispatch.cpp
    APPEND_ENV
    "DYLD_LIBRARY_PATH=${TEST_LIB_DIR}"
    "LD_LIBRARY_PATH=${TEST_LIB_DIR}"
    "PATH=${TEST_LIB_DIR}")
  if(TARGET test_service_type_support)
    target_link_libraries(test_service_type_support
      ${PROJECT_NAME}
    )
    target_compile_definitions(test_service_type_support PUBLIC RCUTILS_ENABLE_FAULT_INJECTION)
  endif()

  # Test type_support_dispatch throws runtime error when trying to load this "library"
  file(GENERATE
    OUTPUT
    "${TEST_LIB_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}rosidl_typesupport_cpp__test_type_support3${CMAKE_SHARED_LIBRARY_SUFFIX}"
    CONTENT "I'm not a shared library, why would you treat me like one?")

  add_performance_test(benchmark_type_support_dispatch test/benchmark/benchmark_type_support_dispatch.cpp
    APPEND_ENV
    "DYLD_LIBRARY_PATH=${TEST_LIB_DIR}"
    "LD_LIBRARY_PATH=${TEST_LIB_DIR}"
    "PATH=${TEST_LIB_DIR}")
  if(TARGET benchmark_type_support_dispatch)
    target_link_libraries(benchmark_type_support_dispatch ${PROJECT_NAME})
    ament_target_dependencies(benchmark_type_support_dispatch rcpputils)
    target_include_directories(benchmark_type_support_dispatch PRIVATE include)
  endif()
endif()

if(BUILD_SHARED_LIBS)
  set(${PROJECT_NAME}_LIBRARY_TYPE "SHARED")
else()
  set(${PROJECT_NAME}_LIBRARY_TYPE "STATIC")
endif()

ament_package(
  CONFIG_EXTRAS "rosidl_typesupport_cpp-extras.cmake.in"
)

install(
  PROGRAMS bin/rosidl_typesupport_cpp
  DESTINATION lib/rosidl_typesupport_cpp
)
install(
  DIRECTORY cmake resource
  DESTINATION share/${PROJECT_NAME}
)
install(
  DIRECTORY include/
  DESTINATION include
)
install(
  TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
