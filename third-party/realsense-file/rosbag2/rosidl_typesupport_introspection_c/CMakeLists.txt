cmake_minimum_required(VERSION 3.5)

project(rosidl_typesupport_introspection_c)

# Default to C11
if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 11)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake_python REQUIRED)
find_package(ament_cmake_ros REQUIRED)

ament_export_dependencies(rosidl_cmake)
ament_export_dependencies(rosidl_runtime_c)
# The reason the impl folder is exported is that it contains the implementation
# for the get_*_type_support_handle functions and defines the opensplice
# specific version of these functions.

ament_export_include_directories(include)

ament_python_install_package(${PROJECT_NAME})

add_library(${PROJECT_NAME} src/identifier.c)
if(WIN32)
  target_compile_definitions(${PROJECT_NAME}
    PRIVATE "ROSIDL_TYPESUPPORT_INTROSPECTION_C_BUILDING_DLL")
endif()
target_include_directories(${PROJECT_NAME} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include>")
ament_export_libraries(${PROJECT_NAME})
ament_export_targets(${PROJECT_NAME})

ament_index_register_resource("rosidl_typesupport_c")
ament_index_register_resource("rosidl_runtime_packages")

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

if(BUILD_SHARED_LIBS)
  set(${PROJECT_NAME}_LIBRARY_TYPE "SHARED")
else()
  set(${PROJECT_NAME}_LIBRARY_TYPE "STATIC")
endif()

ament_package(
  CONFIG_EXTRAS "rosidl_typesupport_introspection_c-extras.cmake.in"
)

install(
  PROGRAMS bin/rosidl_typesupport_introspection_c
  DESTINATION lib/rosidl_typesupport_introspection_c
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
