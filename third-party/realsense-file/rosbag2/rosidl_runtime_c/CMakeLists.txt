cmake_minimum_required(VERSION 3.5)

project(rosidl_runtime_c C)

# Default to C11
if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 11)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake_ros REQUIRED)
find_package(rcutils REQUIRED)
find_package(rosidl_typesupport_interface REQUIRED)

add_library(${PROJECT_NAME}
  "src/message_type_support.c"
  "src/primitives_sequence_functions.c"
  "src/sequence_bound.c"
  "src/service_type_support.c"
  "src/string_functions.c"
  "src/u16string_functions.c"
)
target_include_directories(${PROJECT_NAME} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include>")
ament_target_dependencies(${PROJECT_NAME}
  "rcutils"
  "rosidl_typesupport_interface")
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set_target_properties(${PROJECT_NAME} PROPERTIES
    COMPILE_OPTIONS -Wall -Wextra -Wpedantic)
endif()
if(WIN32)
  target_compile_definitions(${PROJECT_NAME}
    PRIVATE "ROSIDL_GENERATOR_C_BUILDING_DLL")
endif()

if(BUILD_TESTING AND NOT RCUTILS_DISABLE_FAULT_INJECTION)
  target_compile_definitions(${PROJECT_NAME} PUBLIC RCUTILS_ENABLE_FAULT_INJECTION)
endif()

ament_export_dependencies(rcutils rosidl_typesupport_interface)
ament_export_include_directories(include)
ament_export_libraries(${PROJECT_NAME})
ament_export_targets(${PROJECT_NAME})

ament_index_register_resource("rosidl_runtime_packages")

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  find_package(performance_test_fixture REQUIRED)
  # Give cppcheck hints about macro definitions coming from outside this package
  get_target_property(ament_cmake_cppcheck_ADDITIONAL_INCLUDE_DIRS performance_test_fixture::performance_test_fixture INTERFACE_INCLUDE_DIRECTORIES)
  ament_lint_auto_find_test_dependencies()

  # For gtest
  enable_language(CXX)
  # Default to C++14
  if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 14)
  endif()

  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(test_message_type_support test/test_message_type_support.cpp)
  if(TARGET test_message_type_support)
    target_include_directories(test_message_type_support PUBLIC include)
    target_link_libraries(test_message_type_support ${PROJECT_NAME})
  endif()

  ament_add_gtest(test_primitives_sequence_functions test/test_primitives_sequence_functions.cpp)
  if(TARGET test_primitives_sequence_functions)
    target_include_directories(test_primitives_sequence_functions PUBLIC include)
    target_link_libraries(test_primitives_sequence_functions ${PROJECT_NAME})
  endif()

  ament_add_gtest(test_sequence_bound test/test_sequence_bound.cpp)
  if(TARGET test_sequence_bound)
    ament_target_dependencies(test_sequence_bound
      "rosidl_typesupport_interface")
    target_include_directories(test_sequence_bound PUBLIC include)
    target_link_libraries(test_sequence_bound ${PROJECT_NAME})
  endif()

  ament_add_gtest(test_service_type_support test/test_service_type_support.cpp)
  if(TARGET test_service_type_support)
    target_include_directories(test_service_type_support PUBLIC include)
    target_link_libraries(test_service_type_support ${PROJECT_NAME})
  endif()

  ament_add_gtest(test_string_functions test/test_string_functions.cpp)
  if(TARGET test_string_functions)
    target_include_directories(test_string_functions PUBLIC include)
    target_link_libraries(test_string_functions ${PROJECT_NAME})
    target_compile_definitions(test_string_functions PUBLIC RCUTILS_ENABLE_FAULT_INJECTION)
  endif()

  ament_add_gtest(test_u16string_functions test/test_u16string_functions.cpp)
  if(TARGET test_u16string_functions)
    target_include_directories(test_u16string_functions PUBLIC include)
    target_link_libraries(test_u16string_functions ${PROJECT_NAME})
    target_compile_definitions(test_u16string_functions PUBLIC RCUTILS_ENABLE_FAULT_INJECTION)
  endif()

  add_performance_test(benchmark_string_conversion test/benchmark/benchmark_string_conversion.cpp)
  if(TARGET benchmark_string_conversion)
    target_include_directories(benchmark_string_conversion PUBLIC include)
    target_link_libraries(benchmark_string_conversion ${PROJECT_NAME})
  endif()
endif()

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

ament_package()
