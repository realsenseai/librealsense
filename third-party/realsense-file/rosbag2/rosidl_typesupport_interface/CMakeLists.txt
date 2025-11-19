cmake_minimum_required(VERSION 3.5)

project(rosidl_typesupport_interface NONE)

find_package(ament_cmake REQUIRED)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE
  "$<INSTALL_INTERFACE:include>")

ament_export_include_directories(include)
ament_export_targets(${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()

  # For gtest
  enable_language(C CXX)
  # Default to C11
  if(NOT CMAKE_C_STANDARD)
    set(CMAKE_C_STANDARD 11)
  endif()
  # Default to C++14
  if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 14)
  endif()

  find_package(ament_cmake_gtest REQUIRED)

  if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
  endif()

  ament_add_gtest(test_macros test/test_macros.cpp)
  if(TARGET test_macros)
    target_include_directories(test_macros PUBLIC include)
  endif()

endif()

ament_package()

install(
  DIRECTORY include/
  DESTINATION include
)
install(
  TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}
)
